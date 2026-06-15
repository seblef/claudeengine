#include "editor/tools/TerrainSculptTool.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "abstract/VideoDevice.h"
#include "editor/EditorCommandHistory.h"
#include "editor/commands/PaintBrushCommand.h"
#include "editor/commands/SculptBrushCommand.h"
#include "editor/tools/EditorToolContext.h"
#include "editor/tools/ViewportRaycast.h"
#include "terrain/TerrainData.h"
#include "terrain/TerrainMaterial.h"
#include "terrain/TerrainRenderer.h"

#include <imgui.h>
#include <loguru.hpp>

namespace {
// Max raise/lower speed in metres per second at strength = 1.
constexpr float kBrushRate = 10.f;
}  // namespace

namespace editor {

TerrainSculptTool::TerrainSculptTool(
    terrain::TerrainData* data,
    terrain::TerrainMaterial* material,
    abstract::VideoDevice* video,
    EditorCommandHistory* history,
    std::function<void(float, float, bool, float)> on_foliage_brush,
    std::function<void()> on_foliage_end)
    : data_(data),
      material_(material),
      video_(video),
      history_(history),
      on_foliage_brush_(std::move(on_foliage_brush)),
      on_foliage_end_(std::move(on_foliage_end)) {}

bool TerrainSculptTool::IsCapturingMouse() const {
  return stroke_active_ || paint_stroke_active_ || foliage_stroke_active_;
}

void TerrainSculptTool::OnRender(const EditorToolContext& ctx,
                                  ImVec2 image_pos, ImVec2 image_size) {
  if (!ImGui::IsWindowHovered()) return;

  const bool lmb_down = ImGui::IsMouseDown(ImGuiMouseButton_Left)
                        && !ImGui::GetIO().KeyAlt;

  if (lmb_down) {
    const auto hit = ComputeTerrainHit(ctx.camera, data_,
                                        ImGui::GetMousePos(), image_pos, image_size);
    if (hit) {
      const float dt = ImGui::GetIO().DeltaTime;
      if (mode_ == Mode::kPaint) {
        OnPaintAt(hit->x, hit->z, !paint_stroke_active_);
      } else if (mode_ == Mode::kFoliage) {
        if (on_foliage_brush_) {
          on_foliage_brush_(hit->x, hit->z, !foliage_stroke_active_, dt);
          foliage_stroke_active_ = true;
        }
      } else {
        OnSculptAt(hit->x, hit->z, !stroke_active_, dt);
      }
    }
  } else {
    if (stroke_active_)         OnSculptEnd();
    if (paint_stroke_active_)   OnPaintEnd();
    if (foliage_stroke_active_) {
      foliage_stroke_active_ = false;
      if (on_foliage_end_) on_foliage_end_();
    }
  }
}

// ---- Falloff -----------------------------------------------------------------

float TerrainSculptTool::ComputeFalloff(float t) const {
  if (falloff_ == Falloff::kLinear) return 1.f - t;
  const float s = 1.f - t;
  return s * s * (3.f - 2.f * s);
}

// ---- Sculpt implementation ---------------------------------------------------

void TerrainSculptTool::EnsureRegionCovers(int nx0, int nz0, int nx1, int nz1) {
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

void TerrainSculptTool::ApplyBrushFootprint(float cx_world, float cz_world,
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

      const float w      = ComputeFalloff(t);
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
}

void TerrainSculptTool::OnSculptAt(float wx, float wz, bool first_touch,
                                    float dt) {
  if (!data_ || !video_) return;

  const float mpt    = data_->GetMetersPerTexel();
  const int r_texels = static_cast<int>(std::ceil(radius_ / mpt)) + 1;
  const int center_tx = static_cast<int>(wx / mpt);
  const int center_tz = static_cast<int>(wz / mpt);

  const int bx0 = std::max(0, center_tx - r_texels);
  const int bz0 = std::max(0, center_tz - r_texels);
  const int bx1 = std::min(data_->GetTexelWidth(),  center_tx + r_texels + 1);
  const int bz1 = std::min(data_->GetTexelHeight(), center_tz + r_texels + 1);

  if (first_touch) {
    stroke_flatten_h_ = data_->GetHeight(wx, wz);
    stroke_x0_        = bx0;
    stroke_z0_        = bz0;
    stroke_x1_        = bx1;
    stroke_z1_        = bz1;
    stroke_active_    = true;

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

void TerrainSculptTool::OnSculptEnd() {
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
        data_, video_,
        stroke_x0_, stroke_z0_, sw, sh,
        std::move(stroke_pre_snapshot_),
        std::move(post)));
  }

  stroke_active_ = false;
  LOG_F(9, "TerrainSculptTool::OnSculptEnd — stroke region [%d,%d)+[%d,%d]",
        stroke_x0_, stroke_z0_, sw, sh);
}

// ---- Paint implementation ----------------------------------------------------

void TerrainSculptTool::EnsurePaintRegionCovers(int nx0, int nz0,
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

void TerrainSculptTool::ApplyPaintFootprint(float cx_world, float cz_world) {
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

void TerrainSculptTool::OnPaintAt(float wx, float wz, bool first_touch) {
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

void TerrainSculptTool::OnPaintEnd() {
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
  LOG_F(9, "TerrainSculptTool::OnPaintEnd — paint region [%d,%d)+[%d,%d]",
        paint_x0_, paint_z0_, pw, ph);
}

}  // namespace editor
