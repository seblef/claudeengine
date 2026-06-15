#include "editor/tools/ViewportRaycast.h"

#include <cmath>

#include "core/Camera.h"
#include "core/Mat4f.h"
#include "core/Vec3f.h"
#include "core/Vec4f.h"
#include "game/GameCamera.h"
#include "terrain/TerrainData.h"

#include <imgui.h>

namespace editor {

std::optional<core::Vec3f> ComputeTerrainHit(
    const game::GameCamera* camera,
    const terrain::TerrainData* terrain_data,
    ImVec2 mouse_pos, ImVec2 image_pos, ImVec2 image_size) {
  const float ndc_x = (mouse_pos.x - image_pos.x) / image_size.x * 2.f - 1.f;
  const float ndc_y = 1.f - (mouse_pos.y - image_pos.y) / image_size.y * 2.f;

  const core::Camera* cam        = camera->GetCamera();
  const core::Vec3f   ray_origin = cam->GetPosition();
  const core::Mat4f   vp_inv     = cam->GetViewProjectionMatrix().Inverse();

  const core::Vec4f clip(ndc_x, ndc_y, -1.f, 1.f);
  const core::Vec4f world4 = clip * vp_inv;
  if (std::abs(world4.w) < 1e-6f) return std::nullopt;
  const core::Vec3f world3(world4.x / world4.w,
                           world4.y / world4.w,
                           world4.z / world4.w);
  const core::Vec3f ray_dir = (world3 - ray_origin).Normalized();

  if (!terrain_data) {
    if (std::abs(ray_dir.y) < 1e-4f) return std::nullopt;
    const float t = -ray_origin.y / ray_dir.y;
    if (t < 0.f) return std::nullopt;
    return ray_origin + ray_dir * t;
  }

  // Ray-march against the heightmap with binary-search refinement.
  constexpr int   kSteps   = 64;
  constexpr float kMaxDist = 2000.f;
  const float     step     = kMaxDist / kSteps;

  float prev_t = 0.f;
  for (int i = 0; i < kSteps; ++i) {
    const float       t = static_cast<float>(i + 1) * step;
    const core::Vec3f p = ray_origin + ray_dir * t;
    const float terrain_h = terrain_data->GetHeight(p.x, p.z);
    if (p.y <= terrain_h) {
      float lo = prev_t;
      float hi = t;
      for (int j = 0; j < 8; ++j) {
        const float       mid = (lo + hi) * 0.5f;
        const core::Vec3f mp  = ray_origin + ray_dir * mid;
        if (mp.y <= terrain_data->GetHeight(mp.x, mp.z))
          hi = mid;
        else
          lo = mid;
      }
      return ray_origin + ray_dir * ((lo + hi) * 0.5f);
    }
    prev_t = t;
  }
  return std::nullopt;
}

}  // namespace editor
