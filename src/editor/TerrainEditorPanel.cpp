#include "editor/TerrainEditorPanel.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <memory>
#include <utility>

#include <imgui.h>
#include <loguru.hpp>

#include "abstract/VideoDevice.h"
#include "editor/EditorCommandHistory.h"
#include "editor/commands/PaintBrushCommand.h"
#include "editor/commands/SculptBrushCommand.h"
#include "terrain/TerrainData.h"
#include "terrain/TerrainMaterial.h"
#include "terrain/TerrainNormalMap.h"
#include "terrain/TerrainRenderer.h"

namespace {
// Max raise/lower speed in metres per second at strength = 1.
constexpr float kBrushRate = 10.f;

// Predefined swatch colours for the four paint layers (RGBA float).
// Used as a visual hint when the actual albedo texture colour is unavailable.
constexpr ImVec4 kLayerSwatchColors[terrain::kMaxTerrainLayers] = {
    {0.30f, 0.65f, 0.25f, 1.f},   // layer 0 — green  (grass)
    {0.50f, 0.45f, 0.35f, 1.f},   // layer 1 — brown  (rock/earth)
    {0.80f, 0.75f, 0.55f, 1.f},   // layer 2 — tan    (sand)
    {0.90f, 0.92f, 0.95f, 1.f},   // layer 3 — white  (snow)
};
}  // namespace

namespace editor {

void TerrainEditorPanel::SetContext(terrain::TerrainData* data,
                                    terrain::TerrainMaterial* material,
                                    terrain::TerrainNormalMap* normal_map,
                                    abstract::VideoDevice* video,
                                    EditorCommandHistory* history) {
  data_       = data;
  material_   = material;
  normal_map_ = normal_map;
  video_      = video;
  history_    = history;
  stroke_active_       = false;
  paint_stroke_active_ = false;

  if (material_ && active_layer_ >= material_->GetLayerCount())
    active_layer_ = 0;
}

// ---- Render -----------------------------------------------------------------

void TerrainEditorPanel::Render() {
  if (!data_) {
    ImGui::TextDisabled("No terrain in scene.");
    return;
  }

  if (ImGui::BeginTabBar("##terrain_tabs")) {
    if (ImGui::BeginTabItem("Sculpt")) {
      active_tab_ = ActiveTab::kSculpt;
      RenderSculptTab();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Paint")) {
      active_tab_ = ActiveTab::kPaint;
      RenderPaintTab();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
}

void TerrainEditorPanel::RenderSculptTab() {
  ImGui::TextUnformatted("Tool");
  ImGui::SameLine();
  const auto tool_button = [&](const char* label, Tool t) {
    if (tool_ == t) {
      ImGui::PushStyleColor(ImGuiCol_Button,
                            ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    }
    if (ImGui::Button(label)) tool_ = t;
    if (tool_ == t) ImGui::PopStyleColor();
    ImGui::SameLine();
  };
  tool_button("Raise##tool",   Tool::kRaise);
  tool_button("Lower##tool",   Tool::kLower);
  tool_button("Smooth##tool",  Tool::kSmooth);
  tool_button("Flatten##tool", Tool::kFlatten);
  ImGui::NewLine();

  ImGui::SliderFloat("Radius (m)",  &radius_,   0.5f,  200.f, "%.1f");
  ImGui::SliderFloat("Strength",    &strength_,  0.01f, 1.f,   "%.2f");

  ImGui::TextUnformatted("Falloff");
  ImGui::SameLine();
  if (ImGui::RadioButton("Linear##falloff", falloff_ == Falloff::kLinear))
    falloff_ = Falloff::kLinear;
  ImGui::SameLine();
  if (ImGui::RadioButton("Smooth##falloff", falloff_ == Falloff::kSmooth))
    falloff_ = Falloff::kSmooth;

  ImGui::Spacing();
  ImGui::TextDisabled("Left-drag in viewport to sculpt.");
}

void TerrainEditorPanel::RenderPaintTab() {
  if (!material_ || material_->GetLayerCount() == 0) {
    ImGui::TextDisabled("No terrain material layers defined.");
    return;
  }

  ImGui::TextUnformatted("Layer");
  ImGui::SameLine();

  const int layer_count = material_->GetLayerCount();
  for (int i = 0; i < layer_count; ++i) {
    const ImVec4& col = kLayerSwatchColors[i];
    if (active_layer_ == i) {
      ImGui::PushStyleColor(ImGuiCol_Button,         col);
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  col);
      ImGui::PushStyleColor(ImGuiCol_ButtonActive,   col);
      ImGui::PushStyleColor(ImGuiCol_Border,
                            ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));
      const float prev_border = ImGui::GetStyle().FrameBorderSize;
      ImGui::GetStyle().FrameBorderSize = 2.f;
      char label[32];
      snprintf(label, sizeof(label), "%d##layer", i);
      ImGui::Button(label, {28.f, 28.f});  // already active; click is a no-op
      ImGui::GetStyle().FrameBorderSize = prev_border;
      ImGui::PopStyleColor(4);
    } else {
      ImGui::PushStyleColor(ImGuiCol_Button,        col);
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col);
      ImGui::PushStyleColor(ImGuiCol_ButtonActive,  col);
      char label[32];
      snprintf(label, sizeof(label), "%d##layer", i);
      if (ImGui::Button(label, {28.f, 28.f})) active_layer_ = i;
      ImGui::PopStyleColor(3);
    }
    if (i < layer_count - 1) ImGui::SameLine();
  }
  ImGui::NewLine();

  ImGui::SliderFloat("Opacity",    &paint_opacity_, 0.f,   1.f,   "%.2f");
  ImGui::SliderFloat("Radius (m)", &radius_,        0.5f,  200.f, "%.1f");

  ImGui::TextUnformatted("Falloff");
  ImGui::SameLine();
  if (ImGui::RadioButton("Linear##pfalloff", falloff_ == Falloff::kLinear))
    falloff_ = Falloff::kLinear;
  ImGui::SameLine();
  if (ImGui::RadioButton("Smooth##pfalloff", falloff_ == Falloff::kSmooth))
    falloff_ = Falloff::kSmooth;

  ImGui::Spacing();
  ImGui::TextDisabled("Left-drag in viewport to paint.");
}

// ---- Brush dispatch ---------------------------------------------------------

void TerrainEditorPanel::OnBrushAt(float wx, float wz, bool first_touch,
                                    float dt) {
  if (active_tab_ == ActiveTab::kPaint) {
    OnPaintAt(wx, wz, first_touch);
    return;
  }
  OnSculptAt(wx, wz, first_touch, dt);
}

void TerrainEditorPanel::OnBrushEnd() {
  if (active_tab_ == ActiveTab::kPaint) {
    OnPaintEnd();
    return;
  }
  OnSculptEnd();
}

// ---- Shared falloff ---------------------------------------------------------

float TerrainEditorPanel::ComputeFalloff(float t) const {
  if (falloff_ == Falloff::kLinear) return 1.f - t;
  const float s = 1.f - t;
  return s * s * (3.f - 2.f * s);
}

// ---- Sculpt implementation --------------------------------------------------

void TerrainEditorPanel::EnsureRegionCovers(int nx0, int nz0, int nx1, int nz1) {
  if (!stroke_active_) return;

  const int new_x0 = std::min(stroke_x0_, nx0);
  const int new_z0 = std::min(stroke_z0_, nz0);
  const int new_x1 = std::max(stroke_x1_, nx1);
  const int new_z1 = std::max(stroke_z1_, nz1);

  if (new_x0 == stroke_x0_ && new_z0 == stroke_z0_ &&
      new_x1 == stroke_x1_ && new_z1 == stroke_z1_) {
    return;
  }

  const int new_w = new_x1 - new_x0;
  const int new_h = new_z1 - new_z0;
  std::vector<uint16_t> new_pre(static_cast<std::size_t>(new_w) * new_h);

  for (int z = stroke_z0_; z < stroke_z1_; ++z) {
    for (int x = stroke_x0_; x < stroke_x1_; ++x) {
      const int si = (z - stroke_z0_) * (stroke_x1_ - stroke_x0_) + (x - stroke_x0_);
      const int di = (z - new_z0) * new_w + (x - new_x0);
      new_pre[static_cast<std::size_t>(di)] =
          stroke_pre_snapshot_[static_cast<std::size_t>(si)];
    }
  }

  for (int z = new_z0; z < new_z1; ++z) {
    for (int x = new_x0; x < new_x1; ++x) {
      const bool in_old = (x >= stroke_x0_ && x < stroke_x1_ &&
                           z >= stroke_z0_ && z < stroke_z1_);
      if (in_old) continue;
      const int di = (z - new_z0) * new_w + (x - new_x0);
      new_pre[static_cast<std::size_t>(di)] = data_->GetSample(x, z);
    }
  }

  stroke_pre_snapshot_ = std::move(new_pre);
  stroke_x0_ = new_x0;
  stroke_z0_ = new_z0;
  stroke_x1_ = new_x1;
  stroke_z1_ = new_z1;
}

void TerrainEditorPanel::ApplyBrushFootprint(float cx_world, float cz_world,
                                              float dt) {
  const float mpt          = data_->GetMetersPerTexel();
  const float height_range = data_->GetMaxHeight() - data_->GetMinHeight();
  if (height_range <= 0.f || radius_ <= 0.f) return;

  const int center_tx = static_cast<int>(cx_world / mpt);
  const int center_tz = static_cast<int>(cz_world / mpt);
  const int r_texels  = static_cast<int>(std::ceil(radius_ / mpt)) + 1;

  const int tw = data_->GetTexelWidth();
  const int th = data_->GetTexelHeight();

  const int x0 = std::max(0, center_tx - r_texels);
  const int z0 = std::max(0, center_tz - r_texels);
  const int x1 = std::min(tw, center_tx + r_texels + 1);
  const int z1 = std::min(th, center_tz + r_texels + 1);
  if (x1 <= x0 || z1 <= z0) return;

  for (int z = z0; z < z1; ++z) {
    for (int x = x0; x < x1; ++x) {
      const float tx_world = (static_cast<float>(x) + 0.5f) * mpt;
      const float tz_world = (static_cast<float>(z) + 0.5f) * mpt;
      const float dx = tx_world - cx_world;
      const float dz = tz_world - cz_world;
      const float dist = std::sqrt(dx * dx + dz * dz);
      const float t = dist / radius_;
      if (t >= 1.f) continue;

      const float w = ComputeFalloff(t);
      const uint16_t sample = data_->GetSample(x, z);
      float h = data_->GetMinHeight() +
                static_cast<float>(sample) / 65535.f * height_range;

      switch (tool_) {
        case Tool::kRaise:
          h += strength_ * w * dt * kBrushRate;
          break;
        case Tool::kLower:
          h -= strength_ * w * dt * kBrushRate;
          break;
        case Tool::kSmooth: {
          float sum   = 0.f;
          int   count = 0;
          for (int nz = z - 1; nz <= z + 1; ++nz) {
            for (int nx = x - 1; nx <= x + 1; ++nx) {
              if (nx < 0 || nx >= tw || nz < 0 || nz >= th) continue;
              const uint16_t ns = data_->GetSample(nx, nz);
              sum += data_->GetMinHeight() +
                     static_cast<float>(ns) / 65535.f * height_range;
              ++count;
            }
          }
          const float avg = sum / static_cast<float>(count);
          h += (avg - h) * strength_ * w;
          break;
        }
        case Tool::kFlatten:
          h += (stroke_flatten_h_ - h) * strength_ * w;
          break;
      }

      h = std::clamp(h, data_->GetMinHeight(), data_->GetMaxHeight());
      data_->SetSample(x, z, data_->HeightToSample(h));
    }
  }

  if (terrain::TerrainRenderer::IsInstanced())
    terrain::TerrainRenderer::Instance().UpdateHeightmapTile(
        x0, z0, x1 - x0, z1 - z0, *data_);

  if (normal_map_ && video_)
    normal_map_->UploadTile(video_, x0, z0, x1 - x0, z1 - z0);
}

void TerrainEditorPanel::OnSculptAt(float wx, float wz, bool first_touch,
                                     float dt) {
  if (!data_ || !normal_map_ || !video_) return;

  const float mpt     = data_->GetMetersPerTexel();
  const int r_texels  = static_cast<int>(std::ceil(radius_ / mpt)) + 1;
  const int center_tx = static_cast<int>(wx / mpt);
  const int center_tz = static_cast<int>(wz / mpt);

  const int bx0 = std::max(0, center_tx - r_texels);
  const int bz0 = std::max(0, center_tz - r_texels);
  const int bx1 = std::min(data_->GetTexelWidth(),  center_tx + r_texels + 1);
  const int bz1 = std::min(data_->GetTexelHeight(), center_tz + r_texels + 1);

  if (first_touch) {
    stroke_flatten_h_  = data_->GetHeight(wx, wz);
    stroke_x0_         = bx0;
    stroke_z0_         = bz0;
    stroke_x1_         = bx1;
    stroke_z1_         = bz1;
    stroke_active_     = true;

    const int sw = bx1 - bx0;
    const int sh = bz1 - bz0;
    stroke_pre_snapshot_.resize(static_cast<std::size_t>(sw) * sh);
    for (int z = bz0; z < bz1; ++z)
      for (int x = bx0; x < bx1; ++x)
        stroke_pre_snapshot_[static_cast<std::size_t>((z - bz0) * sw + (x - bx0))] =
            data_->GetSample(x, z);
  } else {
    EnsureRegionCovers(bx0, bz0, bx1, bz1);
  }

  ApplyBrushFootprint(wx, wz, dt);
}

void TerrainEditorPanel::OnSculptEnd() {
  if (!stroke_active_ || !data_ || !history_) {
    stroke_active_ = false;
    return;
  }

  const int sw = stroke_x1_ - stroke_x0_;
  const int sh = stroke_z1_ - stroke_z0_;

  std::vector<uint16_t> post(static_cast<std::size_t>(sw) * sh);
  for (int z = stroke_z0_; z < stroke_z1_; ++z)
    for (int x = stroke_x0_; x < stroke_x1_; ++x)
      post[static_cast<std::size_t>((z - stroke_z0_) * sw + (x - stroke_x0_))] =
          data_->GetSample(x, z);

  if (post != stroke_pre_snapshot_) {
    history_->Push(std::make_unique<SculptBrushCommand>(
        data_, normal_map_, video_,
        stroke_x0_, stroke_z0_, sw, sh,
        std::move(stroke_pre_snapshot_),
        std::move(post)));
  }

  stroke_active_ = false;
  LOG_F(9, "TerrainEditorPanel::OnSculptEnd — stroke region [%d,%d)+[%d,%d]",
        stroke_x0_, stroke_z0_, sw, sh);
}

// ---- Paint implementation ---------------------------------------------------

void TerrainEditorPanel::EnsurePaintRegionCovers(int nx0, int nz0,
                                                  int nx1, int nz1) {
  if (!paint_stroke_active_) return;

  const int new_x0 = std::min(paint_x0_, nx0);
  const int new_z0 = std::min(paint_z0_, nz0);
  const int new_x1 = std::max(paint_x1_, nx1);
  const int new_z1 = std::max(paint_z1_, nz1);

  if (new_x0 == paint_x0_ && new_z0 == paint_z0_ &&
      new_x1 == paint_x1_ && new_z1 == paint_z1_) {
    return;
  }

  const int new_w = new_x1 - new_x0;
  const int new_h = new_z1 - new_z0;
  const int old_w = paint_x1_ - paint_x0_;
  std::vector<uint8_t> new_pre(static_cast<std::size_t>(new_w) * new_h * 4);

  for (int z = paint_z0_; z < paint_z1_; ++z) {
    for (int x = paint_x0_; x < paint_x1_; ++x) {
      const int si = ((z - paint_z0_) * old_w + (x - paint_x0_)) * 4;
      const int di = ((z - new_z0) * new_w + (x - new_x0)) * 4;
      for (int c = 0; c < 4; ++c)
        new_pre[static_cast<std::size_t>(di + c)] =
            paint_pre_snapshot_[static_cast<std::size_t>(si + c)];
    }
  }

  const auto& pixels = material_->GetSplatmapPixels();
  const int sw = material_->GetSplatWidth();
  for (int z = new_z0; z < new_z1; ++z) {
    for (int x = new_x0; x < new_x1; ++x) {
      const bool in_old = (x >= paint_x0_ && x < paint_x1_ &&
                           z >= paint_z0_ && z < paint_z1_);
      if (in_old) continue;
      const int di = ((z - new_z0) * new_w + (x - new_x0)) * 4;
      const int si = (z * sw + x) * 4;
      for (int c = 0; c < 4; ++c)
        new_pre[static_cast<std::size_t>(di + c)] =
            pixels[static_cast<std::size_t>(si + c)];
    }
  }

  paint_pre_snapshot_ = std::move(new_pre);
  paint_x0_ = new_x0;
  paint_z0_ = new_z0;
  paint_x1_ = new_x1;
  paint_z1_ = new_z1;
}

void TerrainEditorPanel::ApplyPaintFootprint(float cx_world, float cz_world) {
  assert(material_ && data_);
  if (radius_ <= 0.f) return;

  const float mpt = data_->GetMetersPerTexel();
  const int center_tx = static_cast<int>(cx_world / mpt);
  const int center_tz = static_cast<int>(cz_world / mpt);
  const int r_texels  = static_cast<int>(std::ceil(radius_ / mpt)) + 1;

  const int sw = material_->GetSplatWidth();
  const int sh = material_->GetSplatHeight();

  const int x0 = std::max(0, center_tx - r_texels);
  const int z0 = std::max(0, center_tz - r_texels);
  const int x1 = std::min(sw, center_tx + r_texels + 1);
  const int z1 = std::min(sh, center_tz + r_texels + 1);
  if (x1 <= x0 || z1 <= z0) return;

  const auto& pixels = material_->GetSplatmapPixels();

  for (int z = z0; z < z1; ++z) {
    for (int x = x0; x < x1; ++x) {
      const float tx_world = (static_cast<float>(x) + 0.5f) * mpt;
      const float tz_world = (static_cast<float>(z) + 0.5f) * mpt;
      const float dx = tx_world - cx_world;
      const float dz = tz_world - cz_world;
      const float dist = std::sqrt(dx * dx + dz * dz);
      const float t = dist / radius_;
      if (t >= 1.f) continue;

      const float w = ComputeFalloff(t);

      // Read current RGBA as floats in [0, 1].
      const int pidx = (z * sw + x) * 4;
      float ch[4];
      for (int c = 0; c < 4; ++c)
        ch[c] = pixels[static_cast<std::size_t>(pidx + c)] / 255.f;

      // Guard against a degenerate all-zero splatmap texel.
      const float sum = ch[0] + ch[1] + ch[2] + ch[3];
      if (sum < 1e-6f)
        ch[0] = 1.f;

      // Increase active layer channel and renormalise.
      ch[active_layer_] = std::min(1.f, ch[active_layer_] + paint_opacity_ * w);

      float other_sum = 0.f;
      for (int c = 0; c < 4; ++c)
        if (c != active_layer_) other_sum += ch[c];

      const float remaining = 1.f - ch[active_layer_];
      if (other_sum > 1e-6f) {
        const float scale = remaining / other_sum;
        for (int c = 0; c < 4; ++c)
          if (c != active_layer_) ch[c] *= scale;
      } else {
        // All other channels are zero; give everything to the active layer.
        ch[active_layer_] = 1.f;
        for (int c = 0; c < 4; ++c)
          if (c != active_layer_) ch[c] = 0.f;
      }

      for (int c = 0; c < 4; ++c)
        material_->SetSplatWeight(x, z, c, ch[c]);
    }
  }

  material_->UploadSplatTile(video_, x0, z0, x1 - x0, z1 - z0);
}

void TerrainEditorPanel::OnPaintAt(float wx, float wz, bool first_touch) {
  if (!data_ || !material_ || !video_) return;

  const float mpt    = data_->GetMetersPerTexel();
  const int r_texels = static_cast<int>(std::ceil(radius_ / mpt)) + 1;
  const int center_tx = static_cast<int>(wx / mpt);
  const int center_tz = static_cast<int>(wz / mpt);

  const int sw = material_->GetSplatWidth();
  const int sh = material_->GetSplatHeight();

  const int bx0 = std::max(0, center_tx - r_texels);
  const int bz0 = std::max(0, center_tz - r_texels);
  const int bx1 = std::min(sw, center_tx + r_texels + 1);
  const int bz1 = std::min(sh, center_tz + r_texels + 1);

  if (first_touch) {
    paint_x0_ = bx0;
    paint_z0_ = bz0;
    paint_x1_ = bx1;
    paint_z1_ = bz1;
    paint_stroke_active_ = true;

    const int bw = bx1 - bx0;
    const int bh = bz1 - bz0;
    paint_pre_snapshot_.resize(static_cast<std::size_t>(bw) * bh * 4);
    const auto& pixels = material_->GetSplatmapPixels();
    for (int z = bz0; z < bz1; ++z) {
      for (int x = bx0; x < bx1; ++x) {
        const int si = (z * sw + x) * 4;
        const int di = ((z - bz0) * bw + (x - bx0)) * 4;
        for (int c = 0; c < 4; ++c)
          paint_pre_snapshot_[static_cast<std::size_t>(di + c)] =
              pixels[static_cast<std::size_t>(si + c)];
      }
    }
  } else {
    EnsurePaintRegionCovers(bx0, bz0, bx1, bz1);
  }

  ApplyPaintFootprint(wx, wz);
}

void TerrainEditorPanel::OnPaintEnd() {
  if (!paint_stroke_active_ || !material_ || !history_) {
    paint_stroke_active_ = false;
    return;
  }

  const int pw = paint_x1_ - paint_x0_;
  const int ph = paint_z1_ - paint_z0_;
  const int sw = material_->GetSplatWidth();

  std::vector<uint8_t> post(static_cast<std::size_t>(pw) * ph * 4);
  const auto& pixels = material_->GetSplatmapPixels();
  for (int z = paint_z0_; z < paint_z1_; ++z) {
    for (int x = paint_x0_; x < paint_x1_; ++x) {
      const int si = (z * sw + x) * 4;
      const int di = ((z - paint_z0_) * pw + (x - paint_x0_)) * 4;
      for (int c = 0; c < 4; ++c)
        post[static_cast<std::size_t>(di + c)] =
            pixels[static_cast<std::size_t>(si + c)];
    }
  }

  if (post != paint_pre_snapshot_) {
    history_->Push(std::make_unique<PaintBrushCommand>(
        material_, video_,
        paint_x0_, paint_z0_, pw, ph,
        std::move(paint_pre_snapshot_),
        std::move(post)));
  }

  paint_stroke_active_ = false;
  LOG_F(9, "TerrainEditorPanel::OnPaintEnd — paint region [%d,%d)+[%d,%d]",
        paint_x0_, paint_z0_, pw, ph);
}

}  // namespace editor
