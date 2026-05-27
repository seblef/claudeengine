#pragma once

#include <memory>
#include <vector>

#include "abstract/ShaderStorageBuffer.h"
#include "abstract/VideoDevice.h"
#include "core/Mat4f.h"
#include "core/Singleton.h"
#include "core/Vec4f.h"
#include "renderer/GeometryData.h"

namespace abstract { class Texture; }
namespace core    { class Camera;   }
namespace terrain {
class FoliageLayer;
class TerrainData;
}

namespace renderer {

// Maximum foliage instances rendered per layer in a single draw call.
// Near and billboard counts each are individually capped at this value.
inline constexpr int kMaxFoliageInstances = 32768;

// Singleton GPU renderer for terrain foliage layers.
//
// Builds GPU resources (SSBO for instance matrices, loaded mesh geometry,
// albedo texture) for each layer registered via SetLayers().
//
// Frame pipeline integration:
//   - Renderer calls Render()          in the geometry pass  (near instances)
//   - Renderer calls RenderBillboards() in the emissive pass  (far instances)
//
// SSBO binding points (separate namespace from UBOs):
//   Binding 0 — near  instance world matrices   (mat4[], row_major)
//   Binding 1 — billboard positions + uniform scale (vec4[], xyz=pos, w=scale)
//
// Usage:
//   new FoliageRenderer();
//   FoliageRenderer::Instance().Build(video, terrain_data, layers);
//   FoliageRenderer::Instance().Render(camera);          // geometry pass
//   FoliageRenderer::Instance().RenderBillboards(camera); // emissive pass
//   FoliageRenderer::Shutdown();
class FoliageRenderer : public core::Singleton<FoliageRenderer> {
 public:
  FoliageRenderer() = default;
  ~FoliageRenderer();

  FoliageRenderer(const FoliageRenderer&)            = delete;
  FoliageRenderer& operator=(const FoliageRenderer&) = delete;
  FoliageRenderer(FoliageRenderer&&)                 = delete;
  FoliageRenderer& operator=(FoliageRenderer&&)      = delete;

  // Uploads GPU resources for all registered layers.
  // Loads mesh geometry + albedo texture for each layer.
  // Marks all layers dirty so instance generation runs on first Render().
  // video and terrain_data must outlive this renderer.
  void Build(abstract::VideoDevice* video,
             const terrain::TerrainData& terrain_data,
             const std::vector<terrain::FoliageLayer*>& layers);

  // Frees all GPU resources and resets to an empty state.
  void Reset();

  // Renders near instances (dist <= billboard_distance) into the G-buffer.
  // Must be called inside the geometry pass with the G-buffer FBO bound.
  void Render(const core::Camera& camera);

  // Renders billboard instances (billboard_distance < dist <= cull_distance)
  // into the emissive FBO (additive blending, depth read-only).
  void RenderBillboards(const core::Camera& camera);

  // Forces a rebuild of instance lists for all dirty layers on the next Render().
  void RebuildDirtyLayers();

  [[nodiscard]] bool IsReady() const { return video_ != nullptr; }

 private:
  // GPU resources for one foliage layer.
  struct LayerGPU {
    terrain::FoliageLayer*                         layer = nullptr;
    std::unique_ptr<GeometryData>                  geometry;
    abstract::Texture*                             albedo_tex  = nullptr;
    std::unique_ptr<abstract::ShaderStorageBuffer> near_ssbo;
    std::unique_ptr<abstract::ShaderStorageBuffer> billboard_ssbo;
    // cppcheck-suppress unusedStructMember
    int                                            near_count  = 0;
    // cppcheck-suppress unusedStructMember
    int                                            bill_count  = 0;
  };

  // Creates a 1 × 1 billboard quad centred at x=0 with y in [0, 1].
  std::unique_ptr<GeometryData> MakeBillboardQuad() const;

  // Classifies instances into near/billboard, uploads to SSBOs, returns counts.
  void ClassifyAndUpload(LayerGPU& gpu, const core::Camera& camera);

  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice*   video_            = nullptr;
  // cppcheck-suppress unusedStructMember
  const terrain::TerrainData* terrain_data_  = nullptr;
  // cppcheck-suppress unusedStructMember
  abstract::Shader*        mesh_shader_      = nullptr;
  // cppcheck-suppress unusedStructMember
  abstract::Shader*        billboard_shader_ = nullptr;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<GeometryData> billboard_quad_;
  // cppcheck-suppress unusedStructMember
  std::vector<LayerGPU>    layers_;
};

}  // namespace renderer
