#include "renderer/TreeRenderer.h"

#include <cmath>
#include <random>

#include <loguru.hpp>

#include "abstract/BufferUsage.h"
#include "abstract/PrimitiveType.h"
#include "abstract/IndexType.h"
#include "abstract/Texture.h"
#include "core/Camera.h"
#include "core/Config.h"
#include "core/Vec3f.h"
#include "core/Vertex3D.h"
#include "environment/TreeLayer.h"
#include "renderer/MeshLoader.h"
#include "terrain/TerrainData.h"

namespace renderer {

TreeRenderer::~TreeRenderer() {
  Reset();
}

void TreeRenderer::Reset() {
  for (auto& gpu : layers_) {
    if (gpu.billboard_tex) {
      gpu.billboard_tex->Release();
      gpu.billboard_tex = nullptr;
    }
  }
  layers_.clear();
  all_instances_.clear();
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

std::unique_ptr<GeometryData> TreeRenderer::MakeBillboardQuad() const {
  const core::Vec3f n(0.f, 0.f, 1.f);
  const core::Vec3f t(1.f, 0.f, 0.f);
  const core::Vec3f b(0.f, 1.f, 0.f);
  const core::Vertex3D verts[4] = {
    { {-0.5f, 0.f, 0.f}, n, b, t, {0.f, 1.f} },
    { { 0.5f, 0.f, 0.f}, n, b, t, {1.f, 1.f} },
    { { 0.5f, 1.f, 0.f}, n, b, t, {1.f, 0.f} },
    { {-0.5f, 1.f, 0.f}, n, b, t, {0.f, 0.f} },
  };
  const uint16_t idx[6] = { 0, 1, 2, 0, 2, 3 };
  return std::make_unique<GeometryData>(video_, 4, verts, 2, idx);
}

void TreeRenderer::GenerateInstances(const LayerGPU& gpu,
                                      const terrain::TerrainData& terrain,
                                      std::vector<core::Vec4f>& out) {
  const environment::TreeLayerDesc& desc = gpu.layer->GetDesc();
  const float world_w = terrain.GetWorldWidth();
  const float world_d = terrain.GetWorldHeight();
  const float cell    = desc.spacing_max;
  if (cell <= 0.f || world_w <= 0.f || world_d <= 0.f) return;

  const int map_w = gpu.layer->GetMapWidth();
  const int map_h = gpu.layer->GetMapHeight();
  const std::vector<uint8_t>& density = gpu.layer->GetDensityMap();

  std::mt19937 rng(137u);
  std::uniform_real_distribution<float> jitter(0.f, 1.f);
  std::uniform_real_distribution<float> scale_dist(desc.scale_min, desc.scale_max);

  const int cols = static_cast<int>(world_w / cell) + 1;
  const int rows = static_cast<int>(world_d / cell) + 1;

  for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < cols; ++col) {
      const float wx = col * cell + jitter(rng) * cell;
      const float wz = row * cell + jitter(rng) * cell;
      if (wx >= world_w || wz >= world_d) continue;

      const float u  = wx / world_w;
      const float v  = wz / world_d;
      const int   tx = static_cast<int>(u * static_cast<float>(map_w - 1) + 0.5f);
      const int   tz = static_cast<int>(v * static_cast<float>(map_h - 1) + 0.5f);
      const uint8_t d = density[tz * map_w + tx];
      if (d == 0u) continue;
      if (jitter(rng) * 255.f > static_cast<float>(d)) continue;

      const float y     = terrain.GetHeight(wx, wz);
      const float scale = scale_dist(rng);
      out.push_back({wx, y, wz, scale});
    }
  }
}

void TreeRenderer::Build(abstract::VideoDevice* video,
                          const terrain::TerrainData& terrain_data,
                          const std::vector<environment::TreeLayer*>& layers) {
  Reset();
  video_        = video;
  terrain_data_ = &terrain_data;

  mesh_shader_      = video_->CreateShader("trees/tree");
  billboard_shader_ = video_->CreateShader("trees/tree_billboard");
  billboard_quad_   = MakeBillboardQuad();

  layers_.reserve(layers.size());
  all_instances_.reserve(layers.size());

  for (environment::TreeLayer* layer : layers) {
    LayerGPU gpu;
    gpu.layer      = layer;
    gpu.trunk_sway = layer->GetDesc().trunk_sway_strength;
    gpu.leaf_sway  = layer->GetDesc().leaf_sway_strength;

    // Load full-resolution mesh.
    const environment::TreeLayerDesc& desc = layer->GetDesc();
    if (!desc.mesh_path.empty()) {
      const auto path = core::Config::GetDataFolder() / desc.mesh_path;
      gpu.geometry = MeshLoader::LoadGeometry(path.string(), video_);
      if (gpu.geometry) {
        const core::BBox3& bb = gpu.geometry->GetBBox();
        const float h = bb.GetMax().y - bb.GetMin().y;
        gpu.mesh_height = (h > 0.f) ? h : 1.f;
      } else {
        LOG_F(WARNING, "TreeRenderer: failed to load mesh '%s'",
              desc.mesh_path.c_str());
      }
    }

    // Load billboard texture.
    if (!desc.billboard_path.empty())
      gpu.billboard_tex = video_->CreateTexture(desc.billboard_path);

    // Pre-allocate SSBOs (binding 2 = near, binding 3 = billboard).
    const int bytes = kMaxTreeInstances * static_cast<int>(sizeof(core::Vec4f));
    gpu.near_ssbo      = video_->CreateShaderStorageBuffer(bytes, 2);
    gpu.billboard_ssbo = video_->CreateShaderStorageBuffer(bytes, 3);

    // Scatter instance positions immediately from the density map.
    std::vector<core::Vec4f> instances;
    GenerateInstances(gpu, terrain_data, instances);
    all_instances_.push_back(std::move(instances));

    layers_.push_back(std::move(gpu));
  }

  LOG_F(INFO, "TreeRenderer::Build — %d layers", static_cast<int>(layers_.size()));
}

void TreeRenderer::ClassifyAndUpload(LayerGPU& gpu, const core::Camera& camera) {
  const int idx = static_cast<int>(&gpu - layers_.data());
  const std::vector<core::Vec4f>& all = all_instances_[idx];

  const environment::TreeLayerDesc& desc = gpu.layer->GetDesc();
  const core::Vec3f eye = camera.GetPosition();
  const float cull2 = desc.cull_distance     * desc.cull_distance;
  const float bill2 = desc.billboard_distance * desc.billboard_distance;

  std::vector<core::Vec4f> near_vecs;
  std::vector<core::Vec4f> bill_vecs;
  near_vecs.reserve(std::min(static_cast<int>(all.size()), kMaxTreeInstances));
  bill_vecs.reserve(std::min(static_cast<int>(all.size()), kMaxTreeInstances));

  for (const core::Vec4f& inst : all) {
    const float dx    = inst.x - eye.x;
    const float dz    = inst.z - eye.z;
    const float dist2 = dx*dx + dz*dz;
    if (dist2 > cull2) continue;

    if (dist2 <= bill2) {
      if (static_cast<int>(near_vecs.size()) < kMaxTreeInstances)
        near_vecs.push_back(inst);
    } else {
      if (static_cast<int>(bill_vecs.size()) < kMaxTreeInstances)
        bill_vecs.push_back(inst);
    }
  }

  gpu.near_count = static_cast<int>(near_vecs.size());
  gpu.bill_count = static_cast<int>(bill_vecs.size());

  if (gpu.near_count > 0)
    gpu.near_ssbo->Fill(near_vecs.data(),
                        gpu.near_count * static_cast<int>(sizeof(core::Vec4f)));
  if (gpu.bill_count > 0)
    gpu.billboard_ssbo->Fill(bill_vecs.data(),
                             gpu.bill_count * static_cast<int>(sizeof(core::Vec4f)));
}

void TreeRenderer::Render(const core::Camera& camera) {
  if (!video_ || !mesh_shader_ || layers_.empty()) return;

  mesh_shader_->Activate();

  for (auto& gpu : layers_) {
    if (!gpu.geometry) continue;

    ClassifyAndUpload(gpu, camera);
    if (gpu.near_count == 0) continue;

    // Upload per-layer sway uniforms and mesh height for vertex_y_norm.
    mesh_shader_->SetUniformFloat("u_trunk_sway",  gpu.trunk_sway);
    mesh_shader_->SetUniformFloat("u_leaf_sway",   gpu.leaf_sway);
    mesh_shader_->SetUniformFloat("u_mesh_height", gpu.mesh_height);

    // Bind billboard texture as albedo for the G-buffer pass.
    // Trees typically share the same texture for near and far.
    if (gpu.billboard_tex)
      gpu.billboard_tex->Bind(0);

    gpu.near_ssbo->Bind();
    gpu.geometry->Set();
    video_->SetPrimitiveType(abstract::PrimitiveType::kTriangleList);
    video_->SetIndexType(abstract::IndexType::kUInt16);
    video_->RenderIndexedInstanced(gpu.geometry->GetNumIndices(), gpu.near_count);
  }
}

void TreeRenderer::RenderBillboards(const core::Camera& camera) {
  if (!video_ || !billboard_shader_ || !billboard_quad_ || layers_.empty()) return;

  billboard_shader_->Activate();
  billboard_quad_->Set();
  video_->SetPrimitiveType(abstract::PrimitiveType::kTriangleList);
  video_->SetIndexType(abstract::IndexType::kUInt16);

  for (auto& gpu : layers_) {
    if (gpu.bill_count == 0) continue;

    if (gpu.billboard_tex)
      gpu.billboard_tex->Bind(0);

    gpu.billboard_ssbo->Bind();
    video_->RenderIndexedInstanced(billboard_quad_->GetNumIndices(), gpu.bill_count);
  }
}

}  // namespace renderer
