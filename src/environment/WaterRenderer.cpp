#include "environment/WaterRenderer.h"

#include <cmath>
#include <cstdint>
#include <vector>

#include <loguru.hpp>

#include "abstract/BufferUsage.h"
#include "abstract/CompareFunc.h"
#include "abstract/IndexType.h"
#include "abstract/RenderTarget.h"
#include "core/Vec3f.h"
#include "core/Vertex3D.h"
#include "core/VertexType.h"

namespace environment {

namespace {

constexpr float kCellSize      = 10.f;   // world units per grid cell
constexpr int   kNormalMapSize = 512;    // resolution of the procedural normal map
constexpr int   kFoamTexSize   = 256;    // resolution of the procedural foam texture

// Returns the height of an overlapping multi-frequency sine field at (u, v).
// u and v are in [0, kNormalMapSize) and represent normalised tile coordinates.
float NormalMapHeight(float u, float v) {
  const float pi2 = 6.28318530f;
  const float s   = pi2 / static_cast<float>(kNormalMapSize);
  // Four overlapping sine waves at different frequencies and angles.
  return  0.40f * std::sin(u * s * 3.f + v * s * 1.f)
        + 0.25f * std::sin(u * s * 7.f - v * s * 5.f)
        + 0.20f * std::sin(u * s * 2.f + v * s * 11.f)
        + 0.15f * std::sin(u * s * 13.f + v * s * 3.f);
}

// Returns a foam intensity in [0,1] at texel (u,v) by taking the product of four
// overlapping sine terms.  Peaks only occur where all four waves coincide, producing
// sparse, isolated blobby bright patches that tile seamlessly.
float FoamValue(float u, float v) {
  const float pi2 = 6.28318530f;
  const float s   = pi2 / static_cast<float>(kFoamTexSize);
  const float a   = (std::sin(u * s * 3.f + v * s * 2.f) + 1.0f) * 0.5f;
  const float b   = (std::sin(u * s * 7.f - v * s * 5.f) + 1.0f) * 0.5f;
  const float c   = (std::sin(u * s * 5.f + v * s * 9.f) + 1.0f) * 0.5f;
  const float d   = (std::sin(u * s * 11.f - v * s * 3.f) + 1.0f) * 0.5f;
  return a * b * c * d;
}

}  // namespace

void WaterRenderer::Build(abstract::VideoDevice* video,
                          float water_level,
                          float terrain_world_width,
                          float terrain_world_height,
                          int grid_size) {
  video_                = video;
  water_level_          = water_level;
  terrain_world_width_  = terrain_world_width;
  terrain_world_height_ = terrain_world_height;
  shader_               = video_->CreateShader("water/water");

  BuildMesh(grid_size);
  BuildNormalMap();
  BuildFoamTexture();
}

void WaterRenderer::BuildMesh(int grid_size) {
  // When terrain dimensions are supplied, fit the mesh to the terrain:
  //   centre the grid at the terrain's midpoint and extend it by
  //   kTerrainMarginFactor so the water visibly exceeds the terrain edges.
  float center_x = 0.f;
  float center_z = 0.f;
  float half      = 0.f;

  if (terrain_world_width_ > 0.f && terrain_world_height_ > 0.f) {
    center_x  = terrain_world_width_  * 0.5f;
    center_z  = terrain_world_height_ * 0.5f;
    const float terrain_half = std::max(terrain_world_width_,
                                        terrain_world_height_) * 0.5f;
    half      = terrain_half * kTerrainMarginFactor;
    grid_size = static_cast<int>(std::ceil(half * 2.f / kCellSize));
    LOG_F(INFO, "WaterRenderer: terrain %.0fx%.0f m → water center=(%.0f,%.0f) "
          "half=%.0f grid=%d", terrain_world_width_, terrain_world_height_,
          center_x, center_z, half, grid_size);
  } else {
    half = static_cast<float>(grid_size) * kCellSize * 0.5f;
    LOG_F(INFO, "WaterRenderer: no terrain — fixed grid=%d half=%.0f",
          grid_size, half);
  }

  const int verts_per_side = grid_size + 1;
  const int num_verts      = verts_per_side * verts_per_side;
  const int num_quads      = grid_size * grid_size;

  std::vector<core::Vertex3D> verts;
  verts.reserve(static_cast<size_t>(num_verts));

  for (int j = 0; j <= grid_size; ++j) {
    for (int i = 0; i <= grid_size; ++i) {
      const float x = center_x - half + static_cast<float>(i) * kCellSize;
      const float z = center_z - half + static_cast<float>(j) * kCellSize;
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
  // uint32 indices are required: large terrains exceed the uint16 limit of 65535
  // vertices (e.g. a 984-quad grid has 985² = 970 225 vertices).
  std::vector<uint32_t> indices;
  indices.reserve(static_cast<size_t>(num_quads) * 6);

  for (int j = 0; j < grid_size; ++j) {
    for (int i = 0; i < grid_size; ++i) {
      const uint32_t tl = static_cast<uint32_t>( j      * verts_per_side + i    );
      const uint32_t tr = static_cast<uint32_t>( j      * verts_per_side + i + 1);
      const uint32_t bl = static_cast<uint32_t>((j + 1) * verts_per_side + i    );
      const uint32_t br = static_cast<uint32_t>((j + 1) * verts_per_side + i + 1);
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
      abstract::IndexType::kUInt32, num_indices_,
      abstract::BufferUsage::kImmutable, indices.data());
}

void WaterRenderer::BuildNormalMap() {
  constexpr int   size  = kNormalMapSize;
  constexpr float scale = 8.f;  // normal contrast multiplier
  constexpr float eps   = 1.f;  // finite-difference step in texel units

  std::vector<uint8_t> pixels;
  pixels.resize(static_cast<size_t>(size * size * 4));

  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      // Wrap-around finite differences for tileability.
      const float fx = static_cast<float>(x);
      const float fy = static_cast<float>(y);

      const float dhdx = (NormalMapHeight(fx + eps, fy) -
                          NormalMapHeight(fx - eps, fy)) / (2.f * eps);
      const float dhdz = (NormalMapHeight(fx, fy + eps) -
                          NormalMapHeight(fx, fy - eps)) / (2.f * eps);

      // Surface normal from height field finite differences.
      const float nx = -dhdx * scale;
      const float ny = 1.f;
      const float nz = -dhdz * scale;
      const float len = std::sqrt(nx * nx + ny * ny + nz * nz);

      const int idx = (y * size + x) * 4;
      // Encode: R = N.x*0.5+0.5, G = N.z*0.5+0.5, B = N.y*0.5+0.5
      pixels[static_cast<size_t>(idx + 0)] =
          static_cast<uint8_t>((nx / len) * 127.5f + 127.5f);
      pixels[static_cast<size_t>(idx + 1)] =
          static_cast<uint8_t>((nz / len) * 127.5f + 127.5f);
      pixels[static_cast<size_t>(idx + 2)] =
          static_cast<uint8_t>((ny / len) * 127.5f + 127.5f);
      pixels[static_cast<size_t>(idx + 3)] = 255;
    }
  }

  normal_map_tex_ = video_->CreateTileableTexture(size, size, pixels.data());
}

void WaterRenderer::BuildFoamTexture() {
  constexpr int size = kFoamTexSize;

  std::vector<uint8_t> pixels(static_cast<size_t>(size * size * 4));
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      // Boost sparse bright peaks into visible range, then clamp to [0,1].
      float val = std::min(1.0f, FoamValue(static_cast<float>(x),
                                           static_cast<float>(y)) * 6.0f);
      const auto  b8  = static_cast<uint8_t>(val * 255.0f);
      const int   idx = (y * size + x) * 4;
      pixels[static_cast<size_t>(idx + 0)] = b8;
      pixels[static_cast<size_t>(idx + 1)] = b8;
      pixels[static_cast<size_t>(idx + 2)] = b8;
      pixels[static_cast<size_t>(idx + 3)] = 255;
    }
  }

  foam_tex_ = video_->CreateTileableTexture(size, size, pixels.data());
}

void WaterRenderer::Render([[maybe_unused]] const core::Camera& camera,
                           abstract::RenderTarget* scene_color,
                           abstract::RenderTarget* depth) {
  if (!shader_) return;

  normal_map_tex_->Bind(0);
  foam_tex_->Bind(1);
  scene_color->BindAsSampler(2);
  depth->BindAsSampler(3);

  video_->SetDepthTestEnabled(true);
  video_->SetDepthFunc(abstract::CompareFunc::kLessEqual);
  video_->SetDepthWriteEnabled(false);

  shader_->Activate();
  grid_vb_->Bind();
  grid_ib_->Bind();
  video_->RenderIndexed(num_indices_);

  video_->UnbindSampler(1);
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
  normal_map_tex_.reset();
  foam_tex_.reset();
  video_     = nullptr;
  num_indices_ = 0;
}

}  // namespace environment
