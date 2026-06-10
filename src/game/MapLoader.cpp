#include "game/MapLoader.h"

#include <fstream>
#include <string>
#include <vector>

#include <loguru.hpp>
#include <yaml-cpp/yaml.h>

#include "core/Config.h"
#include "core/CoordinateSystem.h"
#include "core/Mat4f.h"
#include "core/ProjectionType.h"
#include "core/YamlUtils.h"
#include "game/GameCamera.h"
#include "game/GameLight.h"
#include "game/GameMaterial.h"
#include "game/GameMesh.h"
#include "game/GameParticleSystem.h"
#include "game/GamePlayerStart.h"
#include "game/GameTerrain.h"
#include "game/MeshTemplate.h"
#include "particles/ParticleSystemTemplate.h"
#include "renderer/GeometryUtils.h"
#include "renderer/Light.h"
#include "renderer/MaterialDesc.h"
#include "terrain/FoliageLayer.h"
#include "terrain/TerrainData.h"
#include "terrain/TerrainMaterial.h"

namespace game {

namespace {

environment::EnvironmentDesc ParseEnvironmentDesc(const YAML::Node& node) {
  environment::EnvironmentDesc desc;
  if (!node) return desc;

  if (node["sky_enabled"])
    desc.sky_enabled = node["sky_enabled"].as<bool>(desc.sky_enabled);
  if (node["water_enabled"])
    desc.water_enabled = node["water_enabled"].as<bool>(desc.water_enabled);
  if (node["cloud_enabled"])
    desc.cloud_enabled = node["cloud_enabled"].as<bool>(desc.cloud_enabled);
  if (node["wind_enabled"])
    desc.wind_enabled = node["wind_enabled"].as<bool>(desc.wind_enabled);
  if (node["trees_enabled"])
    desc.trees_enabled = node["trees_enabled"].as<bool>(desc.trees_enabled);
  if (node["water_level"])
    desc.water_level = node["water_level"].as<float>(desc.water_level);
  if (node["water_color"] && node["water_color"].IsSequence() &&
      node["water_color"].size() >= 3) {
    desc.water_color_r = node["water_color"][0].as<float>(desc.water_color_r);
    desc.water_color_g = node["water_color"][1].as<float>(desc.water_color_g);
    desc.water_color_b = node["water_color"][2].as<float>(desc.water_color_b);
  }
  if (node["roughness"])
    desc.roughness = node["roughness"].as<float>(desc.roughness);
  if (node["sun_intensity"])
    desc.sun_intensity = node["sun_intensity"].as<float>(desc.sun_intensity);
  if (node["refraction_strength"])
    desc.refraction_strength =
        node["refraction_strength"].as<float>(desc.refraction_strength);
  if (node["absorption_scale"])
    desc.absorption_scale =
        node["absorption_scale"].as<float>(desc.absorption_scale);
  if (node["foam_height_thresh"])
    desc.foam_height_thresh =
        node["foam_height_thresh"].as<float>(desc.foam_height_thresh);
  if (node["foam_shoreline_depth"])
    desc.foam_shoreline_depth =
        node["foam_shoreline_depth"].as<float>(desc.foam_shoreline_depth);
  if (node["foam_steepness_thresh"])
    desc.foam_steepness_thresh =
        node["foam_steepness_thresh"].as<float>(desc.foam_steepness_thresh);
  if (node["foam_speed"])
    desc.foam_speed = node["foam_speed"].as<float>(desc.foam_speed);
  if (node["normal_scale1"])
    desc.normal_scale1 = node["normal_scale1"].as<float>(desc.normal_scale1);
  if (node["normal_scale2"])
    desc.normal_scale2 = node["normal_scale2"].as<float>(desc.normal_scale2);
  if (node["normal_scroll_speed1"])
    desc.normal_scroll_speed1 =
        node["normal_scroll_speed1"].as<float>(desc.normal_scroll_speed1);
  if (node["normal_scroll_speed2"])
    desc.normal_scroll_speed2 =
        node["normal_scroll_speed2"].as<float>(desc.normal_scroll_speed2);
  if (node["start_time_of_day"])
    desc.start_time_of_day = node["start_time_of_day"].as<float>(desc.start_time_of_day);
  if (node["time_scale"])
    desc.time_scale = node["time_scale"].as<float>(desc.time_scale);
  if (node["cloud_density"])
    desc.cloud_density = node["cloud_density"].as<float>(desc.cloud_density);
  if (node["wind_strength"])
    desc.wind_strength = node["wind_strength"].as<float>(desc.wind_strength);
  if (node["wind_direction"]) {
    desc.wind_direction = core::ParseVec3(node["wind_direction"],
                                          desc.wind_direction).Normalized();
  }
  if (node["turbidity"])
    desc.turbidity = node["turbidity"].as<float>(desc.turbidity);
  if (node["moon_texture"])
    desc.moon_texture = node["moon_texture"].as<std::string>("");
  if (node["night_sky_texture"])
    desc.night_sky_texture = node["night_sky_texture"].as<std::string>("");
  return desc;
}

GameLightDesc ParseGlobalLightDesc(const YAML::Node& node) {
  GameLightDesc desc;
  if (!node) return desc;

  if (node["direction"])
    desc.direction = core::ParseVec3(node["direction"], desc.direction).Normalized();
  if (node["color"])
    desc.color = core::ParseColor(node["color"], desc.color);
  if (node["intensity"])
    desc.intensity = node["intensity"].as<float>(desc.intensity);
  if (node["ambient_color"])
    desc.ambient_color = core::ParseVec3(node["ambient_color"], desc.ambient_color);
  if (node["cast_shadow"])
    desc.cast_shadow = node["cast_shadow"].as<bool>(desc.cast_shadow);
  if (node["shadow_resolution"])
    desc.shadow_resolution = node["shadow_resolution"].as<int>(desc.shadow_resolution);
  if (node["shadow_bias"])
    desc.shadow_bias = node["shadow_bias"].as<float>(desc.shadow_bias);
  return desc;
}

std::string StripMaterialPath(const std::string& raw) {
  std::string name = raw;
  if (name.rfind("materials/", 0) == 0)
    name = name.substr(10);
  if (name.size() >= 5 && name.substr(name.size() - 5) == ".yaml")
    name.resize(name.size() - 5);
  return name;
}

GameMaterial* LoadMaterialWithFallback(const std::string& raw,
                                       abstract::VideoDevice* video) {
  const std::string name = StripMaterialPath(raw);
  GameMaterial* mat = GameMaterial::GetOrLoad(name, video);
  if (!mat) {
    LOG_F(WARNING, "MapLoader: material '%s' failed to load, using default",
          name.c_str());
    mat = new GameMaterial("__default_" + name, renderer::MaterialDesc{}, video);
  }
  return mat;
}

std::unique_ptr<GameObject> ParseMesh(const YAML::Node& node,
                                      abstract::VideoDevice* video) {
  const std::string name      = node["name"].as<std::string>("Mesh");
  const std::string mesh_path = node["mesh"].as<std::string>("");
  const std::string mat_raw   = node["material"].as<std::string>("");
  const core::Mat4f transform = core::ParseMat4(node["transform"]);

  GameMaterial* mat = nullptr;
  if (!mat_raw.empty()) {
    mat = LoadMaterialWithFallback(mat_raw, video);
  } else {
    mat = new GameMaterial("__default_mesh", renderer::MaterialDesc{}, video);
  }

  MeshTemplate* tmpl = nullptr;
  if (!mesh_path.empty()) {
    const std::string full_path =
        (core::Config::GetDataFolder() / mesh_path).string();
    tmpl = MeshTemplate::GetOrLoad(full_path, video, mat);
  }

  if (!tmpl) {
    LOG_F(WARNING, "MapLoader: mesh '%s' failed to load, using unit-cube fallback",
          mesh_path.c_str());
    auto geo = renderer::CreateCubeMesh(video);
    tmpl = new MeshTemplate("__proc_cube_" + name, std::move(geo), mat);
  }
  mat->Release();  // tmpl holds its own ref

  auto mesh = std::make_unique<GameMesh>(tmpl);
  tmpl->Release();  // GameMesh holds its own ref

  mesh->SetName(name);
  mesh->SetWorldTransform(transform);
  return mesh;
}

renderer::LightType ParseLightType(const std::string& s) {
  if (s == "omni")        return renderer::LightType::kOmni;
  if (s == "circle_spot") return renderer::LightType::kCircleSpot;
  if (s == "rect_spot")   return renderer::LightType::kRectSpot;
  return renderer::LightType::kGlobal;
}

std::unique_ptr<GameObject> ParseLight(const YAML::Node& node) {
  const std::string name      = node["name"].as<std::string>("Light");
  const std::string type_str  = node["light_type"].as<std::string>("global");
  const core::Mat4f transform = core::ParseMat4(node["transform"]);

  const renderer::LightType light_type = ParseLightType(type_str);

  GameLightDesc desc;
  if (node["color"])
    desc.color = core::ParseColor(node["color"], desc.color);
  if (node["intensity"])
    desc.intensity = node["intensity"].as<float>(desc.intensity);
  if (node["radius"])
    desc.radius = node["radius"].as<float>(desc.radius);
  if (node["cast_shadow"])
    desc.cast_shadow = node["cast_shadow"].as<bool>(desc.cast_shadow);
  if (node["shadow_resolution"])
    desc.shadow_resolution = node["shadow_resolution"].as<int>(desc.shadow_resolution);
  if (node["shadow_bias"])
    desc.shadow_bias = node["shadow_bias"].as<float>(desc.shadow_bias);

  auto light = std::make_unique<GameLight>(light_type, desc);
  light->SetName(name);
  light->SetWorldTransform(transform);
  return light;
}

std::unique_ptr<GameObject> ParseCamera(const YAML::Node& node) {
  const std::string name      = node["name"].as<std::string>("Camera");
  const core::Mat4f transform = core::ParseMat4(node["transform"]);

  auto camera = std::make_unique<GameCamera>(
      core::ProjectionType::kPerspective,
      core::CoordinateSystem::kRightHanded);
  camera->SetName(name);
  camera->SetWorldTransform(transform);
  return camera;
}

std::unique_ptr<GameObject> ParsePlayerStart(const YAML::Node& node) {
  const std::string name      = node["name"].as<std::string>("PlayerStart");
  const core::Mat4f transform = core::ParseMat4(node["transform"]);

  auto ps = std::make_unique<GamePlayerStart>();
  ps->SetName(name);
  ps->SetWorldTransform(transform);
  return ps;
}

std::unique_ptr<GameObject> ParseParticleSystem(const YAML::Node& node,
                                               abstract::VideoDevice* video) {
  const std::string tmpl_name = node["template"].as<std::string>("");
  auto* tmpl = particles::ParticleSystemTemplate::GetOrLoad(tmpl_name, video);
  if (!tmpl) {
    LOG_F(WARNING, "MapLoader: particle template '%s' not found", tmpl_name.c_str());
    return nullptr;
  }
  auto go = std::make_unique<GameParticleSystem>(tmpl, video);
  tmpl->Release();
  go->SetName(node["name"].as<std::string>("ParticleSystem"));
  go->SetWorldTransform(core::ParseMat4(node["transform"]));
  return go;
}

std::unique_ptr<GameObject> ParseTerrain(const YAML::Node& node,
                                         const std::filesystem::path& map_path,
                                         abstract::VideoDevice* video) {
  const std::string hm_file = node["heightmap"].as<std::string>("");
  const int   width  = node["heightmap_width"].as<int>(0);
  const int   height = node["heightmap_height"].as<int>(0);
  const float mpt    = node["meters_per_texel"].as<float>(1.f);
  const float min_h  = node["min_height"].as<float>(0.f);
  const float max_h  = node["max_height"].as<float>(100.f);

  if (hm_file.empty() || width <= 0 || height <= 0) {
    LOG_F(WARNING, "MapLoader: invalid terrain section (missing heightmap or dimensions)");
    return nullptr;
  }

  // Read .r16 (raw little-endian uint16_t, row-major) from the map's directory.
  const std::filesystem::path r16_path = map_path.parent_path() / hm_file;
  const size_t expected_bytes =
      static_cast<size_t>(width) * height * sizeof(uint16_t);
  std::vector<uint16_t> hm_data(static_cast<size_t>(width) * height);
  {
    std::ifstream r16(r16_path, std::ios::binary);
    if (!r16) {
      LOG_F(ERROR, "MapLoader: cannot open heightmap '%s'",
            r16_path.string().c_str());
      return nullptr;
    }
    r16.read(reinterpret_cast<char*>(hm_data.data()),
             static_cast<std::streamsize>(expected_bytes));
    if (static_cast<size_t>(r16.gcount()) != expected_bytes) {
      LOG_F(ERROR, "MapLoader: heightmap '%s' truncated (%zu / %zu bytes)",
            r16_path.string().c_str(),
            static_cast<size_t>(r16.gcount()), expected_bytes);
      return nullptr;
    }
  }

  auto terrain_data = std::make_unique<terrain::TerrainData>(
      hm_data.data(), width, height, mpt, min_h, max_h);
  // Data was just read from the on-disk .r16; the file is already up-to-date.
  terrain_data->ClearDirty();

  auto terrain_material = std::make_unique<terrain::TerrainMaterial>();
  if (node["material"])
    terrain_material->Load(node["material"], video, width, height);

  auto gt = std::make_unique<GameTerrain>(
      std::move(terrain_data), std::move(terrain_material), video);
  gt->SetName("terrain");

  // Parse optional foliage_layers sequence.
  if (node["foliage_layers"]) {
    for (const YAML::Node& fn : node["foliage_layers"]) {
      const std::string dm_file = fn["density_map"].as<std::string>("");
      const int dm_w = fn["density_width"].as<int>(0);
      const int dm_h = fn["density_height"].as<int>(0);

      terrain::FoliageLayerDesc desc;
      desc.name               = fn["name"].as<std::string>("");
      desc.mesh_path          = fn["mesh"].as<std::string>("");
      desc.texture_path       = fn["texture"].as<std::string>("");
      desc.spacing_min        = fn["spacing_min"].as<float>(0.5f);
      desc.spacing_max        = fn["spacing_max"].as<float>(1.5f);
      desc.scale_min          = fn["scale_min"].as<float>(0.8f);
      desc.scale_max          = fn["scale_max"].as<float>(1.2f);
      desc.cull_distance      = fn["cull_distance"].as<float>(80.f);
      desc.billboard_distance = fn["billboard_distance"].as<float>(40.f);

      auto flayer = std::make_unique<terrain::FoliageLayer>(
          dm_w > 0 ? dm_w : width,
          dm_h > 0 ? dm_h : height,
          std::move(desc));

      // Load density map from raw .r8 file.
      if (!dm_file.empty() && dm_w > 0 && dm_h > 0) {
        const std::filesystem::path dm_path = map_path.parent_path() / dm_file;
        std::ifstream dm_stream(dm_path, std::ios::binary);
        if (dm_stream) {
          auto& dm = flayer->GetDensityMap();
          dm_stream.read(reinterpret_cast<char*>(dm.data()),
                         static_cast<std::streamsize>(dm.size()));
          flayer->MarkDirty();
          LOG_F(INFO, "MapLoader: loaded foliage density '%s' (%dx%d)",
                dm_file.c_str(), dm_w, dm_h);
        } else {
          LOG_F(WARNING, "MapLoader: cannot open foliage density '%s'",
                dm_file.c_str());
        }
      }

      gt->AddFoliageLayer(std::move(flayer));
    }
  }

  return gt;
}

}  // namespace

// static
MapData MapLoader::Load(const std::filesystem::path& path,
                        abstract::VideoDevice* video) {
  MapData result;

  YAML::Node root;
  try {
    root = core::LoadYamlFile(path);
  } catch (const std::exception& e) {
    LOG_F(ERROR, "MapLoader: failed to parse '%s': %s",
          path.string().c_str(), e.what());
    return result;
  }

  result.name     = root["name"].as<std::string>("");
  result.map_size = root["map_size"].as<float>(120.f);

  if (root["environment"])
    result.environment_desc = ParseEnvironmentDesc(root["environment"]);
  if (root["global_light"])
    result.global_light = ParseGlobalLightDesc(root["global_light"]);

  // Terrain is optional; backwards-compatible if the key is absent.
  if (root["terrain"]) {
    try {
      auto gt = ParseTerrain(root["terrain"], path, video);
      if (gt) result.objects.push_back(std::move(gt));
    } catch (const std::exception& e) {
      LOG_F(WARNING, "MapLoader: error parsing terrain: %s", e.what());
    }
  }

  const YAML::Node& objects = root["objects"];
  if (!objects || !objects.IsSequence())
    return result;

  for (const YAML::Node& obj : objects) {
    const std::string type = obj["type"].as<std::string>("");

    try {
      if (type == "mesh") {
        result.objects.push_back(ParseMesh(obj, video));
      } else if (type == "light") {
        result.objects.push_back(ParseLight(obj));
      } else if (type == "camera") {
        result.objects.push_back(ParseCamera(obj));
      } else if (type == "player_start") {
        result.objects.push_back(ParsePlayerStart(obj));
      } else if (type == "particle_system") {
        auto go = ParseParticleSystem(obj, video);
        if (go) result.objects.push_back(std::move(go));
      } else {
        LOG_F(WARNING, "MapLoader: unknown object type '%s', skipping",
              type.c_str());
      }
    } catch (const std::exception& e) {
      LOG_F(WARNING, "MapLoader: error parsing object '%s': %s",
            obj["name"].as<std::string>("?").c_str(), e.what());
    }
  }

  return result;
}

}  // namespace game
