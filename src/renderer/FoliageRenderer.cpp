#include "renderer/FoliageRenderer.h"

#include <loguru.hpp>

#include "abstract/Texture.h"
#include "core/Camera.h"
#include "core/Config.h"
#include "core/Vec3f.h"
#include "core/Vertex3D.h"
#include "renderer/MeshLoader.h"
#include "terrain/FoliageLayer.h"
#include "terrain/TerrainData.h"

namespace renderer {

namespace {

// Extracts the world position and uniform scale from a foliage instance matrix.
// The matrix is TRS with uniform scale, so scale = length of first column.
inline float ExtractScale(const core::Mat4f& m) {
  const float sx = m(0, 0), sy = m(0, 1), sz = m(0, 2);
  return std::sqrt(sx*sx + sy*sy + sz*sz);
}

inline core::Vec3f ExtractPosition(const core::Mat4f& m) {
  return {m(0, 3), m(1, 3), m(2, 3)};
}

}  // namespace

FoliageRenderer::~FoliageRenderer() {
  Reset();
}

void FoliageRenderer::Reset() {
  for (auto& gpu : layers_) {
    if (gpu.albedo_tex) {
      gpu.albedo_tex->Release();
      gpu.albedo_tex = nullptr;
    }
  }
  layers_.clear();
  if (mesh_shader_) {
    mesh_shader_->Release();
    mesh_shader_ = nullptr;
  }
  if (billboard_shader_) {
    billboard_shader_->Release();
    billboard_shader_ = nullptr;
  }
  billboard_quad_.reset();
  video_        = nullptr;
  terrain_data_ = nullptr;
}

std::unique_ptr<GeometryData> FoliageRenderer::MakeBillboardQuad() const {
  // 4 vertices: bottom-left, bottom-right, top-right, top-left
  // x in [-0.5, 0.5], y in [0, 1], z = 0
  const core::Vec3f n(0.f, 0.f, 1.f);
  const core::Vec3f t(1.f, 0.f, 0.f);
  const core::Vec3f b(0.f, 1.f, 0.f);
  const core::Vertex3D verts[4] = {
    { {-0.5f, 0.f, 0.f}, n, b, t, {0.f, 1.f} },
    { { 0.5f, 0.f, 0.f}, n, b, t, {1.f, 1.f} },
    { { 0.5f, 1.f, 0.f}, n, b, t, {1.f, 0.f} },
    { {-0.5f, 1.f, 0.f}, n, b, t, {0.f, 0.f} },
  };
  const uint32_t idx[6] = { 0, 1, 2, 0, 2, 3 };
  return std::make_unique<GeometryData>(video_, 4, verts, 2, idx);
}

void FoliageRenderer::Build(abstract::VideoDevice* video,
                             const terrain::TerrainData& terrain_data,
                             const std::vector<terrain::FoliageLayer*>& layers) {
  Reset();

  video_        = video;
  terrain_data_ = &terrain_data;

  mesh_shader_      = video_->CreateShader("foliage/foliage");
  billboard_shader_ = video_->CreateShader("foliage/foliage_billboard");
  billboard_quad_   = MakeBillboardQuad();

  layers_.reserve(layers.size());
  for (terrain::FoliageLayer* layer : layers) {
    LayerGPU gpu;
    gpu.layer = layer;

    const terrain::FoliageLayerDesc& desc = layer->GetDesc();

    // Load mesh geometry.
    if (!desc.mesh_path.empty()) {
      const auto path = core::Config::GetDataFolder() / desc.mesh_path;
      gpu.geometry = MeshLoader::LoadGeometry(path.string(), video_);
      if (!gpu.geometry)
        LOG_F(WARNING, "FoliageRenderer: failed to load mesh '%s'",
              desc.mesh_path.c_str());
    }

    // Load albedo texture.
    if (!desc.texture_path.empty())
      gpu.albedo_tex = video_->CreateTexture(desc.texture_path);

    // Pre-allocate SSBOs at maximum capacity.
    const int near_bytes = kMaxFoliageInstances * static_cast<int>(sizeof(core::Mat4f));
    const int bill_bytes = kMaxFoliageInstances * static_cast<int>(sizeof(core::Vec4f));
    gpu.near_ssbo      = video_->CreateShaderStorageBuffer(near_bytes, 0);
    gpu.billboard_ssbo = video_->CreateShaderStorageBuffer(bill_bytes, 1);

    layer->MarkDirty();
    layers_.push_back(std::move(gpu));
  }

  LOG_F(INFO, "FoliageRenderer::Build — %d layers", static_cast<int>(layers_.size()));
}

void FoliageRenderer::ClassifyAndUpload(LayerGPU& gpu, const core::Camera& camera) {
  if (gpu.layer->IsDirty())
    gpu.layer->RebuildInstances(*terrain_data_);

  const terrain::FoliageLayerDesc& desc = gpu.layer->GetDesc();
  const core::Vec3f eye = camera.GetPosition();
  const float cull2     = desc.cull_distance      * desc.cull_distance;
  const float bill2     = desc.billboard_distance  * desc.billboard_distance;

  const std::vector<core::Mat4f>& all = gpu.layer->GetInstances();

  // Temporary staging: near matrices + billboard vec4s.
  std::vector<core::Mat4f> near_mats;
  std::vector<core::Vec4f> bill_vecs;
  near_mats.reserve(std::min(static_cast<int>(all.size()), kMaxFoliageInstances));
  bill_vecs.reserve(std::min(static_cast<int>(all.size()), kMaxFoliageInstances));

  for (const core::Mat4f& m : all) {
    const core::Vec3f pos  = ExtractPosition(m);
    const float       dx   = pos.x - eye.x;
    const float       dz   = pos.z - eye.z;
    const float       dist2 = dx*dx + dz*dz;

    if (dist2 > cull2) continue;

    if (dist2 <= bill2) {
      if (static_cast<int>(near_mats.size()) < kMaxFoliageInstances)
        near_mats.push_back(m);
    } else {
      if (static_cast<int>(bill_vecs.size()) < kMaxFoliageInstances) {
        const float s = ExtractScale(m);
        bill_vecs.push_back({pos.x, pos.y, pos.z, s});
      }
    }
  }

  gpu.near_count = static_cast<int>(near_mats.size());
  gpu.bill_count = static_cast<int>(bill_vecs.size());

  if (gpu.near_count > 0)
    gpu.near_ssbo->Fill(near_mats.data(),
                        gpu.near_count * static_cast<int>(sizeof(core::Mat4f)));
  if (gpu.bill_count > 0)
    gpu.billboard_ssbo->Fill(bill_vecs.data(),
                             gpu.bill_count * static_cast<int>(sizeof(core::Vec4f)));
}

void FoliageRenderer::Render(const core::Camera& camera) {
  if (!video_ || !mesh_shader_ || layers_.empty()) return;

  mesh_shader_->Activate();
  video_->SetIndexType(abstract::IndexType::kUInt32);

  for (auto& gpu : layers_) {
    if (!gpu.geometry) continue;

    ClassifyAndUpload(gpu, camera);

    if (gpu.near_count == 0) continue;

    if (gpu.albedo_tex)
      gpu.albedo_tex->Bind(0);

    gpu.near_ssbo->Bind();
    gpu.geometry->Set();
    video_->SetPrimitiveType(abstract::PrimitiveType::kTriangleList);
    video_->RenderIndexedInstanced(gpu.geometry->GetNumIndices(), gpu.near_count);
  }
}

void FoliageRenderer::RenderBillboards(const core::Camera& camera) {
  if (!video_ || !billboard_shader_ || !billboard_quad_ || layers_.empty()) return;

  billboard_shader_->Activate();
  billboard_quad_->Set();
  video_->SetPrimitiveType(abstract::PrimitiveType::kTriangleList);
  video_->SetIndexType(abstract::IndexType::kUInt32);

  for (auto& gpu : layers_) {
    if (gpu.bill_count == 0) continue;

    if (gpu.albedo_tex)
      gpu.albedo_tex->Bind(0);

    gpu.billboard_ssbo->Bind();
    video_->RenderIndexedInstanced(billboard_quad_->GetNumIndices(), gpu.bill_count);
  }
}

void FoliageRenderer::RebuildDirtyLayers() {
  if (!terrain_data_) return;
  for (auto& gpu : layers_) {
    if (gpu.layer && gpu.layer->IsDirty())
      gpu.layer->RebuildInstances(*terrain_data_);
  }
}

}  // namespace renderer
