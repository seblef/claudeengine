#include "environment/WaterRenderer.h"

#include <cstdint>

#include "abstract/BufferUsage.h"
#include "abstract/CompareFunc.h"
#include "abstract/IndexType.h"
#include "abstract/RenderTarget.h"
#include "core/Vec3f.h"
#include "core/Vertex3D.h"
#include "core/VertexType.h"

namespace environment {

namespace {
constexpr float kCellSize = 10.f;   // world units per grid cell
}  // namespace

void WaterRenderer::Build(abstract::VideoDevice* video,
                          float water_level, int grid_size) {
  video_       = video;
  water_level_ = water_level;
  shader_      = video_->CreateShader("water/water");

  BuildMesh(grid_size);
}

void WaterRenderer::BuildMesh(int grid_size) {
  const int   verts_per_side = grid_size + 1;
  const int   num_verts      = verts_per_side * verts_per_side;
  const int   num_quads      = grid_size * grid_size;
  const float half           = grid_size * kCellSize * 0.5f;

  std::vector<core::Vertex3D> verts;
  verts.reserve(static_cast<size_t>(num_verts));

  for (int j = 0; j <= grid_size; ++j) {
    for (int i = 0; i <= grid_size; ++i) {
      const float x = -half + static_cast<float>(i) * kCellSize;
      const float z = -half + static_cast<float>(j) * kCellSize;
      verts.push_back({
        core::Vec3f(x, 0.f, z),
        core::Vec3f(0.f, 1.f, 0.f),
        core::Vec3f(1.f, 0.f, 0.f),
        core::Vec3f(0.f, 0.f, 1.f),
        core::Vec2f(static_cast<float>(i) / static_cast<float>(grid_size),
                    static_cast<float>(j) / static_cast<float>(grid_size)),
      });
    }
  }

  // Each quad → 2 CW triangles (consistent with the geometry pass winding).
  std::vector<uint16_t> indices;
  indices.reserve(static_cast<size_t>(num_quads) * 6);

  for (int j = 0; j < grid_size; ++j) {
    for (int i = 0; i < grid_size; ++i) {
      const uint16_t tl = static_cast<uint16_t>( j      * verts_per_side + i    );
      const uint16_t tr = static_cast<uint16_t>( j      * verts_per_side + i + 1);
      const uint16_t bl = static_cast<uint16_t>((j + 1) * verts_per_side + i    );
      const uint16_t br = static_cast<uint16_t>((j + 1) * verts_per_side + i + 1);
      indices.push_back(tl);
      indices.push_back(br);
      indices.push_back(tr);
      indices.push_back(tl);
      indices.push_back(bl);
      indices.push_back(br);
    }
  }

  num_indices_ = static_cast<int>(indices.size());

  grid_vb_ = video_->CreateVertexBuffer(
      core::VertexType::k3D, num_verts,
      abstract::BufferUsage::kImmutable, verts.data());
  grid_ib_ = video_->CreateIndexBuffer(
      abstract::IndexType::kUInt16, num_indices_,
      abstract::BufferUsage::kImmutable, indices.data());
}

void WaterRenderer::Render([[maybe_unused]] const core::Camera& camera,
                           abstract::RenderTarget* scene_color,
                           abstract::RenderTarget* depth) {
  if (!shader_) return;

  scene_color->BindAsSampler(2);
  depth->BindAsSampler(3);

  video_->SetDepthTestEnabled(true);
  video_->SetDepthFunc(abstract::CompareFunc::kLessEqual);
  video_->SetDepthWriteEnabled(false);

  shader_->Activate();
  grid_vb_->Bind();
  grid_ib_->Bind();
  video_->RenderIndexed(num_indices_);

  video_->UnbindSampler(2);
  video_->UnbindSampler(3);
}

void WaterRenderer::Reset() {
  if (shader_) {
    shader_->Release();
    shader_ = nullptr;
  }
  grid_vb_.reset();
  grid_ib_.reset();
  video_     = nullptr;
  num_indices_ = 0;
}

}  // namespace environment
