#include "editor/TerrainPainterWindow.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numeric>
#include <vector>

#include <imgui.h>
#include <loguru.hpp>

#include "editor/EditorCommandHistory.h"
#include "editor/commands/PaintBrushCommand.h"
#include "terrain/TerrainData.h"
#include "terrain/TerrainMaterial.h"

namespace editor {

// ---- Context ----------------------------------------------------------------

void TerrainPainterWindow::SetContext(terrain::TerrainData* data,
                                       terrain::TerrainMaterial* material,
                                       abstract::VideoDevice* video,
                                       EditorCommandHistory* history) {
  data_     = data;
  material_ = material;
  video_    = video;
  history_  = history;

  if (!data_) {
    show_ = false;
    status_msg_.clear();
  }
}

// ---- Open -------------------------------------------------------------------

void TerrainPainterWindow::Open() {
  show_ = true;
  status_msg_.clear();
}

// ---- Noise helper -----------------------------------------------------------

float TerrainPainterWindow::Noise(int x, int z) {
  int n = x + z * 57;
  n = (n << 13) ^ n;
  const int h = (n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff;
  // Map [0, 0x7fffffff] → [-1, 1].
  return 1.f - static_cast<float>(h) / 1073741824.f;
}

// ---- Render -----------------------------------------------------------------

void TerrainPainterWindow::Render() {
  if (!show_) return;

  ImGui::SetNextWindowSize({420.f, 0.f}, ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Auto-Paint Terrain by Height", &show_,
                    ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::End();
    return;
  }

  if (!data_ || !material_) {
    ImGui::TextDisabled("No terrain in scene.");
    ImGui::End();
    return;
  }

  ImGui::TextWrapped(
      "Paints the whole terrain splatmap by height. Each range maps a "
      "height interval to a material layer. Overlapping ranges are blended. "
      "Noise widens and randomises the transitions.");
  ImGui::Spacing();

  // ---- Height range list ----------------------------------------------------
  ImGui::SeparatorText("Height Ranges");

  const float min_h = data_->GetMinHeight();
  const float max_h = data_->GetMaxHeight();
  const int layer_count = material_->GetLayerCount();

  // Table: Layer | Min height | Max height | Remove
  constexpr ImGuiTableFlags kTableFlags =
      ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
      ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp;

  if (ImGui::BeginTable("##ranges", 4, kTableFlags)) {
    ImGui::TableSetupColumn("Layer",      ImGuiTableColumnFlags_WidthFixed,   60.f);
    ImGui::TableSetupColumn("Min (m)",    ImGuiTableColumnFlags_WidthStretch, 1.f);
    ImGui::TableSetupColumn("Max (m)",    ImGuiTableColumnFlags_WidthStretch, 1.f);
    ImGui::TableSetupColumn("##remove",  ImGuiTableColumnFlags_WidthFixed,   28.f);
    ImGui::TableHeadersRow();

    int to_remove = -1;
    for (int i = 0; i < static_cast<int>(ranges_.size()); ++i) {
      HeightRange& r = ranges_[static_cast<std::size_t>(i)];
      ImGui::TableNextRow();
      ImGui::PushID(i);

      ImGui::TableSetColumnIndex(0);
      ImGui::SetNextItemWidth(-1.f);
      ImGui::SliderInt("##layer", &r.layer, 0, layer_count - 1);

      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(-1.f);
      ImGui::DragFloat("##min", &r.min_height, 0.5f, min_h, r.max_height - 0.1f, "%.1f");

      ImGui::TableSetColumnIndex(2);
      ImGui::SetNextItemWidth(-1.f);
      ImGui::DragFloat("##max", &r.max_height, 0.5f, r.min_height + 0.1f, max_h, "%.1f");

      ImGui::TableSetColumnIndex(3);
      if (ImGui::SmallButton("X")) to_remove = i;

      ImGui::PopID();
    }
    ImGui::EndTable();

    if (to_remove >= 0)
      ranges_.erase(ranges_.begin() + to_remove);
  }

  ImGui::Spacing();
  if (ImGui::Button("+ Add Range")) {
    HeightRange r;
    r.min_height = min_h;
    r.max_height = max_h;
    r.layer      = std::min(static_cast<int>(ranges_.size()), layer_count - 1);
    ranges_.push_back(r);
  }

  // ---- Noise control --------------------------------------------------------
  ImGui::Spacing();
  ImGui::SeparatorText("Transition");
  ImGui::SliderFloat("Noise Level", &noise_level_, 0.f, 1.f, "%.2f");
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "Controls how wide and ragged the layer boundaries are.\n"
        "0 = crisp horizontal lines.\n"
        "1 = heavily blurred organic transitions.");
  }

  // ---- Apply ----------------------------------------------------------------
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  const bool can_apply = !ranges_.empty() && layer_count > 0;
  if (!can_apply) ImGui::BeginDisabled();
  if (ImGui::Button("Apply", {120.f, 0.f})) {
    ApplyPainting();
  }
  if (!can_apply) ImGui::EndDisabled();

  if (!status_msg_.empty()) {
    ImGui::SameLine();
    if (status_ok_)
      ImGui::TextColored({0.4f, 0.9f, 0.4f, 1.f}, "%s", status_msg_.c_str());
    else
      ImGui::TextColored({0.9f, 0.3f, 0.3f, 1.f}, "%s", status_msg_.c_str());
  }

  ImGui::End();
}

// ---- Painting logic ---------------------------------------------------------

void TerrainPainterWindow::ApplyPainting() {
  if (!data_ || !material_ || !video_ || ranges_.empty()) return;

  const int sw = material_->GetSplatWidth();
  const int sh = material_->GetSplatHeight();
  if (sw <= 0 || sh <= 0) return;

  // Snapshot the splatmap before modification for undo/redo.
  const auto& pixels = material_->GetSplatmapPixels();
  std::vector<uint8_t> pre_snapshot(pixels.begin(), pixels.end());

  const float terrain_min = data_->GetMinHeight();
  const float terrain_max = data_->GetMaxHeight();
  const float height_range = terrain_max - terrain_min;
  if (height_range <= 0.f) {
    status_msg_ = "Height range is zero — cannot paint.";
    status_ok_  = false;
    return;
  }

  // Noise perturbation amplitude scales with noise_level and height range.
  // At noise_level = 1 the perturbation can shift heights by ±15% of the
  // full terrain range, producing wide, organic boundaries.
  const float noise_amplitude = noise_level_ * height_range * 0.15f;

  // Blend zone half-width around each range edge, proportional to noise.
  // Ensures soft gradients in the transition region even without noise.
  const float blend_half = std::max(0.5f, noise_amplitude * 0.6f);

  const float mpt = data_->GetMetersPerTexel();

  for (int sz = 0; sz < sh; ++sz) {
    for (int sx = 0; sx < sw; ++sx) {
      const float world_x = (static_cast<float>(sx) + 0.5f) * mpt;
      const float world_z = (static_cast<float>(sz) + 0.5f) * mpt;
      const float h       = data_->GetHeight(world_x, world_z);
      const float h_noisy = h + Noise(sx, sz) * noise_amplitude;

      // Accumulate per-layer weights from all matching ranges.
      float weights[terrain::kMaxTerrainLayers] = {};

      for (const HeightRange& r : ranges_) {
        if (r.layer < 0 || r.layer >= terrain::kMaxTerrainLayers) continue;

        // Smooth ramp-in above min_height and ramp-out below max_height.
        // blend_half controls how wide the blending zone is.
        const float lo = (h_noisy - r.min_height) / blend_half;
        const float hi = (r.max_height - h_noisy) / blend_half;
        const float t  = std::clamp(std::min(lo, hi), 0.f, 1.f);
        // Smoothstep so the ramp is C1-continuous.
        const float w  = t * t * (3.f - 2.f * t);

        weights[r.layer] += w;
      }

      // Normalise so all channels sum to 1.
      const float sum = std::accumulate(std::begin(weights), std::end(weights),
                                        0.f);

      if (sum < 1e-6f) {
        // No range covers this height — fall back to layer 0.
        weights[0] = 1.f;
      } else {
        std::transform(std::begin(weights), std::end(weights),
                       std::begin(weights),
                       [sum](float w) { return w / sum; });
      }

      for (int c = 0; c < terrain::kMaxTerrainLayers; ++c)
        material_->SetSplatWeight(sx, sz, c, weights[c]);
    }
  }

  // Upload the full splatmap to the GPU in one call.
  material_->UploadSplatTile(video_, 0, 0, sw, sh);

  // Capture post-paint state and push an undoable command.
  const auto& pixels_post = material_->GetSplatmapPixels();
  std::vector<uint8_t> post_snapshot(pixels_post.begin(), pixels_post.end());

  if (history_ && post_snapshot != pre_snapshot) {
    history_->Push(std::make_unique<PaintBrushCommand>(
        material_, video_,
        0, 0, sw, sh,
        std::move(pre_snapshot),
        std::move(post_snapshot)));
  }

  LOG_F(INFO, "TerrainPainterWindow: painted %d×%d splatmap, %zu ranges, "
        "noise=%.2f", sw, sh, ranges_.size(), noise_level_);

  status_msg_ = "Terrain painted successfully.";
  status_ok_  = true;
}

}  // namespace editor
