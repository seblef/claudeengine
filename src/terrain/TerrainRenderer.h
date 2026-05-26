#pragma once

#include <memory>
#include <vector>

#include "abstract/ConstantBuffer.h"
#include "abstract/RawTexture.h"
#include "abstract/Shader.h"
#include "abstract/VideoDevice.h"
#include "core/Singleton.h"
#include "core/Vec2f.h"
#include "terrain/CDLODQuadTree.h"
#include "terrain/TerrainPatch.h"
#include "terrain/TerrainPatchMesh.h"

namespace core { class Camera; }

namespace terrain {

class TerrainData;

// Singleton G-buffer pass renderer for terrain.
//
// Owns the GPU resources for one terrain: heightmap texture, CDLOD quadtree,
// shared patch mesh, and per-patch constant buffer (slot 6).
//
// Usage:
//   new TerrainRenderer();
//   TerrainRenderer::Instance().Init(video, data);
//   // each frame, inside the geometry pass:
//   TerrainRenderer::Instance().Render(camera);
//   TerrainRenderer::Shutdown();
class TerrainRenderer : public core::Singleton<TerrainRenderer> {
 public:
  TerrainRenderer() = default;
  ~TerrainRenderer();

  TerrainRenderer(const TerrainRenderer&)            = delete;
  TerrainRenderer& operator=(const TerrainRenderer&) = delete;
  TerrainRenderer(TerrainRenderer&&)                 = delete;
  TerrainRenderer& operator=(TerrainRenderer&&)      = delete;

  // Uploads the heightmap texture, builds the CDLOD quadtree, and creates the
  // shared patch mesh. Must be called before Render().
  //
  // patch_size — quads per patch side (e.g. 64); trade-off between draw-call
  //              count and GPU vertex processing per patch.
  // lod_count  — number of LOD levels; more levels = wider LOD range.
  void Init(abstract::VideoDevice* video, const TerrainData& data,
            int patch_size = 64, int lod_count = 6);

  // Updates the maximum triangle budget forwarded to CDLODQuadTree::Select().
  void SetTriangleBudget(int budget) { triangle_budget_ = budget; }

  // Enables or disables hardware tessellation for close-in LOD levels.
  // Tessellation is applied to patches whose LOD ≤ max_tess_lod (default: 2).
  // Has no effect if the terrain shader does not have tessellation stages.
  void SetTessellationEnabled(bool enabled) { tess_enabled_ = enabled; }

  // Sets the camera distance at which the tessellation factor falls to 1.0.
  // Patches closer than this distance are subdivided up to max_tess_ times.
  void SetTessellationFalloffDistance(float dist) { tess_falloff_dist_ = dist; }

  // Sets the maximum tessellation factor applied at camera distance = 0.
  void SetMaxTessFactor(float max_tess) { max_tess_ = max_tess; }

  // Sets the highest LOD level that receives tessellation (0 = finest).
  void SetMaxTessLod(int max_lod) { max_tess_lod_ = max_lod; }

  // Returns true if Init() has been called successfully.
  [[nodiscard]] bool IsReady() const { return shader_ != nullptr; }

  // Selects visible patches from the quadtree and renders them into the
  // currently bound G-buffer. Must be called inside the geometry pass.
  void Render(const core::Camera& camera);

 private:
  void FillPatchInfos(const TerrainPatch& patch);

  abstract::VideoDevice*                    video_         = nullptr;
  abstract::Shader*                         shader_        = nullptr;
  std::unique_ptr<abstract::RawTexture>     heightmap_;
  std::unique_ptr<abstract::ConstantBuffer> patch_cb_;
  // cppcheck-suppress unusedStructMember
  CDLODQuadTree                             quadtree_;
  std::unique_ptr<TerrainPatchMesh>         patch_mesh_;
  // cppcheck-suppress unusedStructMember
  std::vector<TerrainPatch>                 patches_;

  int         triangle_budget_    = 500'000;
  int         patch_size_         = 64;
  float       meters_per_texel_   = 1.f;
  float       heightmap_scale_    = 1.f;
  float       heightmap_offset_   = 0.f;
  core::Vec2f inv_terrain_world_  = {};

  bool        tess_enabled_       = true;
  float       tess_falloff_dist_  = 500.f;
  float       max_tess_           = 8.f;
  int         max_tess_lod_       = 2;
};

}  // namespace terrain
