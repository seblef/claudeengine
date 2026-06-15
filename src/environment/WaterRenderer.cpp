#include "environment/WaterRenderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <loguru.hpp>

#include "abstract/BlendFactor.h"
#include "abstract/BufferUsage.h"
#include "abstract/CompareFunc.h"
#include "abstract/IndexType.h"
#include "abstract/RenderTarget.h"
#include "abstract/RenderTargetGroup.h"
#include "abstract/TextureFormat.h"
#include "core/Color.h"
#include "core/Config.h"
#include "core/Mat4f.h"
#include "core/Vec3f.h"
#include "core/Vertex3D.h"
#include "core/VertexType.h"

namespace environment {

namespace {

constexpr int   kFoamTexSize    = 256;    // resolution of the procedural foam texture
constexpr int   kCausticTexSize = 256;    // resolution of the procedural caustic texture
constexpr int   kTileSize       = 8;      // grid cells per tile side for frustum culling

// LOD ring specs: { cell_size_m, inner_radius_m, outer_radius_m }.
// outer_radius for LOD 2 is filled at Build() time from terrain dimensions.
constexpr float kLodSpecs[3][2] = {
    { 10.f,   0.f },   // LOD 0: near  disc,   0 – 200 m
    { 20.f, 200.f },   // LOD 1: mid   ring, 200 – 500 m
    { 40.f, 500.f },   // LOD 2: far   ring, 500 m – terrain margin
};
constexpr float kLod0OuterRadius = 200.f;
constexpr float kLod1OuterRadius = 500.f;

// Returns a caustic intensity in [0,1] at texel (u,v) by summing cosine rings
// emanating from several source points (toroidal distance for tileability).
// Values above the threshold become bright veins; below is dark background.
float CausticValue(float u, float v) {
  struct Source { float cx; float cy; float freq; float phase; };
  // Six sources at irregular positions scaled to kCausticTexSize.
  static constexpr Source kSources[] = {
      {  51.2f,  51.2f, 0.15f, 0.00f },
      { 204.8f,  76.8f, 0.17f, 1.20f },
      { 128.0f, 204.8f, 0.13f, 2.40f },
      {  76.8f, 153.6f, 0.19f, 0.80f },
      { 179.2f, 153.6f, 0.12f, 3.00f },
      {  38.4f, 217.6f, 0.16f, 1.80f },
  };
  constexpr float kHalf = kCausticTexSize * 0.5f;
  float sum = 0.f;
  for (const auto& src : kSources) {
    float dx = u - src.cx;
    float dz = v - src.cy;
    // Toroidal wrap: keep dx/dz in (-half, +half] so the pattern tiles.
    if (dx >  kHalf) dx -= kCausticTexSize;
    if (dx < -kHalf) dx += kCausticTexSize;
    if (dz >  kHalf) dz -= kCausticTexSize;
    if (dz < -kHalf) dz += kCausticTexSize;
    const float dist = std::sqrt(dx * dx + dz * dz);
    sum += std::cos(dist * src.freq + src.phase);
  }
  // Threshold: only the upper fraction of the interference peak becomes bright.
  // sum range ≈ [-6, 6]; values > 4 mapped to [0, 1].
  constexpr float kThresh = 4.0f;
  constexpr float kScale  = 1.0f / (6.0f - kThresh);
  return std::max(0.f, (sum - kThresh) * kScale);
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
  screen_w_             = video_->GetWidth();
  screen_h_             = video_->GetHeight();
  shader_               = video_->CreateShader("water/water");
  ssr_shader_           = video_->CreateShader("water/water_ssr");

  // Compute LOD 2 outer radius from terrain dimensions or grid_size fallback.
  float lod2_outer = std::max(1000.f,
      static_cast<float>(grid_size) * kLodSpecs[0][0] * 0.5f);
  if (terrain_world_width_ > 0.f && terrain_world_height_ > 0.f) {
    const float terrain_half = std::max(terrain_world_width_,
                                        terrain_world_height_) * 0.5f;
    lod2_outer = terrain_half * kTerrainMarginFactor;
  }

  const float outer_radii[3] = { kLod0OuterRadius, kLod1OuterRadius, lod2_outer };

  for (int i = 0; i < 3; ++i) {
    LodRing& ring       = lod_rings_[i];
    ring.cell_size      = kLodSpecs[i][0];
    ring.inner_radius   = kLodSpecs[i][1];
    ring.outer_radius   = outer_radii[i];
    ring.last_snap      = {-1e9f, -1e9f};
    ring.num_indices    = 0;
    ring.tiles.clear();

    // Worst-case counts for a square bounding the ring (all quads included).
    const int side       = static_cast<int>(
        std::ceil(2.5f * ring.outer_radius / ring.cell_size));
    const int max_verts  = (side + 1) * (side + 1);
    const int max_idx    = side * side * 6;

    ring.vb = video_->CreateVertexBuffer(
        core::VertexType::k3D, max_verts, abstract::BufferUsage::kDynamic);
    ring.ib = video_->CreateIndexBuffer(
        abstract::IndexType::kUInt32, max_idx, abstract::BufferUsage::kDynamic);
  }

  normal_map_tex_  = LoadNormalMap("");
  normal_map_tex2_ = LoadNormalMap("");
  BuildFoamTexture();
  BuildCausticTexture();
  BuildSsrTarget();
}

void WaterRenderer::BuildSsrTarget() {
  ssr_fbo_.reset();
  ssr_rt_.reset();

  const int hw = std::max(1, screen_w_ / 2);
  const int hh = std::max(1, screen_h_ / 2);
  ssr_rt_ = video_->CreateRenderTarget(hw, hh, abstract::TextureFormat::kRGBA16F);

  std::array<abstract::RenderTarget*, 1> colors = {ssr_rt_.get()};
  ssr_fbo_ = video_->CreateRenderTargetGroup(colors, nullptr);
}

void WaterRenderer::Resize(int w, int h) {
  screen_w_ = w;
  screen_h_ = h;
  if (video_) BuildSsrTarget();
}

void WaterRenderer::BuildRingGeometry(LodRing& ring, core::Vec2f snap_pos) {
  // Guard band on the inner radius: a quad's centre and the adjacent ring's
  // quad centre covering the same world point can be up to
  // (cell_inner + cell_outer)*sqrt(2)/2 ≈ cell_size*sqrt(2) apart.
  // Shrinking the inner exclusion by 2*cell_size guarantees the two rings
  // always overlap at boundaries, preventing holes.
  const float inner_eff  = (ring.inner_radius > ring.cell_size * 2.f)
                           ? ring.inner_radius - ring.cell_size * 2.f : 0.f;
  const float inner2     = inner_eff * inner_eff;
  const float outer2     = ring.outer_radius * ring.outer_radius;
  const int   half_cells = static_cast<int>(
      std::ceil(ring.outer_radius / ring.cell_size));
  const int   side           = half_cells * 2;
  const int   verts_per_side = side + 1;

  // Vertex grid origin (world space) aligned to snap_pos.
  const float x0 = snap_pos.x - static_cast<float>(half_cells) * ring.cell_size;
  const float z0 = snap_pos.y - static_cast<float>(half_cells) * ring.cell_size;

  std::vector<core::Vertex3D> verts;
  verts.reserve(static_cast<size_t>(verts_per_side * verts_per_side));

  for (int j = 0; j <= side; ++j) {
    for (int i = 0; i <= side; ++i) {
      const float wx = x0 + static_cast<float>(i) * ring.cell_size;
      const float wz = z0 + static_cast<float>(j) * ring.cell_size;
      // UV = world XZ; the vertex shader overrides v_uv with in_position.xz.
      verts.push_back({
          core::Vec3f(wx, 0.f, wz),
          core::Vec3f(0.f, 1.f, 0.f),
          core::Vec3f(1.f, 0.f, 0.f),
          core::Vec3f(0.f, 0.f, 1.f),
          core::Vec2f(wx, wz),
      });
    }
  }

  // Indices in tile-major order for per-tile frustum culling.
  // Only quads whose XZ centre lies in [inner_radius, outer_radius] are emitted.
  std::vector<uint32_t> indices;
  indices.reserve(static_cast<size_t>(side * side * 6));

  ring.tiles.clear();
  for (int tj = 0; tj < side; tj += kTileSize) {
    for (int ti = 0; ti < side; ti += kTileSize) {
      const int cell_x1 = std::min(ti + kTileSize, side);
      const int cell_z1 = std::min(tj + kTileSize, side);

      TileInfo tile;
      tile.first_index = static_cast<int>(indices.size());

      for (int j = tj; j < cell_z1; ++j) {
        for (int i = ti; i < cell_x1; ++i) {
          const float cx  = x0 + (static_cast<float>(i) + 0.5f) * ring.cell_size;
          const float cz  = z0 + (static_cast<float>(j) + 0.5f) * ring.cell_size;
          const float ddx = cx - snap_pos.x;
          const float ddz = cz - snap_pos.y;
          const float d2  = ddx * ddx + ddz * ddz;
          if (d2 < inner2 || d2 > outer2) continue;

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

      tile.index_count = static_cast<int>(indices.size()) - tile.first_index;
      if (tile.index_count == 0) continue;

      tile.x0 = x0 + static_cast<float>(ti)      * ring.cell_size;
      tile.z0 = z0 + static_cast<float>(tj)      * ring.cell_size;
      tile.x1 = x0 + static_cast<float>(cell_x1) * ring.cell_size;
      tile.z1 = z0 + static_cast<float>(cell_z1) * ring.cell_size;
      ring.tiles.push_back(tile);
    }
  }

  ring.num_indices = static_cast<int>(indices.size());
  if (!verts.empty())
    ring.vb->Fill(verts.data(), static_cast<int>(verts.size()));
  if (!indices.empty())
    ring.ib->Fill(indices.data(), ring.num_indices);
}

std::unique_ptr<abstract::RawTexture> WaterRenderer::LoadNormalMap(
    const std::string& path) {
  if (!path.empty()) {
    const std::filesystem::path full =
        core::Config::GetDataFolder() / "textures" / path;
    int w = 0, h = 0;
    std::vector<uint8_t> pixels;
    if (video_->LoadRGBA8File(full, &w, &h, pixels)) {
      return video_->CreateTileableTexture(w, h, pixels.data());
    }
    LOG_F(WARNING, "WaterRenderer: failed to load normal map '%s', using flat fallback",
          path.c_str());
  }
  // 1×1 flat normal pointing straight up: R=128, G=128, B=255, A=255.
  const uint8_t flat[4] = {128, 128, 255, 255};
  return video_->CreateTileableTexture(1, 1, flat);
}

void WaterRenderer::SetNormalMapTextures(const std::string& path1,
                                         const std::string& path2) {
  if (video_) {
    normal_map_tex_  = LoadNormalMap(path1);
    normal_map_tex2_ = LoadNormalMap(path2);
  }
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

void WaterRenderer::BuildCausticTexture() {
  constexpr int size = kCausticTexSize;

  std::vector<uint8_t> pixels(static_cast<size_t>(size * size * 4));
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      const float val = CausticValue(static_cast<float>(x), static_cast<float>(y));
      const auto  b8  = static_cast<uint8_t>(val * 255.0f);
      const int   idx = (y * size + x) * 4;
      pixels[static_cast<size_t>(idx + 0)] = b8;
      pixels[static_cast<size_t>(idx + 1)] = b8;
      pixels[static_cast<size_t>(idx + 2)] = b8;
      pixels[static_cast<size_t>(idx + 3)] = 255;
    }
  }

  caustic_tex_ = video_->CreateTileableTexture(size, size, pixels.data());
}

void WaterRenderer::Render(const core::Camera& camera,
                           abstract::RenderTarget* scene_color,
                           abstract::RenderTarget* depth,
                           abstract::RenderTargetGroup* output_fbo) {
  if (!shader_ || !ssr_rt_) return;

  // ── Frustum culling: project view frustum planes onto the water plane ──────
  // A 3D plane (A, B, C, D) — with Ax+By+Cz+D >= 0 meaning "inside" — reduces
  // to the 2D half-plane  A*x + C*z + (B*water_level_ + D) >= 0  on XZ.
  // Uses the same Gribb-Hartmann row combinations as core::ViewFrustum.
  struct Plane2D { float a, b, d; };  // a*x + b*z + d >= 0 means inside
  Plane2D planes2d[6];
  int  n_planes2d = 0;
  bool skip_all   = false;
  const core::Mat4f vp = camera.GetViewProjectionMatrix();
  const auto add_plane = [&](float A, float B, float C, float D) {
    const float d2d = B * water_level_ + D;
    if (std::abs(A) < 1e-6f && std::abs(C) < 1e-6f) {
      if (d2d < 0.f) skip_all = true;
    } else {
      planes2d[n_planes2d++] = { A, C, d2d };
    }
  };
  add_plane(vp(3,0)+vp(0,0), vp(3,1)+vp(0,1), vp(3,2)+vp(0,2), vp(3,3)+vp(0,3));  // left
  add_plane(vp(3,0)-vp(0,0), vp(3,1)-vp(0,1), vp(3,2)-vp(0,2), vp(3,3)-vp(0,3));  // right
  add_plane(vp(3,0)+vp(1,0), vp(3,1)+vp(1,1), vp(3,2)+vp(1,2), vp(3,3)+vp(1,3));  // bottom
  add_plane(vp(3,0)-vp(1,0), vp(3,1)-vp(1,1), vp(3,2)-vp(1,2), vp(3,3)-vp(1,3));  // top
  add_plane(vp(3,0)+vp(2,0), vp(3,1)+vp(2,1), vp(3,2)+vp(2,2), vp(3,3)+vp(2,3));  // near
  add_plane(vp(3,0)-vp(2,0), vp(3,1)-vp(2,1), vp(3,2)-vp(2,2), vp(3,3)-vp(2,3));  // far
  if (skip_all) return;

  // ── Ring snap + rebuild (once per frame; shared by both passes) ───────────
  const core::Vec3f cam_pos  = camera.GetPosition();
  const core::Vec2f cam_xz   = {cam_pos.x, cam_pos.z};

  // All rings share the same snap centre (LOD 0 grid) so every ring's radial
  // inclusion test measures from an identical world-space point, preventing
  // gaps at ring boundaries when rings would otherwise snap independently.
  const float fine_cell = lod_rings_[0].cell_size;
  const core::Vec2f snap_pos(std::round(cam_xz.x / fine_cell) * fine_cell,
                              std::round(cam_xz.y / fine_cell) * fine_cell);

  const float moved_x  = snap_pos.x - lod_rings_[0].last_snap.x;
  const float moved_z  = snap_pos.y - lod_rings_[0].last_snap.y;
  const float half_cell = fine_cell * 0.5f;
  if (moved_x * moved_x + moved_z * moved_z >= half_cell * half_cell) {
    for (LodRing& ring : lod_rings_) {
      BuildRingGeometry(ring, snap_pos);
      ring.last_snap = snap_pos;
    }
  }

  // Shared draw loop used by both passes.
  const auto DrawRings = [&]() {
    for (LodRing& ring : lod_rings_) {
      if (ring.num_indices == 0) continue;

      ring.vb->Bind();
      ring.ib->Bind();

      for (const TileInfo& tile : ring.tiles) {
        bool visible = true;
        for (int pi = 0; pi < n_planes2d && visible; ++pi) {
          const Plane2D& p = planes2d[pi];
          // Cull tile only when all 4 XZ corners are behind this half-plane.
          const float v00 = p.a * tile.x0 + p.b * tile.z0 + p.d;
          const float v10 = p.a * tile.x1 + p.b * tile.z0 + p.d;
          const float v01 = p.a * tile.x0 + p.b * tile.z1 + p.d;
          const float v11 = p.a * tile.x1 + p.b * tile.z1 + p.d;
          if (v00 < 0.f && v10 < 0.f && v01 < 0.f && v11 < 0.f)
            visible = false;
        }
        if (visible)
          video_->RenderIndexed(tile.index_count, tile.first_index);
      }
    }
  };

  // ── Pass 1: half-resolution SSR ──────────────────────────────────────────
  ssr_fbo_->BindForWriting();
  video_->SetViewport(0, 0, screen_w_ / 2, screen_h_ / 2);
  video_->ClearRenderTargets(core::Color(0.f, 0.f, 0.f, 0.f));
  video_->SetDepthTestEnabled(false);
  video_->SetDepthWriteEnabled(false);
  video_->SetBlendEnabled(false);
  video_->SetIndexType(abstract::IndexType::kUInt32);

  scene_color->BindAsSampler(2);
  depth->BindAsSampler(3);
  ssr_shader_->Activate();
  DrawRings();

  video_->UnbindSampler(2);
  video_->UnbindSampler(3);

  // ── Pass 2: full-resolution main water pass ───────────────────────────────
  output_fbo->BindForWriting();
  video_->SetViewport(0, 0, screen_w_, screen_h_);
  video_->SetDepthTestEnabled(true);
  video_->SetDepthFunc(abstract::CompareFunc::kLessEqual);
  video_->SetDepthWriteEnabled(false);
  video_->SetBlendEnabled(true, abstract::BlendFactor::kSrcAlpha,
                          abstract::BlendFactor::kOneMinusSrcAlpha);

  normal_map_tex_->Bind(0);
  foam_tex_->Bind(1);
  scene_color->BindAsSampler(2);
  depth->BindAsSampler(3);
  ssr_rt_->BindAsSampler(kSsrSlot);
  normal_map_tex2_->Bind(kNormalMap2Slot);
  shader_->Activate();
  DrawRings();

  video_->UnbindSampler(0);
  video_->UnbindSampler(1);
  video_->UnbindSampler(2);
  video_->UnbindSampler(3);
  video_->UnbindSampler(kSsrSlot);
  video_->UnbindSampler(kNormalMap2Slot);
}

void WaterRenderer::SetWaterLevel(float y) {
  water_level_ = y;
}

void WaterRenderer::Reset() {
  if (shader_) {
    shader_->Release();
    shader_ = nullptr;
  }
  if (ssr_shader_) {
    ssr_shader_->Release();
    ssr_shader_ = nullptr;
  }
  ssr_fbo_.reset();
  ssr_rt_.reset();
  for (LodRing& ring : lod_rings_) {
    ring.vb.reset();
    ring.ib.reset();
    ring.num_indices = 0;
    ring.tiles.clear();
    ring.last_snap = {-1e9f, -1e9f};
  }
  normal_map_tex_.reset();
  normal_map_tex2_.reset();
  foam_tex_.reset();
  caustic_tex_.reset();
  video_ = nullptr;
}

}  // namespace environment
