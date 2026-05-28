#pragma once

#include <memory>
#include <vector>

#include "abstract/ShaderStorageBuffer.h"
#include "abstract/VideoDevice.h"
#include "core/BBox3.h"
#include "core/Mat4f.h"
#include "core/Singleton.h"
#include "core/Vec4f.h"
#include "renderer/GeometryData.h"

namespace abstract { class Texture; }
namespace core    { class Camera;   }
namespace environment { class TreeLayer; }
namespace terrain     { class TerrainData; }

namespace renderer {

// Maximum tree instances rendered per layer in a single draw call.
// Near-mesh and billboard counts are each capped individually.
inline constexpr int kMaxTreeInstances = 16384;

// Singleton GPU renderer for GPU-instanced trees with a 2-stage LOD and
// vertex-shader wind sway animation.
//
// Two render passes per layer:
//   - Mesh pass   (geometry pass): full-res OBJ mesh with wind sway
//   - Billboard pass (emissive pass): camera-facing quad alpha-tested
//
// Wind sway uses the WindInfos constant buffer (slot 7), which must be bound
// by the caller (Renderer::Update does this before the geometry pass).
//
// SSBO binding points:
//   Binding 2 — near instance data (vec4[]: xyz=world pos, w=scale)
//   Binding 3 — billboard instance data (vec4[]: xyz=world pos, w=scale)
//
// Usage:
//   new TreeRenderer();
//   TreeRenderer::Instance().Build(video, terrain, layers);
//   TreeRenderer::Instance().Render(camera);           // geometry pass
//   TreeRenderer::Instance().RenderBillboards(camera); // emissive pass
//   TreeRenderer::Shutdown();
class TreeRenderer : public core::Singleton<TreeRenderer> {
 public:
  TreeRenderer() = default;
  ~TreeRenderer();

  TreeRenderer(const TreeRenderer&)            = delete;
  TreeRenderer& operator=(const TreeRenderer&) = delete;
  TreeRenderer(TreeRenderer&&)                 = delete;
  TreeRenderer& operator=(TreeRenderer&&)      = delete;

  // Generates instance SSBOs for each layer (placed from density map + terrain
  // height) and loads tree/billboard shaders.
  // video and terrain_data must outlive this renderer.
  void Build(abstract::VideoDevice* video,
             const terrain::TerrainData& terrain_data,
             const std::vector<environment::TreeLayer*>& layers);

  // Frees all GPU resources and resets to an empty state.
  void Reset();

  // Renders near-mesh instances (dist <= billboard_distance) into the G-buffer
  // with wind sway. Must be called inside the geometry pass.
  void Render(const core::Camera& camera);

  // Renders billboard instances (billboard_distance < dist <= cull_distance)
  // into the emissive FBO. Must be called inside the emissive pass.
  void RenderBillboards(const core::Camera& camera);

  [[nodiscard]] bool IsReady() const { return video_ != nullptr; }

 private:
  // GPU resources for one tree layer.
  struct LayerGPU {
    environment::TreeLayer*                         layer = nullptr;
    std::unique_ptr<GeometryData>                   geometry;
    abstract::Texture*                              billboard_tex = nullptr;
    std::unique_ptr<abstract::ShaderStorageBuffer>  near_ssbo;
    std::unique_ptr<abstract::ShaderStorageBuffer>  billboard_ssbo;
    // cppcheck-suppress unusedStructMember
    int                                             near_count    = 0;
    // cppcheck-suppress unusedStructMember
    int                                             bill_count    = 0;
    // cppcheck-suppress unusedStructMember
    float                                           trunk_sway    = 0.04f;
    // cppcheck-suppress unusedStructMember
    float                                           leaf_sway     = 0.08f;
    // cppcheck-suppress unusedStructMember
    float                                           mesh_height   = 1.f;   // bbox Y range
  };

  // Creates a 1×1 billboard quad centred at x=0 with y ∈ [0, 1].
  std::unique_ptr<GeometryData> MakeBillboardQuad() const;

  // Scatters instance positions from density map + terrain height for one layer.
  static void GenerateInstances(const LayerGPU& gpu,
                                const terrain::TerrainData& terrain,
                                std::vector<core::Vec4f>& out);

  // Splits the instance list into near/billboard buckets, uploads to SSBOs.
  void ClassifyAndUpload(LayerGPU& gpu, const core::Camera& camera);

  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice*          video_            = nullptr;
  // cppcheck-suppress unusedStructMember
  const terrain::TerrainData*     terrain_data_     = nullptr;
  // cppcheck-suppress unusedStructMember
  abstract::Shader*               mesh_shader_      = nullptr;
  // cppcheck-suppress unusedStructMember
  abstract::Shader*               billboard_shader_ = nullptr;
  std::unique_ptr<GeometryData>   billboard_quad_;
  // cppcheck-suppress unusedStructMember
  std::vector<LayerGPU>           layers_;
  // All instance positions (world pos + scale) pre-built in Build().
  // cppcheck-suppress unusedStructMember
  std::vector<std::vector<core::Vec4f>> all_instances_;
};

}  // namespace renderer
