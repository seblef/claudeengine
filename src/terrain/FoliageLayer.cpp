#include "terrain/FoliageLayer.h"

#include <cmath>
#include <random>

#include "terrain/TerrainData.h"

namespace terrain {

FoliageLayer::FoliageLayer(int map_width, int map_height, FoliageLayerDesc desc)
    : desc_(std::move(desc)),
      map_width_(map_width),
      map_height_(map_height),
      density_map_(static_cast<std::size_t>(map_width) * map_height, 0u) {}

void FoliageLayer::RebuildInstances(const TerrainData& data) {
  dirty_ = false;
  instances_.clear();

  const float world_w = data.GetWorldWidth();
  const float world_d = data.GetWorldHeight();
  const float cell    = desc_.spacing_max;

  if (cell <= 0.f || world_w <= 0.f || world_d <= 0.f) return;

  std::mt19937 rng(42u);
  std::uniform_real_distribution<float> jitter(0.f, 1.f);
  std::uniform_real_distribution<float> yaw_dist(0.f, 6.283185f);
  std::uniform_real_distribution<float> scale_dist(desc_.scale_min, desc_.scale_max);

  const int cols = static_cast<int>(world_w / cell) + 1;
  const int rows = static_cast<int>(world_d / cell) + 1;

  for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < cols; ++col) {
      const float wx = col * cell + jitter(rng) * cell;
      const float wz = row * cell + jitter(rng) * cell;

      if (wx >= world_w || wz >= world_d) continue;

      const float u  = wx / world_w;
      const float v  = wz / world_d;
      const int   tx = static_cast<int>(u * static_cast<float>(map_width_  - 1) + 0.5f);
      const int   tz = static_cast<int>(v * static_cast<float>(map_height_ - 1) + 0.5f);
      const uint8_t density = density_map_[tz * map_width_ + tx];

      if (density == 0u) continue;

      if (jitter(rng) * 255.f > static_cast<float>(density)) continue;

      const float y     = data.GetHeight(wx, wz);
      const float yaw   = yaw_dist(rng);
      const float scale = scale_dist(rng);

      core::Mat4f m = core::Mat4f::Translation({wx, y, wz}) *
                      core::Mat4f::RotationY(yaw) *
                      core::Mat4f::Scale3D({scale, scale, scale});
      instances_.push_back(m);
    }
  }
}

void FoliageLayer::BrushImpl(float wx, float wz, float radius, float strength,
                              bool erase, const TerrainData& data) {
  const float world_w = data.GetWorldWidth();
  const float world_d = data.GetWorldHeight();
  const float mpt     = data.GetMetersPerTexel();
  if (mpt <= 0.f) return;

  const int texel_r = static_cast<int>(radius / mpt) + 1;
  const int cx = static_cast<int>(wx / world_w * static_cast<float>(map_width_  - 1));
  const int cz = static_cast<int>(wz / world_d * static_cast<float>(map_height_ - 1));

  for (int dz = -texel_r; dz <= texel_r; ++dz) {
    for (int dx = -texel_r; dx <= texel_r; ++dx) {
      const int tx = cx + dx;
      const int tz = cz + dz;
      if (tx < 0 || tx >= map_width_ || tz < 0 || tz >= map_height_) continue;

      const float dist = std::sqrt(static_cast<float>(dx*dx + dz*dz)) * mpt;
      if (dist > radius) continue;

      const float t = dist / radius;
      const float w = 1.f - t * t;

      auto& v = density_map_[tz * map_width_ + tx];
      if (erase) {
        const int nv = static_cast<int>(v) - static_cast<int>(strength * w * 255.f);
        v = static_cast<uint8_t>(nv < 0 ? 0 : nv);
      } else {
        const int nv = static_cast<int>(v) + static_cast<int>(strength * w * 255.f);
        v = static_cast<uint8_t>(nv > 255 ? 255 : nv);
      }
    }
  }
  dirty_      = true;
  save_dirty_ = true;
}

void FoliageLayer::PaintBrush(float wx, float wz, float radius, float strength,
                               const TerrainData& data) {
  BrushImpl(wx, wz, radius, strength, false, data);
}

void FoliageLayer::EraseBrush(float wx, float wz, float radius, float strength,
                               const TerrainData& data) {
  BrushImpl(wx, wz, radius, strength, true, data);
}

}  // namespace terrain
