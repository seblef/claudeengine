#include "environment/WaterRenderer.h"

#include <cstdint>

#include "abstract/BufferUsage.h"
#include "abstract/CompareFunc.h"
#include "abstract/IndexType.h"
#include "core/Vec3f.h"
#include "core/Vertex3D.h"
#include "core/VertexType.h"
#include "environment/WaterInfos.h"

namespace environment {

namespace {
constexpr int   kWaterInfosSlot    = 9;
constexpr int   kWaterInfosFloat4s = sizeof(WaterInfos) / 16;  // 48 / 16 = 3
constexpr float kCellSize          = 10.f;   // world units per grid cell
}  // namespace

void WaterRenderer::Build(abstract::VideoDevice* video,
                          float water_level, int grid_size) {
  video_       = video;
  water_level_ = water_level;
  shader_      = video_->CreateShader("water/water");

  water_cb_ = video_->CreateConstantBuffer(
      kWaterInfosFloat4s, kWaterInfosSlot, abstract::BufferUsage::kDynamic);

  BuildMesh(grid_size);
}

void WaterRenderer::BuildMesh(int grid_size) {
  // The mesh is a flat XZ grid centered at the origin.  The vertex shader
  // displaces Y using sine-wave sums driven by WindInfos (slot 7).
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
      // Normal and tangent-space basis are flat; the VS overwrites them.
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

  // Each quad is split into 2 CW triangles (OpenGL default front face = CCW,
  // but the G-buffer pass uses back-face culling, so winding must be consistent).
  std::vector<uint16_t> indices;
  indices.reserve(static_cast<size_t>(num_quads) * 6);

  for (int j = 0; j < grid_size; ++j) {
    for (int i = 0; i < grid_size; ++i) {
      const uint16_t tl = static_cast<uint16_t>( j      * verts_per_side + i    );
      const uint16_t tr = static_cast<uint16_t>( j      * verts_per_side + i + 1);
      const uint16_t bl = static_cast<uint16_t>((j + 1) * verts_per_side + i    );
      const uint16_t br = static_cast<uint16_t>((j + 1) * verts_per_side + i + 1);
      // Triangle 1: tl, br, tr
      indices.push_back(tl);
      indices.push_back(br);
      indices.push_back(tr);
      // Triangle 2: tl, bl, br
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

void WaterRenderer::Render([[maybe_unused]] const core::Camera& camera) {
  if (!shader_) return;

  WaterInfos wi;
  wi.water_level  = water_level_;
  wi.sky_zenith_r = sky_zenith_r_;
  wi.sky_zenith_g = sky_zenith_g_;
  wi.sky_zenith_b = sky_zenith_b_;
  wi.sun_dir_x    = sun_direction_.x;
  wi.sun_dir_y    = sun_direction_.y;
  wi.sun_dir_z    = sun_direction_.z;

  water_cb_->Bind();
  water_cb_->Fill(&wi);

  video_->SetDepthTestEnabled(true);
  video_->SetDepthWriteEnabled(true);

  shader_->Activate();
  grid_vb_->Bind();
  grid_ib_->Bind();
  video_->RenderIndexed(num_indices_);
}

void WaterRenderer::Reset() {
  if (shader_) {
    shader_->Release();
    shader_ = nullptr;
  }
  water_cb_.reset();
  grid_vb_.reset();
  grid_ib_.reset();
  video_     = nullptr;
  num_indices_ = 0;
}

}  // namespace environment
