#include "terrain/TerrainRenderer.h"

#include <loguru.hpp>

#include "abstract/BufferUsage.h"
#include "abstract/CullFace.h"
#include "abstract/PrimitiveType.h"
#include "abstract/RenderTargetGroup.h"
#include "core/Camera.h"
#include "terrain/TerrainData.h"
#include "terrain/TerrainMaterial.h"
#include "terrain/TerrainPatchInfos.h"

namespace terrain {

namespace {
constexpr int kPatchInfosSlot    = 6;
constexpr int kPatchInfosFloat4s = sizeof(TerrainPatchInfos) / 16;  // 80 / 16 = 5
constexpr int kHeightmapSlot     = 0;
constexpr int kSplatmapSlot      = 1;
constexpr int kAlbedoBaseSlot    = 2;   // slots 2-5 for 4 layer albedos
constexpr int kNormalBaseSlot    = 6;   // slots 6-9 for 4 layer normals
constexpr int kMacroTextureSlot  = 10;
}  // namespace

TerrainRenderer::~TerrainRenderer() {
  if (shader_)           shader_->Release();
  if (wireframe_shader_) wireframe_shader_->Release();
}

void TerrainRenderer::Init(abstract::VideoDevice* video, const TerrainData& data,
                           int patch_size, int lod_count) {
  video_      = video;
  patch_size_ = patch_size;

  meters_per_texel_  = data.GetMetersPerTexel();
  heightmap_scale_   = data.GetMaxHeight() - data.GetMinHeight();
  heightmap_offset_  = data.GetMinHeight();
  inv_terrain_world_ = {1.f / data.GetWorldWidth(), 1.f / data.GetWorldHeight()};

  heightmap_  = video->CreateHeightmapTexture(
      data.GetTexelWidth(), data.GetTexelHeight(), data.GetRawData());
  quadtree_.Build(data, patch_size, lod_count);
  patch_mesh_ = std::make_unique<TerrainPatchMesh>(video, patch_size);
  patch_cb_   = video->CreateConstantBuffer(
      kPatchInfosFloat4s, kPatchInfosSlot, abstract::BufferUsage::kDynamic);
  shader_           = video->CreateShader("terrain/terrain");
  wireframe_shader_ = video->CreateShader("terrain/terrain_wireframe");

  LOG_F(INFO, "TerrainRenderer::Init — patch_size=%d lod_count=%d", patch_size, lod_count);
}

void TerrainRenderer::BindMaterialTextures() const {
  if (!material_) return;

  material_->GetSplatmap()->Bind(kSplatmapSlot);

  const int layer_count = material_->GetLayerCount();
  for (int i = 0; i < layer_count; ++i) {
    if (material_->GetAlbedo(i))
      material_->GetAlbedo(i)->Bind(kAlbedoBaseSlot + i);
    if (material_->GetNormal(i))
      material_->GetNormal(i)->Bind(kNormalBaseSlot + i);
  }

  if (macro_texture_)
    macro_texture_->Bind(kMacroTextureSlot);
}

void TerrainRenderer::Render(const core::Camera& camera) {
  if (!shader_) return;

  quadtree_.Select(camera, triangle_budget_, patches_);
  LOG_F(INFO, "Num patches: %zu", patches_.size());
  if (patches_.empty()) return;

  heightmap_->Bind(kHeightmapSlot);
  BindMaterialTextures();
  patch_cb_->Bind();

  const bool use_tess = tess_enabled_ && shader_->HasTessellation();

  // Tessellated pass: low-LOD patches rendered as GL_PATCHES.
  if (use_tess) {
    LOG_F(INFO, "Rendeing with tesselation: %d", patch_mesh_->GetTessIndexCount());
    shader_->ActivateTess();
    video_->SetPrimitiveType(abstract::PrimitiveType::kPatch4);
    for (const TerrainPatch& patch : patches_) {
      if (patch.lod > max_tess_lod_) continue;
      FillPatchInfos(patch);
      patch_mesh_->BindTess();
      video_->RenderIndexed(patch_mesh_->GetTessIndexCount());
    }
  }

  // Standard pass: high-LOD patches rendered as triangles.
  shader_->Activate();
  video_->SetPrimitiveType(abstract::PrimitiveType::kTriangleList);
  LOG_F(INFO, "Rendeing without tesselation: %d", patch_mesh_->GetIndexCount());
  for (const TerrainPatch& patch : patches_) {
    if (use_tess && patch.lod <= max_tess_lod_) continue;
    FillPatchInfos(patch);
    patch_mesh_->Bind();
    video_->RenderIndexed(patch_mesh_->GetIndexCount());
  }
}

void TerrainRenderer::FillPatchInfos(const TerrainPatch& patch) {
  // patch_scale = world metres per mesh unit at this LOD.
  const float lod_multiplier = static_cast<float>(1 << patch.lod);
  const float patch_scale    = meters_per_texel_ * lod_multiplier;

  TerrainPatchInfos infos;
  infos.patch_origin    = {
      static_cast<float>(patch.grid_x) * patch_scale * static_cast<float>(patch_size_),
      static_cast<float>(patch.grid_z) * patch_scale * static_cast<float>(patch_size_)};
  infos.patch_scale        = patch_scale;
  infos.lod_level          = patch.lod;
  infos.morph_factor       = patch.morph;
  infos.heightmap_scale    = heightmap_scale_;
  infos.heightmap_offset   = heightmap_offset_;
  infos.inv_terrain_world  = inv_terrain_world_;
  infos.tess_falloff_dist  = tess_falloff_dist_;
  infos.max_tess           = max_tess_;
  infos.triplanar_threshold = triplanar_threshold_;
  infos.use_macro_texture  = (macro_texture_ != nullptr) ? 1 : 0;

  if (material_) {
    const int n = material_->GetLayerCount();
    infos.tiling = {
        n > 0 ? material_->GetLayer(0).tiling : 1.f,
        n > 1 ? material_->GetLayer(1).tiling : 1.f,
        n > 2 ? material_->GetLayer(2).tiling : 1.f,
        n > 3 ? material_->GetLayer(3).tiling : 1.f};
  }

  patch_cb_->Fill(&infos);
}

void TerrainRenderer::RenderWireframe(abstract::VideoDevice* video,
                                       const core::Camera& camera,
                                       abstract::RenderTargetGroup* fbo) {
  if (!wireframe_shader_) return;

  quadtree_.Select(camera, triangle_budget_, patches_);
  if (patches_.empty()) return;

  fbo->BindForWriting();
  video->SetDepthTestEnabled(false);
  video->SetDepthWriteEnabled(false);
  video->SetFaceCulling(abstract::CullFace::kBack);
  video->SetWireframeEnabled(true);
  video->SetPrimitiveType(abstract::PrimitiveType::kTriangleList);

  wireframe_shader_->Activate();
  heightmap_->Bind(kHeightmapSlot);
  patch_cb_->Bind();

  for (const TerrainPatch& patch : patches_) {
    FillPatchInfos(patch);
    patch_mesh_->Bind();
    video->RenderIndexed(patch_mesh_->GetIndexCount());
  }

  video->SetWireframeEnabled(false);
  fbo->UnbindForWriting();

  video->SetDepthWriteEnabled(true);
  video->SetFaceCulling(abstract::CullFace::kBack);
  video->SetPrimitiveType(abstract::PrimitiveType::kTriangleList);
}

}  // namespace terrain
