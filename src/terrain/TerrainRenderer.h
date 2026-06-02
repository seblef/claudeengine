#pragma once

#include <memory>
#include <vector>

#include "abstract/ConstantBuffer.h"
#include "abstract/RawTexture.h"
#include "abstract/Shader.h"
#include "abstract/Texture.h"
#include "abstract/VideoDevice.h"
#include "core/Singleton.h"
#include "core/Vec2f.h"
#include "terrain/CDLODQuadTree.h"
#include "terrain/TerrainPatch.h"
#include "terrain/TerrainPatchMesh.h"

namespace abstract { class RenderTargetGroup; }
namespace core { class Camera; }

namespace terrain {

class TerrainData;
class TerrainMaterial;

// Singleton G-buffer pass renderer for terrain.
//
// Owns the GPU resources for one terrain: heightmap texture, CDLOD quadtree,
// shared patch mesh, and per-patch constant buffer (slot 6).
//
// Texture slot assignment:
//   0 — heightmap       (VS / TESE)
//   1 — splatmap        (PS)
//   2–5 — layer albedos (PS, layers 0–3)
//   6–9 — layer normals (PS, layers 0–3)
//  10 — macro texture   (PS, optional)
//
// Usage:
//   new TerrainRenderer();
//   TerrainRenderer::Instance().Init(video, data);
//   TerrainRenderer::Instance().SetMaterial(&material);   // optional
//   TerrainRenderer::Instance().SetMacroTexture(tex);     // optional
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

  // Releases all GPU resources and resets the renderer to its pre-Init() state.
  // After this call IsReady() returns false and Render() is a no-op.
  // Call when the terrain is removed from the scene so the singleton can be
  // re-Inited for a new terrain without destroying and recreating it.
  void Deinit();

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

  // Attaches a terrain material for splatmap-based layer blending.
  // The renderer does not take ownership; caller must ensure the material
  // outlives the renderer. Pass nullptr to detach.
  void SetMaterial(const TerrainMaterial* material) { material_ = material; }

  // Attaches an optional macro texture bound to slot 10 during rendering.
  // The renderer does not take ownership. Pass nullptr to detach.
  void SetMacroTexture(abstract::Texture* tex) { macro_texture_ = tex; }

  // Sets the |world_normal.y| threshold below which triplanar mapping activates.
  void SetTriplanarThreshold(float threshold) { triplanar_threshold_ = threshold; }

  // Returns true if Init() has been called successfully.
  [[nodiscard]] bool IsReady() const { return shader_ != nullptr; }

  // Re-uploads a rectangular tile of the heightmap texture after a sculpt edit.
  // (texel_x, texel_z) is the top-left corner; w×h is the size in texels.
  // No-op when Init() has not been called.
  void UpdateHeightmapTile(int texel_x, int texel_z, int w, int h,
                           const TerrainData& data);

  // Re-creates the heightmap GPU texture and rebuilds the CDLOD quadtree using
  // the patch_size and lod_count stored from the last Init() call. Use this
  // after a heightmap import that may have changed terrain dimensions.
  // No-op when Init() has not been called.
  void Rebuild(const TerrainData& data);

  // Updates the height-range constants forwarded to the shader each frame.
  // Call after TerrainData::SetMinHeight() / SetMaxHeight(); does not require
  // re-uploading the heightmap GPU texture.
  void SetHeightRange(float min_h, float max_h) {
    heightmap_scale_  = max_h - min_h;
    heightmap_offset_ = min_h;
  }

  // Selects visible patches from the quadtree and renders them into the
  // currently bound G-buffer. Must be called inside the geometry pass.
  void Render(const core::Camera& camera);

  // Renders terrain patch edges as a flat-colour wireframe overlay into fbo.
  //
  // Intended as a debug aid. Draws all visible patches with glPolygonMode
  // GL_LINE using a simple white shader; depth testing is disabled so the
  // overlay is always visible. fbo must remain valid for the duration of
  // the call.
  //
  // Must be called after Init() and after Render() so that the scene UBO
  // (slot 2) is already bound.
  void RenderWireframe(abstract::VideoDevice* video,
                       const core::Camera& camera,
                       abstract::RenderTargetGroup* fbo);

 private:
  void BindMaterialTextures() const;
  void FillPatchInfos(const TerrainPatch& patch);

  abstract::VideoDevice*                    video_         = nullptr;
  abstract::Shader*                         shader_        = nullptr;
  abstract::Shader*                         wireframe_shader_ = nullptr;
  std::unique_ptr<abstract::RawTexture>     heightmap_;
  std::unique_ptr<abstract::ConstantBuffer> patch_cb_;
  // cppcheck-suppress unusedStructMember
  CDLODQuadTree                             quadtree_;
  std::unique_ptr<TerrainPatchMesh>         patch_mesh_;
  // cppcheck-suppress unusedStructMember
  std::vector<TerrainPatch>                 patches_;

  const TerrainMaterial*  material_            = nullptr;
  abstract::Texture*      macro_texture_       = nullptr;

  int         triangle_budget_       = 500'000;
  int         patch_size_            = 64;
  int         lod_count_             = 6;
  float       meters_per_texel_      = 1.f;
  float       heightmap_scale_       = 1.f;
  float       heightmap_offset_      = 0.f;
  core::Vec2f inv_terrain_world_     = {};
  float       triplanar_threshold_   = 0.5f;

  bool        tess_enabled_       = true;
  // cppcheck-suppress unusedStructMember
  mutable bool logged_origin_     = false;
  float       tess_falloff_dist_  = 500.f;
  float       max_tess_           = 8.f;
  int         max_tess_lod_       = 2;
};

}  // namespace terrain
