#include "terrain/TerrainRenderer.h"

#include <loguru.hpp>

#include "abstract/BufferUsage.h"
#include "abstract/PrimitiveType.h"
#include "core/Camera.h"
#include "terrain/TerrainData.h"
#include "terrain/TerrainPatchInfos.h"

namespace terrain {

namespace {
constexpr int kPatchInfosSlot    = 6;
constexpr int kPatchInfosFloat4s = sizeof(TerrainPatchInfos) / 16;  // 48 / 16 = 3
constexpr int kHeightmapSlot     = 0;
}  // namespace

TerrainRenderer::~TerrainRenderer() {
  if (shader_) shader_->Release();
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
  shader_     = video->CreateShader("terrain/terrain");

  LOG_F(INFO, "TerrainRenderer::Init — patch_size=%d lod_count=%d", patch_size, lod_count);
}

void TerrainRenderer::Render(const core::Camera& camera) {
  if (!shader_) return;

  quadtree_.Select(camera, triangle_budget_, patches_);
  if (patches_.empty()) return;

  heightmap_->Bind(kHeightmapSlot);
  patch_cb_->Bind();

  const bool use_tess = tess_enabled_ && shader_->HasTessellation();

  // Tessellated pass: low-LOD patches rendered as GL_PATCHES.
  if (use_tess) {
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
  infos.patch_scale      = patch_scale;
  infos.lod_level        = patch.lod;
  infos.morph_factor     = patch.morph;
  infos.heightmap_scale    = heightmap_scale_;
  infos.heightmap_offset   = heightmap_offset_;
  infos.inv_terrain_world  = inv_terrain_world_;
  infos.tess_falloff_dist  = tess_falloff_dist_;
  infos.max_tess           = max_tess_;

  patch_cb_->Fill(&infos);
}

}  // namespace terrain
