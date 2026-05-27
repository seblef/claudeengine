#include "editor/TerrainEditorPanel.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>

#include <imgui.h>
#include <loguru.hpp>
#include <nfd.h>
#include <stb_image.h>
#include <stb_image_write.h>

#include "abstract/VideoDevice.h"
#include "core/Config.h"
#include "editor/EditorCommandHistory.h"
#include "editor/commands/PaintBrushCommand.h"
#include "editor/commands/SculptBrushCommand.h"
#include "game/GameTerrain.h"
#include "renderer/FoliageRenderer.h"
#include "renderer/Renderer.h"
#include "terrain/FoliageLayer.h"
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
                                    EditorCommandHistory* history,
                                    game::GameTerrain* terrain_obj) {
  data_         = data;
  material_     = material;
  normal_map_   = normal_map;
  video_        = video;
  history_      = history;
  terrain_obj_  = terrain_obj;
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
    if (ImGui::BeginTabItem("Material")) {
      active_tab_ = ActiveTab::kMaterial;
      RenderMaterialTab();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Properties")) {
      active_tab_ = ActiveTab::kProperties;
      RenderPropertiesTab();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Import / Export")) {
      active_tab_ = ActiveTab::kImportExport;
      RenderImportExportTab();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Foliage")) {
      active_tab_ = ActiveTab::kFoliage;
      RenderFoliageTab();
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
  if (active_tab_ == ActiveTab::kFoliage) {
    if (!terrain_obj_ || !data_) return;
    terrain::FoliageLayer* layer =
        terrain_obj_->GetFoliageLayer(foliage_active_layer_);
    if (!layer) return;
    if (foliage_brush_mode_ == FoliageBrushMode::kPaint)
      layer->PaintBrush(wx, wz, foliage_radius_, foliage_strength_ * dt * 5.f, *data_);
    else
      layer->EraseBrush(wx, wz, foliage_radius_, foliage_strength_ * dt * 5.f, *data_);
    foliage_stroke_active_ = true;
    return;
  }
  OnSculptAt(wx, wz, first_touch, dt);
}

void TerrainEditorPanel::OnBrushEnd() {
  if (active_tab_ == ActiveTab::kPaint) {
    OnPaintEnd();
    return;
  }
  if (active_tab_ == ActiveTab::kFoliage) {
    if (foliage_stroke_active_ && terrain_obj_ && data_) {
      terrain::FoliageLayer* layer =
          terrain_obj_->GetFoliageLayer(foliage_active_layer_);
      if (layer) {
        layer->RebuildInstances(*data_);
        if (renderer::FoliageRenderer::Instance().IsReady())
          renderer::FoliageRenderer::Instance().RebuildDirtyLayers();
      }
    }
    foliage_stroke_active_ = false;
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

// ---- Material tab -----------------------------------------------------------

std::string TerrainEditorPanel::BrowseTexture() {
  const std::filesystem::path textures_root =
      core::Config::GetDataFolder() / "textures";
  const std::string root_str = textures_root.string();

  nfdu8char_t* out_path = nullptr;
  const nfdu8filteritem_t filter = {"Images", "png,jpg,jpeg,tga,bmp"};
  const nfdresult_t result = NFD_OpenDialogU8(&out_path, &filter, 1,
                                               root_str.c_str());
  if (result != NFD_OKAY) {
    if (result == NFD_ERROR)
      LOG_F(ERROR, "TerrainEditorPanel::BrowseTexture — NFD error");
    return {};
  }

  const std::filesystem::path abs(out_path);
  NFD_FreePathU8(out_path);

  std::error_code ec;
  const auto rel = std::filesystem::relative(abs, textures_root, ec);
  if (ec || rel.string().substr(0, 2) == "..") {
    LOG_F(WARNING, "TerrainEditorPanel::BrowseTexture — file outside "
          "data/textures/: %s", abs.string().c_str());
    return {};
  }
  return rel.string();
}

void TerrainEditorPanel::RenderMaterialTab() {
  if (!material_) {
    ImGui::TextDisabled("No terrain material.");
    return;
  }

  const int layer_count = material_->GetLayerCount();

  for (int i = 0; i < layer_count; ++i) {
    ImGui::PushID(i);
    char header[32];
    snprintf(header, sizeof(header), "Layer %d", i);
    if (ImGui::CollapsingHeader(header, ImGuiTreeNodeFlags_DefaultOpen)) {
      const terrain::TerrainMaterialLayer& layer = material_->GetLayer(i);

      // Albedo row.
      ImGui::TextUnformatted("Albedo");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(-80.f);
      char albedo_buf[512];
      snprintf(albedo_buf, sizeof(albedo_buf), "%s",
               layer.albedo_path.c_str());
      ImGui::InputText("##albedo", albedo_buf, sizeof(albedo_buf),
                       ImGuiInputTextFlags_ReadOnly);
      ImGui::SameLine();
      if (ImGui::Button("Browse##albedo")) {
        const std::string path = BrowseTexture();
        if (!path.empty())
          material_->SetLayerAlbedo(i, path, video_);
      }

      // Normal row.
      ImGui::TextUnformatted("Normal");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(-80.f);
      char normal_buf[512];
      snprintf(normal_buf, sizeof(normal_buf), "%s",
               layer.normal_path.c_str());
      ImGui::InputText("##normal", normal_buf, sizeof(normal_buf),
                       ImGuiInputTextFlags_ReadOnly);
      ImGui::SameLine();
      if (ImGui::Button("Browse##normal")) {
        const std::string path = BrowseTexture();
        if (!path.empty())
          material_->SetLayerNormal(i, path, video_);
      }

      // Tiling row.
      float tiling = layer.tiling;
      if (ImGui::DragFloat("Tiling", &tiling, 0.1f, 0.01f, 256.f, "%.2f"))
        material_->SetLayerTiling(i, tiling);
    }
    ImGui::PopID();
  }

  ImGui::Spacing();
  if (layer_count < terrain::kMaxTerrainLayers) {
    if (ImGui::Button("+ Add Layer"))
      material_->AddLayer();
  } else {
    ImGui::TextDisabled("Maximum of %d layers reached.", terrain::kMaxTerrainLayers);
  }
}

// ---- Properties tab ---------------------------------------------------------

void TerrainEditorPanel::RenderPropertiesTab() {
  if (!data_) return;

  ImGui::SeparatorText("Height Range");
  ImGui::TextWrapped("Changing the height range reinterprets the existing "
                     "heightmap and rebuilds the normal map.");
  ImGui::Spacing();

  float min_h = data_->GetMinHeight();
  float max_h = data_->GetMaxHeight();

  const bool min_changed = ImGui::DragFloat("Min Height (m)", &min_h,
                                             0.5f, -10000.f, max_h - 0.1f, "%.1f");
  const bool min_deact   = ImGui::IsItemDeactivatedAfterEdit();

  const bool max_changed = ImGui::DragFloat("Max Height (m)", &max_h,
                                             0.5f, min_h + 0.1f, 10000.f, "%.1f");
  const bool max_deact   = ImGui::IsItemDeactivatedAfterEdit();

  if (min_changed || max_changed) {
    max_h = std::max(max_h, min_h + 0.1f);
    data_->SetMinHeight(min_h);
    data_->SetMaxHeight(max_h);
    if (terrain::TerrainRenderer::IsInstanced())
      terrain::TerrainRenderer::Instance().SetHeightRange(min_h, max_h);
  }

  if ((min_deact || max_deact) && normal_map_ && video_) {
    normal_map_->Build(*data_);
    normal_map_->Upload(video_);
    LOG_F(INFO, "TerrainEditorPanel: height range changed [%.1f, %.1f] — "
          "normal map rebuilt", min_h, max_h);
  }

  ImGui::Spacing();
  ImGui::SeparatorText("Info");
  ImGui::LabelText("Width (texels)",  "%d", data_->GetTexelWidth());
  ImGui::LabelText("Height (texels)", "%d", data_->GetTexelHeight());
  ImGui::LabelText("Resolution (m/texel)", "%.3f", data_->GetMetersPerTexel());
  ImGui::LabelText("World size (m)",
                   "%.1f × %.1f",
                   data_->GetWorldWidth(), data_->GetWorldHeight());
}

// ---- Import / Export tab ----------------------------------------------------

void TerrainEditorPanel::RenderImportExportTab() {
  if (!data_) return;

  // ---- Resize-confirmation modal -------------------------------------------
  if (io_state_ == IoState::kConfirmResize) {
    ImGui::OpenPopup("Resize Terrain?##io");
  }
  if (ImGui::BeginPopupModal("Resize Terrain?##io", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextWrapped(
        "The imported heightmap is %d\xc3\x97%d but the terrain is %d\xc3\x97%d.\n"
        "Resizing will rebuild the CDLOD quadtree and reset the splatmap to default.\n"
        "Continue?",
        io_pending_w_, io_pending_h_,
        data_->GetTexelWidth(), data_->GetTexelHeight());
    ImGui::Spacing();
    if (ImGui::Button("Yes, resize", {120.f, 0.f})) {
      ApplyImportedHeightmap(std::move(io_pending_data_),
                             io_pending_w_, io_pending_h_, true);
      io_state_ = IoState::kIdle;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", {120.f, 0.f})) {
      io_pending_data_.clear();
      io_state_ = IoState::kIdle;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  // ---- Status message -------------------------------------------------------
  if (!io_status_msg_.empty()) {
    if (io_status_ok_)
      ImGui::TextColored({0.4f, 0.9f, 0.4f, 1.f}, "%s", io_status_msg_.c_str());
    else
      ImGui::TextColored({0.9f, 0.3f, 0.3f, 1.f}, "%s", io_status_msg_.c_str());
    ImGui::Spacing();
  }

  // ---- Import section -------------------------------------------------------
  ImGui::SeparatorText("Import");
  ImGui::TextWrapped(
      "PNG: maps brightness [0\xe2\x80\x93""255] to height [min, max] (configurable).\n"
      "HDR: float values are world-space metres (EXR requires RGBE encoding).");
  ImGui::Spacing();

  ImGui::DragFloat("Min Height##import", &import_min_h_, 0.5f,
                   -10000.f, import_max_h_ - 0.1f, "%.1f m");
  ImGui::DragFloat("Max Height##import", &import_max_h_, 0.5f,
                   import_min_h_ + 0.1f, 10000.f, "%.1f m");
  import_max_h_ = std::max(import_max_h_, import_min_h_ + 0.1f);

  ImGui::Spacing();

  if (ImGui::Button("Import PNG...", {-1.f, 0.f})) {
    nfdu8char_t* out_path = nullptr;
    const nfdu8filteritem_t filter = {"PNG Heightmap", "png"};
    const nfdresult_t res =
        NFD_OpenDialogU8(&out_path, &filter, 1, nullptr);
    if (res == NFD_OKAY) {
      const std::string path(out_path);
      NFD_FreePathU8(out_path);
      LoadAndApplyPNG(path);
    } else if (res == NFD_ERROR) {
      LOG_F(ERROR, "TerrainEditorPanel: NFD error on PNG import");
    }
  }

  if (ImGui::Button("Import HDR / EXR...", {-1.f, 0.f})) {
    nfdu8char_t* out_path = nullptr;
    const nfdu8filteritem_t filter = {"HDR / EXR Heightmap", "hdr,exr"};
    const nfdresult_t res =
        NFD_OpenDialogU8(&out_path, &filter, 1, nullptr);
    if (res == NFD_OKAY) {
      const std::string path(out_path);
      NFD_FreePathU8(out_path);
      LoadAndApplyHDR(path);
    } else if (res == NFD_ERROR) {
      LOG_F(ERROR, "TerrainEditorPanel: NFD error on HDR import");
    }
  }

  // ---- Export section -------------------------------------------------------
  ImGui::Spacing();
  ImGui::SeparatorText("Export");
  ImGui::TextWrapped(
      "PNG: 8-bit grayscale, maps [min, max] height to [0\xe2\x80\x93""255].\n"
      "R16: raw uint16 binary (row-major, same format as map serialisation).");
  ImGui::Spacing();

  if (ImGui::Button("Export PNG...", {-1.f, 0.f})) {
    nfdu8char_t* out_path = nullptr;
    const nfdu8filteritem_t filter = {"PNG Heightmap", "png"};
    const nfdresult_t res =
        NFD_SaveDialogU8(&out_path, &filter, 1, nullptr, "heightmap.png");
    if (res == NFD_OKAY) {
      const std::string path(out_path);
      NFD_FreePathU8(out_path);
      DoExportPNG(path);
    } else if (res == NFD_ERROR) {
      LOG_F(ERROR, "TerrainEditorPanel: NFD error on PNG export");
    }
  }

  if (ImGui::Button("Export R16...", {-1.f, 0.f})) {
    nfdu8char_t* out_path = nullptr;
    const nfdu8filteritem_t filter = {"Raw 16-bit Heightmap", "r16"};
    const nfdresult_t res =
        NFD_SaveDialogU8(&out_path, &filter, 1, nullptr, "heightmap.r16");
    if (res == NFD_OKAY) {
      const std::string path(out_path);
      NFD_FreePathU8(out_path);
      DoExportR16(path);
    } else if (res == NFD_ERROR) {
      LOG_F(ERROR, "TerrainEditorPanel: NFD error on R16 export");
    }
  }
}

bool TerrainEditorPanel::LoadAndApplyPNG(const std::string& path) {
  io_status_msg_.clear();
  int w = 0, h = 0, channels = 0;
  uint8_t* pixels = stbi_load(path.c_str(), &w, &h, &channels, 1);
  if (!pixels) {
    io_status_msg_ = std::string("PNG load failed: ") + stbi_failure_reason();
    io_status_ok_  = false;
    LOG_F(ERROR, "TerrainEditorPanel: PNG load failed '%s': %s",
          path.c_str(), stbi_failure_reason());
    return false;
  }

  const std::size_t n = static_cast<std::size_t>(w) * h;
  std::vector<uint16_t> samples(n);
  const float range = import_max_h_ - import_min_h_;
  for (std::size_t i = 0; i < n; ++i) {
    const float world_h = import_min_h_ + (pixels[i] / 255.f) * range;
    samples[i] = data_->HeightToSample(world_h);
  }
  stbi_image_free(pixels);

  const bool needs_resize =
      (w != data_->GetTexelWidth() || h != data_->GetTexelHeight());
  if (needs_resize) {
    io_pending_data_ = std::move(samples);
    io_pending_w_    = w;
    io_pending_h_    = h;
    io_state_        = IoState::kConfirmResize;
    return true;
  }

  ApplyImportedHeightmap(samples, w, h, false);
  return true;
}

bool TerrainEditorPanel::LoadAndApplyHDR(const std::string& path) {
  io_status_msg_.clear();
  int w = 0, h = 0, channels = 0;
  float* pixels = stbi_loadf(path.c_str(), &w, &h, &channels, 1);
  if (!pixels) {
    io_status_msg_ = std::string("HDR/EXR load failed: ") + stbi_failure_reason();
    io_status_ok_  = false;
    LOG_F(ERROR, "TerrainEditorPanel: HDR/EXR load failed '%s': %s",
          path.c_str(), stbi_failure_reason());
    return false;
  }

  const std::size_t n = static_cast<std::size_t>(w) * h;
  std::vector<uint16_t> samples(n);
  for (std::size_t i = 0; i < n; ++i) {
    const float world_h = std::clamp(pixels[i],
                                     data_->GetMinHeight(),
                                     data_->GetMaxHeight());
    samples[i] = data_->HeightToSample(world_h);
  }
  stbi_image_free(pixels);

  const bool needs_resize =
      (w != data_->GetTexelWidth() || h != data_->GetTexelHeight());
  if (needs_resize) {
    io_pending_data_ = std::move(samples);
    io_pending_w_    = w;
    io_pending_h_    = h;
    io_state_        = IoState::kConfirmResize;
    return true;
  }

  ApplyImportedHeightmap(samples, w, h, false);
  return true;
}

void TerrainEditorPanel::ApplyImportedHeightmap(const std::vector<uint16_t>& data,
                                                int w, int h,
                                                bool needs_resize) {
  data_->ReplaceHeightmap(data.data(), w, h);

  normal_map_->Build(*data_);
  normal_map_->Upload(video_);

  if (needs_resize) {
    if (terrain::TerrainRenderer::IsInstanced())
      terrain::TerrainRenderer::Instance().Rebuild(*data_);
    if (material_)
      material_->ResetSplatmap(video_, w, h);
    LOG_F(INFO, "TerrainEditorPanel: heightmap imported and terrain resized to %dx%d",
          w, h);
  } else {
    if (terrain::TerrainRenderer::IsInstanced())
      terrain::TerrainRenderer::Instance().UpdateHeightmapTile(
          0, 0, w, h, *data_);
    LOG_F(INFO, "TerrainEditorPanel: heightmap imported %dx%d", w, h);
  }

  history_->Clear();
  io_status_msg_ = "Import successful.";
  io_status_ok_  = true;
}

void TerrainEditorPanel::DoExportPNG(const std::string& path) {
  io_status_msg_.clear();
  const int w = data_->GetTexelWidth();
  const int h = data_->GetTexelHeight();
  const std::size_t n = static_cast<std::size_t>(w) * h;

  std::vector<uint8_t> pixels(n);
  for (std::size_t i = 0; i < n; ++i)
    pixels[i] = static_cast<uint8_t>(
        (data_->GetRawData()[i] / 65535.f) * 255.f + 0.5f);

  const int ok = stbi_write_png(path.c_str(), w, h, 1, pixels.data(), w);
  if (!ok) {
    io_status_msg_ = "PNG export failed (check write permissions).";
    io_status_ok_  = false;
    LOG_F(ERROR, "TerrainEditorPanel: PNG export failed '%s'", path.c_str());
    return;
  }

  io_status_msg_ = "Export successful.";
  io_status_ok_  = true;
  LOG_F(INFO, "TerrainEditorPanel: heightmap exported to '%s'", path.c_str());
}

void TerrainEditorPanel::DoExportR16(const std::string& path) {
  io_status_msg_.clear();
  const int w = data_->GetTexelWidth();
  const int h = data_->GetTexelHeight();

  std::FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) {
    io_status_msg_ = "R16 export failed: cannot open file for writing.";
    io_status_ok_  = false;
    LOG_F(ERROR, "TerrainEditorPanel: R16 export failed '%s'", path.c_str());
    return;
  }

  std::fwrite(data_->GetRawData(), sizeof(uint16_t),
              static_cast<std::size_t>(w) * h, f);
  std::fclose(f);

  io_status_msg_ = "Export successful.";
  io_status_ok_  = true;
  LOG_F(INFO, "TerrainEditorPanel: R16 heightmap exported to '%s'", path.c_str());
}

// ---- Foliage tab ------------------------------------------------------------

void TerrainEditorPanel::RenderFoliageTab() {
  if (!terrain_obj_) {
    ImGui::TextDisabled("No terrain context.");
    return;
  }

  const int layer_count = terrain_obj_->GetFoliageLayerCount();

  // ---- Layer list -----------------------------------------------------------
  ImGui::SeparatorText("Layers");
  for (int i = 0; i < layer_count; ++i) {
    terrain::FoliageLayer* layer = terrain_obj_->GetFoliageLayer(i);
    const terrain::FoliageLayerDesc& desc = layer->GetDesc();

    ImGui::PushID(i);
    const bool selected = (foliage_active_layer_ == i);
    if (ImGui::Selectable(desc.mesh_path.empty() ? "(unnamed)" : desc.mesh_path.c_str(),
                          selected))
      foliage_active_layer_ = i;
    ImGui::PopID();
  }

  // ---- Add / Remove layer ---------------------------------------------------
  if (ImGui::Button("Add layer")) {
    if (layer_count < 8 && data_) {
      terrain::FoliageLayerDesc desc;
      auto layer = std::make_unique<terrain::FoliageLayer>(
          data_->GetTexelWidth(), data_->GetTexelHeight(), desc);
      terrain_obj_->AddFoliageLayer(std::move(layer));
      foliage_active_layer_ = terrain_obj_->GetFoliageLayerCount() - 1;
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Remove layer") && layer_count > 0) {
    terrain_obj_->RemoveFoliageLayer(foliage_active_layer_);
    foliage_active_layer_ = std::max(0, foliage_active_layer_ - 1);
  }

  // ---- Active layer settings ------------------------------------------------
  if (layer_count == 0 || foliage_active_layer_ >= layer_count) return;

  terrain::FoliageLayer*     active = terrain_obj_->GetFoliageLayer(foliage_active_layer_);
  terrain::FoliageLayerDesc& desc   = active->GetDesc();

  ImGui::SeparatorText("Layer settings");

  char mesh_buf[512];
  std::snprintf(mesh_buf, sizeof(mesh_buf), "%s", desc.mesh_path.c_str());
  if (ImGui::InputText("Mesh path", mesh_buf, sizeof(mesh_buf)))
    desc.mesh_path = mesh_buf;

  char tex_buf[512];
  std::snprintf(tex_buf, sizeof(tex_buf), "%s", desc.texture_path.c_str());
  if (ImGui::InputText("Texture path", tex_buf, sizeof(tex_buf)))
    desc.texture_path = tex_buf;

  ImGui::DragFloat("Spacing min", &desc.spacing_min, 0.05f, 0.05f, 10.f);
  ImGui::DragFloat("Spacing max", &desc.spacing_max, 0.05f, 0.05f, 10.f);
  ImGui::DragFloat("Scale min",   &desc.scale_min,   0.01f, 0.01f, 10.f);
  ImGui::DragFloat("Scale max",   &desc.scale_max,   0.01f, 0.01f, 10.f);
  ImGui::DragFloat("Cull dist",        &desc.cull_distance,      1.f, 10.f, 500.f);
  ImGui::DragFloat("Billboard dist",   &desc.billboard_distance,  1.f, 5.f,  200.f);

  // ---- Density brush --------------------------------------------------------
  ImGui::SeparatorText("Density brush");

  ImGui::RadioButton("Paint", reinterpret_cast<int*>(&foliage_brush_mode_),
                     static_cast<int>(FoliageBrushMode::kPaint));
  ImGui::SameLine();
  ImGui::RadioButton("Erase", reinterpret_cast<int*>(&foliage_brush_mode_),
                     static_cast<int>(FoliageBrushMode::kErase));

  ImGui::SliderFloat("Radius",   &foliage_radius_,   0.5f, 50.f);
  ImGui::SliderFloat("Strength", &foliage_strength_,  0.f,  1.f);

  // ---- Rebuild --------------------------------------------------------------
  ImGui::SeparatorText("Rebuild");
  if (ImGui::Button("Rebuild instances")) {
    if (data_) {
      active->RebuildInstances(*data_);
      if (renderer::FoliageRenderer::Instance().IsReady())
        renderer::FoliageRenderer::Instance().RebuildDirtyLayers();
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Build all")) {
    if (data_ && video_) {
      std::vector<terrain::FoliageLayer*> ptrs;
      for (int i = 0; i < terrain_obj_->GetFoliageLayerCount(); ++i)
        ptrs.push_back(terrain_obj_->GetFoliageLayer(i));
      renderer::FoliageRenderer::Instance().Build(video_, *data_, ptrs);
      renderer::Renderer::Instance().SetFoliageEnabled(true);
    }
  }
}

}  // namespace editor
