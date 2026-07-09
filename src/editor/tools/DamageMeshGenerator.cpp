#include "editor/tools/DamageMeshGenerator.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <loguru.hpp>
#include <stb_image.h>
#include <stb_image_write.h>
#include <stb_perlin.h>
#include <yaml-cpp/yaml.h>

#include "core/BBox3.h"
#include "core/Config.h"
#include "core/Vec2f.h"
#include "core/Vec3f.h"
#include "core/Vertex3D.h"
#include "core/YamlSerialiser.h"
#include "mesh/EmeshReader.h"
#include "mesh/EmeshWriter.h"
#include "mesh/LodData.h"
#include "mesh/MeshData.h"
#include "mesh/MeshUtils.h"
#include "mesh/SubMeshRange.h"

namespace editor {

namespace {

constexpr float kMaxDisplacementFraction = 0.3f;
constexpr int   kNoiseOctaves            = 3;
constexpr float kNoiseLacunarity         = 2.1f;
constexpr float kNoisePersistence        = 0.5f;

constexpr float kDarkenStrength = 0.55f;
constexpr float kDesatStrength  = 0.6f;
constexpr float kRustStrength   = 0.35f;

// Seed-dependent domain offsets so distinct seeds visibly diverge even though
// stb_perlin_noise3_seed truncates the seed to a single byte per octave. Each
// offset is derived from a disjoint slice of bits of the (full-range,
// mt19937-produced) seed and then scaled, keeping every offset in the low
// thousands: mesh-local positions are O(1) metres, and stb_perlin internally
// float-to-int casts its input (see stb__perlin_fastfloor), which is
// undefined behaviour once the offset is large enough that adding a small
// position to it no longer changes the float value at all — verified
// empirically that a naive offset = seed * constant (seed being a full
// 32-bit value) lands around 1e11-1e12, silently collapsing every vertex to
// the same NaN noise sample.
constexpr float kSeedScaleX = 12.71f;
constexpr float kSeedScaleY = 7.47f;
constexpr float kSeedScaleZ = 31.17f;

// Fractal-sum 3D noise, roughly in [-1, 1]. Mirrors terrain::TerrainGenerator's
// FBM approach (see src/terrain/TerrainGenerator.cpp) applied to mesh-local
// positions instead of a heightfield.
float Fbm3(const core::Vec3f& p, uint32_t seed) {
  const float ox = static_cast<float>(seed & 0xFFFu) * kSeedScaleX;          // 0-4095
  const float oy = static_cast<float>((seed >> 12) & 0xFFFu) * kSeedScaleY;  // 0-4095
  const float oz = static_cast<float>((seed >> 24) & 0xFFu) * kSeedScaleZ;   // 0-255

  float amplitude = 1.f;
  float frequency = 1.f;
  float sum = 0.f;
  float norm = 0.f;

  for (int o = 0; o < kNoiseOctaves; ++o) {
    const int octave_seed = static_cast<int>((seed + static_cast<uint32_t>(o) * 73u) & 0xFFu);
    sum += amplitude * stb_perlin_noise3_seed(
        (p.x + ox) * frequency, (p.y + oy) * frequency, (p.z + oz) * frequency,
        0, 0, 0, octave_seed);
    norm += amplitude;
    amplitude *= kNoisePersistence;
    frequency *= kNoiseLacunarity;
  }

  return norm > 0.f ? sum / norm : 0.f;
}

// Soft per-vertex "zone extremity" weight in [0,1]: 0 near the body centre,
// approaching 1 toward whichever axis (normalised by half_extents) the point
// is most displaced along. Continuous analogue of
// game::VehicleDamage::ClassifyZone()'s per-axis-normalized, dominant-axis
// classification, without collapsing to a single discrete zone.
float ComputeZoneWeight(const core::Vec3f& local_pos, const core::Vec3f& half_extents) {
  const float nx = std::fabs(local_pos.x / half_extents.x);
  const float ny = std::fabs(local_pos.y / half_extents.y);
  const float nz = std::fabs(local_pos.z / half_extents.z);
  return std::clamp(std::max({nx, ny, nz}), 0.f, 1.f);
}

// Displaces every vertex inward along its (pre-displacement) normal by a
// severity- and zone-weighted noise amount, then recomputes normals/tangents
// and the AABB. Writes the per-vertex zone weight used (needed again to
// rasterize the matching texture damage mask) into *vertex_weights.
void DisplaceMesh(mesh::LodData* lod, const DamageGenerationParams& params,
                   std::vector<float>* vertex_weights) {
  const float avg_half_extent =
      (params.half_extents.x + params.half_extents.y + params.half_extents.z) / 3.f;
  const float base_frequency = 1.6f / std::max(avg_half_extent, 0.1f);
  const float max_displacement = kMaxDisplacementFraction * avg_half_extent;

  vertex_weights->resize(lod->vertices.size());

  for (size_t i = 0; i < lod->vertices.size(); ++i) {
    core::Vertex3D& v = lod->vertices[i];
    const float weight = ComputeZoneWeight(v.position, params.half_extents);
    (*vertex_weights)[i] = weight;

    const float noise = Fbm3(v.position * base_frequency, params.noise_seed);
    // Guard against a non-finite noise sample (e.g. a future domain-offset
    // regression) corrupting this vertex to NaN, which would silently poison
    // the whole mesh's AABB and make it invisible rather than just undamaged.
    const float n01 = std::isfinite(noise) ? std::clamp(noise * 0.5f + 0.5f, 0.f, 1.f) : 0.f;
    const float amount = params.severity * weight * n01 * max_displacement;

    const core::Vec3f normal = v.normal.LengthSquared() > 1e-8f
        ? v.normal.Normalized() : core::Vec3f::kAxisY;
    v.position -= normal * amount;
  }

  mesh::ComputeNormals(lod);
  mesh::ComputeTangents(lod);

  if (!lod->vertices.empty()) {
    core::BBox3 aabb(lod->vertices.front().position, lod->vertices.front().position);
    for (const auto& v : lod->vertices) aabb << v.position;
    lod->aabb = aabb;
  }
}

// Returns true and fills *out_name when every submesh shares one non-empty
// material name — the common case for today's single-material car bodies.
// Mixed-material meshes skip texture generation (mesh-only variant) since
// there is no single diffuse texture to composite onto.
bool GetCommonMaterialName(const mesh::LodData& lod, std::string* out_name) {
  if (lod.submeshes.empty() || lod.submeshes.front().material_name.empty()) return false;
  const std::string& first = lod.submeshes.front().material_name;
  const bool all_same = std::all_of(
      lod.submeshes.begin(), lod.submeshes.end(),
      [&first](const mesh::SubMeshRange& sm) { return sm.material_name == first; });
  if (!all_same) return false;
  *out_name = first;
  return true;
}

// Rasterizes the per-vertex zone weight into UV space via barycentric
// interpolation (max-blended across overlapping/seam triangles), producing a
// width*height mask that lines up with the mesh's own zone-weighted damage.
void RasterizeZoneMask(const mesh::LodData& lod, const std::vector<float>& vertex_weights,
                        int width, int height, std::vector<float>* mask) {
  mask->assign(static_cast<size_t>(width) * static_cast<size_t>(height), 0.f);

  auto edge = [](const core::Vec2f& a, const core::Vec2f& b, const core::Vec2f& c) {
    return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
  };

  for (size_t t = 0; t + 2 < lod.indices.size(); t += 3) {
    const uint32_t i0 = lod.indices[t];
    const uint32_t i1 = lod.indices[t + 1];
    const uint32_t i2 = lod.indices[t + 2];

    const core::Vec2f p0{lod.vertices[i0].uv.x * static_cast<float>(width - 1),
                          (1.f - lod.vertices[i0].uv.y) * static_cast<float>(height - 1)};
    const core::Vec2f p1{lod.vertices[i1].uv.x * static_cast<float>(width - 1),
                          (1.f - lod.vertices[i1].uv.y) * static_cast<float>(height - 1)};
    const core::Vec2f p2{lod.vertices[i2].uv.x * static_cast<float>(width - 1),
                          (1.f - lod.vertices[i2].uv.y) * static_cast<float>(height - 1)};

    float area = edge(p0, p1, p2);
    if (std::fabs(area) < 1e-6f) continue;
    const float sign = area < 0.f ? -1.f : 1.f;
    area *= sign;

    const float w0v = vertex_weights[i0];
    const float w1v = vertex_weights[i1];
    const float w2v = vertex_weights[i2];

    const int min_x = std::clamp(static_cast<int>(std::floor(std::min({p0.x, p1.x, p2.x}))), 0, width - 1);
    const int max_x = std::clamp(static_cast<int>(std::ceil(std::max({p0.x, p1.x, p2.x}))), 0, width - 1);
    const int min_y = std::clamp(static_cast<int>(std::floor(std::min({p0.y, p1.y, p2.y}))), 0, height - 1);
    const int max_y = std::clamp(static_cast<int>(std::ceil(std::max({p0.y, p1.y, p2.y}))), 0, height - 1);

    for (int y = min_y; y <= max_y; ++y) {
      for (int x = min_x; x <= max_x; ++x) {
        const core::Vec2f p{static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f};
        const float e0 = edge(p1, p2, p) * sign;
        const float e1 = edge(p2, p0, p) * sign;
        const float e2 = edge(p0, p1, p) * sign;
        if (e0 < 0.f || e1 < 0.f || e2 < 0.f) continue;

        const float weight = (e0 * w0v + e1 * w1v + e2 * w2v) / area;
        float& cell = (*mask)[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)];
        cell = std::max(cell, weight);
      }
    }
  }
}

// Darkens/desaturates/rust-tints pixels in-place under the mask, modulated by
// procedural grime + elongated "scratch" streak noise so the result isn't a
// perfectly smooth gradient.
void CompositeDamageTexture(const std::vector<float>& mask, int width, int height,
                             float severity, uint32_t seed, std::vector<uint8_t>* pixels) {
  const core::Vec3f rust_tint{0.35f, 0.18f, 0.08f};

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const size_t idx = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
      const float zone_weight = mask[idx];
      if (zone_weight <= 0.f) continue;

      const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(width);
      const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(height);

      const float streak = stb_perlin_noise3_seed(
          u * 45.f, v * 6.f, 0.f, 0, 0, 0, static_cast<int>((seed + 17u) & 0xFFu)) * 0.5f + 0.5f;
      const float grime = stb_perlin_noise3_seed(
          u * 9.f, v * 9.f, 0.f, 0, 0, 0, static_cast<int>((seed + 41u) & 0xFFu)) * 0.5f + 0.5f;

      const float m = std::clamp(
          std::max(zone_weight * severity * (0.5f + 0.5f * grime),
                   zone_weight * severity * streak * 0.6f),
          0.f, 1.f);
      if (m <= 0.001f) continue;

      uint8_t* px = &(*pixels)[idx * 4];
      float r = static_cast<float>(px[0]) / 255.f;
      float g = static_cast<float>(px[1]) / 255.f;
      float b = static_cast<float>(px[2]) / 255.f;

      const float luma = 0.299f * r + 0.587f * g + 0.114f * b;
      r += (luma - r) * (m * kDesatStrength);
      g += (luma - g) * (m * kDesatStrength);
      b += (luma - b) * (m * kDesatStrength);

      r *= (1.f - m * kDarkenStrength);
      g *= (1.f - m * kDarkenStrength);
      b *= (1.f - m * kDarkenStrength);

      const float rust_mix = m * kRustStrength;
      r += (rust_tint.x - r) * rust_mix;
      g += (rust_tint.y - g) * rust_mix;
      b += (rust_tint.z - b) * rust_mix;

      px[0] = static_cast<uint8_t>(std::clamp(r, 0.f, 1.f) * 255.f + 0.5f);
      px[1] = static_cast<uint8_t>(std::clamp(g, 0.f, 1.f) * 255.f + 0.5f);
      px[2] = static_cast<uint8_t>(std::clamp(b, 0.f, 1.f) * 255.f + 0.5f);
    }
  }
}

// Attempts to build a damaged material + texture pair for base_material_name.
// Returns false (mesh-only fallback) when the material has no diffuse
// texture, or that texture has no uncompressed .png source alongside its
// .dds — see DamageMeshGenerator::Generate()'s doc comment.
bool TryGenerateDamagedMaterial(const std::string& base_material_name,
                                 const std::string& variant_id,
                                 const DamageGenerationParams& params,
                                 const std::vector<float>& vertex_weights,
                                 const mesh::LodData& lod,
                                 std::string* out_new_material_name) {
  const std::filesystem::path data_dir = core::Config::GetDataFolder();
  const std::filesystem::path material_path = data_dir / "materials" / (base_material_name + ".yaml");

  YAML::Node root;
  try {
    root = core::LoadYamlFile(material_path);
  } catch (const std::exception& e) {
    LOG_F(WARNING, "DamageMeshGenerator: cannot read material '%s': %s",
          material_path.string().c_str(), e.what());
    return false;
  }

  const YAML::Node rendering = root["rendering"];
  const YAML::Node diffuse_node = rendering ? rendering["textures"]["diffuse"] : YAML::Node();
  if (!diffuse_node || !diffuse_node.IsScalar()) {
    LOG_F(INFO, "DamageMeshGenerator: material '%s' has no diffuse texture, skipping texture generation",
          base_material_name.c_str());
    return false;
  }

  const std::string diffuse_rel = diffuse_node.as<std::string>();
  const std::filesystem::path diffuse_rel_path(diffuse_rel);
  const std::filesystem::path source_png = data_dir / "textures" / diffuse_rel_path.parent_path() /
      (diffuse_rel_path.stem().string() + ".png");

  if (!std::filesystem::exists(source_png)) {
    LOG_F(INFO,
          "DamageMeshGenerator: no uncompressed PNG source next to '%s', "
          "skipping texture generation (mesh-only variant)", diffuse_rel.c_str());
    return false;
  }

  int width = 0, height = 0, channels = 0;
  uint8_t* raw = stbi_load(source_png.string().c_str(), &width, &height, &channels, 4);
  if (!raw) {
    LOG_F(WARNING, "DamageMeshGenerator: failed to load '%s'", source_png.string().c_str());
    return false;
  }
  std::vector<uint8_t> pixels(raw, raw + static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
  stbi_image_free(raw);

  std::vector<float> mask;
  RasterizeZoneMask(lod, vertex_weights, width, height, &mask);
  CompositeDamageTexture(mask, width, height, params.severity, params.noise_seed, &pixels);

  const std::filesystem::path tex_out_rel = diffuse_rel_path.parent_path() / "generated" /
      (diffuse_rel_path.stem().string() + "_dmg_" + variant_id + ".png");
  const std::filesystem::path tex_out_abs = data_dir / "textures" / tex_out_rel;
  std::filesystem::create_directories(tex_out_abs.parent_path());

  if (!stbi_write_png(tex_out_abs.string().c_str(), width, height, 4, pixels.data(), width * 4)) {
    LOG_F(WARNING, "DamageMeshGenerator: failed to write '%s'", tex_out_abs.string().c_str());
    return false;
  }

  root["rendering"]["textures"]["diffuse"] = tex_out_rel.generic_string();

  const std::string new_material_name = "generated/" + base_material_name + "_dmg_" + variant_id;
  const std::filesystem::path mat_out_abs = data_dir / "materials" / (new_material_name + ".yaml");
  std::filesystem::create_directories(mat_out_abs.parent_path());

  std::ofstream mat_out(mat_out_abs);
  if (!mat_out) {
    LOG_F(WARNING, "DamageMeshGenerator: failed to write '%s'", mat_out_abs.string().c_str());
    return false;
  }
  mat_out << root;

  *out_new_material_name = new_material_name;
  return true;
}

}  // namespace

// static
DamageGenerationResult DamageMeshGenerator::Generate(const std::string& source_mesh_path,
                                                      const std::string& variant_id,
                                                      const DamageGenerationParams& params) {
  DamageGenerationResult result;

  const std::filesystem::path data_dir = core::Config::GetDataFolder();
  const std::filesystem::path source_abs = data_dir / source_mesh_path;

  mesh::MeshData mesh_data;
  if (!mesh::EmeshReader{}.Read(source_abs.string(), &mesh_data)) {
    LOG_F(ERROR, "DamageMeshGenerator: cannot read source mesh '%s'", source_abs.string().c_str());
    return result;
  }

  std::vector<float> vertex_weights;
  DisplaceMesh(&mesh_data.lod, params, &vertex_weights);

  std::string base_material_name;
  if (GetCommonMaterialName(mesh_data.lod, &base_material_name)) {
    std::string new_material_name;
    if (TryGenerateDamagedMaterial(base_material_name, variant_id, params, vertex_weights,
                                    mesh_data.lod, &new_material_name)) {
      for (mesh::SubMeshRange& sm : mesh_data.lod.submeshes) {
        if (sm.material_name == base_material_name) sm.material_name = new_material_name;
      }
    }
  }

  const std::filesystem::path src_rel(source_mesh_path);
  const std::filesystem::path mesh_out_rel = src_rel.parent_path() / "generated" /
      (src_rel.stem().string() + "_dmg_" + variant_id + src_rel.extension().string());
  const std::filesystem::path mesh_out_abs = data_dir / mesh_out_rel;
  std::filesystem::create_directories(mesh_out_abs.parent_path());

  if (!mesh::EmeshWriter{}.Write(mesh_data, mesh_out_abs.string())) {
    LOG_F(ERROR, "DamageMeshGenerator: failed to write '%s'", mesh_out_abs.string().c_str());
    return result;
  }

  result.success = true;
  result.mesh_path = mesh_out_rel.generic_string();
  return result;
}

}  // namespace editor
