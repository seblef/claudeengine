#include "game/MapLoader.h"

#include <fstream>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <loguru.hpp>
#include <yaml-cpp/yaml.h>

#include "audio/ResourceManager.h"
#include "audio/SoundManager.h"
#include "core/Config.h"
#include "core/CoordinateSystem.h"
#include "core/Mat4f.h"
#include "core/ProjectionType.h"
#include "core/YamlSerialiser.h"
#include "game/GameCamera.h"
#include "game/GameLight.h"
#include "game/GameMaterial.h"
#include "game/GameMesh.h"
#include "game/GameParticleSystem.h"
#include "game/GamePivot.h"
#include "game/GamePlayerStart.h"
#include "game/GameRoad.h"
#include "game/GameSoundEmitter.h"
#include "game/GameTerrain.h"
#include "game/GameVehicle.h"
#include "game/MeshTemplate.h"
#include "game/VehicleTemplate.h"
#include "track/RoadSpline.h"
#include "physics/CollisionLayer.h"
#include "physics/PhysicsBodyDesc.h"
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
  if (node["foam_scale1"])
    desc.foam_scale1 = node["foam_scale1"].as<float>(desc.foam_scale1);
  if (node["foam_scale2"])
    desc.foam_scale2 = node["foam_scale2"].as<float>(desc.foam_scale2);
  if (node["foam_scroll_speed1"])
    desc.foam_scroll_speed1 =
        node["foam_scroll_speed1"].as<float>(desc.foam_scroll_speed1);
  if (node["foam_scroll_speed2"])
    desc.foam_scroll_speed2 =
        node["foam_scroll_speed2"].as<float>(desc.foam_scroll_speed2);
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
  if (node["normal_dir1"] && node["normal_dir1"].IsSequence() &&
      node["normal_dir1"].size() >= 2) {
    desc.normal_dir1_x = node["normal_dir1"][0].as<float>(desc.normal_dir1_x);
    desc.normal_dir1_y = node["normal_dir1"][1].as<float>(desc.normal_dir1_y);
  }
  if (node["normal_dir2"] && node["normal_dir2"].IsSequence() &&
      node["normal_dir2"].size() >= 2) {
    desc.normal_dir2_x = node["normal_dir2"][0].as<float>(desc.normal_dir2_x);
    desc.normal_dir2_y = node["normal_dir2"][1].as<float>(desc.normal_dir2_y);
  }
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
  if (node["normal_map_texture1"])
    desc.normal_map_texture1 = node["normal_map_texture1"].as<std::string>("");
  if (node["normal_map_texture2"])
    desc.normal_map_texture2 = node["normal_map_texture2"].as<std::string>("");
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

std::optional<physics::PhysicsBodyDesc> ParsePhysicsBodyDesc(
    const YAML::Node& node) {
  if (!node) return std::nullopt;

  const std::string shape_str = node["shape"].as<std::string>("box");

  physics::PhysicsShapeDesc shape;
  if (shape_str == "box") {
    const core::Vec3f he =
        core::ParseVec3(node["half_extents"], core::Vec3f(0.5f, 0.5f, 0.5f));
    shape = physics::PhysicsShapeDesc::MakeBox(he);
  } else if (shape_str == "sphere") {
    shape = physics::PhysicsShapeDesc::MakeSphere(
        node["radius"].as<float>(0.5f));
  } else if (shape_str == "cylinder") {
    shape = physics::PhysicsShapeDesc::MakeCylinder(
        node["radius"].as<float>(0.5f), node["half_height"].as<float>(1.0f));
  } else if (shape_str == "capsule") {
    shape = physics::PhysicsShapeDesc::MakeCapsule(
        node["radius"].as<float>(0.5f), node["half_height"].as<float>(1.0f));
  } else if (shape_str == "convex_hull") {
    shape = physics::PhysicsShapeDesc::MakeConvexHull();
  } else if (shape_str == "exact") {
    shape = physics::PhysicsShapeDesc::MakeExact();
  } else {
    LOG_F(WARNING, "MapLoader: unknown physics shape '%s', defaulting to box",
          shape_str.c_str());
    shape = physics::PhysicsShapeDesc::MakeBox(core::Vec3f(0.5f, 0.5f, 0.5f));
  }

  const std::string motion_str =
      node["motion_type"].as<std::string>("static");
  physics::MotionType motion_type = physics::MotionType::Static;
  if (motion_str == "kinematic") {
    motion_type = physics::MotionType::Kinematic;
  } else if (motion_str == "dynamic") {
    motion_type = physics::MotionType::Dynamic;
  } else if (motion_str != "static") {
    LOG_F(WARNING, "MapLoader: unknown motion_type '%s', defaulting to static",
          motion_str.c_str());
  }

  // exact and terrain shapes are static-only — enforce at parse time.
  const bool shape_requires_static =
      (shape.type == physics::PhysicsShapeType::Exact ||
       shape.type == physics::PhysicsShapeType::Terrain);
  if (shape_requires_static && motion_type != physics::MotionType::Static) {
    LOG_F(WARNING,
          "MapLoader: shape '%s' only supports static motion; "
          "forcing MotionType::Static",
          shape_str.c_str());
    motion_type = physics::MotionType::Static;
  }

  physics::PhysicsMaterialDesc material;
  material.friction        = node["friction"].as<float>(material.friction);
  material.restitution     = node["restitution"].as<float>(material.restitution);
  material.mass            = node["mass"].as<float>(material.mass);
  material.linear_damping  =
      node["linear_damping"].as<float>(material.linear_damping);
  material.angular_damping =
      node["angular_damping"].as<float>(material.angular_damping);
  material.gravity_factor  =
      node["gravity_factor"].as<float>(material.gravity_factor);

  shape.center_offset =
      core::ParseVec3(node["center_offset"], core::Vec3f(0.f, 0.f, 0.f));

  physics::PhysicsBodyDesc desc;
  desc.shape           = shape;
  desc.material        = material;
  desc.motion_type     = motion_type;
  const uint16_t default_layer = (motion_type != physics::MotionType::Static)
      ? physics::kLayerDynamic
      : physics::kLayerWorld;
  desc.collision_layer = static_cast<uint16_t>(
      node["collision_layer"].as<int>(default_layer));
  desc.collision_mask  = static_cast<uint16_t>(
      node["collision_mask"].as<int>(0xFFFF));
  return desc;
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

  if (auto desc = ParsePhysicsBodyDesc(node["physics"]))
    mesh->SetPhysicsDesc(*desc);

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

std::unique_ptr<GameObject> ParsePivot(const YAML::Node& node) {
  const std::string name      = node["name"].as<std::string>("Pivot");
  const core::Mat4f transform = core::ParseMat4(node["transform"]);

  auto pivot = std::make_unique<GamePivot>();
  pivot->SetName(name);
  pivot->SetWorldTransform(transform);
  return pivot;
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

std::unique_ptr<GameObject> ParseSoundEmitter(
    const YAML::Node& node,
    audio::SoundManager* sound_manager,
    audio::ResourceManager* resource_manager) {
  const std::string name         = node["name"].as<std::string>("SoundEmitter");
  const std::string sound_name   = node["sound"].as<std::string>("");
  const float       volume_scale = node["volume_scale"].as<float>(1.f);
  const core::Mat4f transform    = core::ParseMat4(node["transform"]);

  if (sound_name.empty()) {
    LOG_F(WARNING, "MapLoader: sound_emitter '%s' has no sound field, skipping",
          name.c_str());
    return nullptr;
  }

  auto emitter = std::make_unique<GameSoundEmitter>(
      sound_name, sound_manager, resource_manager, volume_scale);
  emitter->SetName(name);
  emitter->SetWorldTransform(transform);
  return emitter;
}

std::unique_ptr<GameObject> ParseVehicle(const YAML::Node& node,
                                         abstract::VideoDevice* video) {
  const std::string desc_path = node["desc"].as<std::string>("");
  const std::string name      = node["name"].as<std::string>("Vehicle");
  const core::Mat4f transform = core::ParseMat4(node["transform"]);

  if (desc_path.empty()) {
    LOG_F(WARNING, "MapLoader: vehicle '%s' has no desc field, skipping",
          name.c_str());
    return nullptr;
  }

  VehicleTemplate* tmpl = VehicleTemplate::GetOrLoad(desc_path, video);
  if (!tmpl) {
    LOG_F(WARNING, "MapLoader: failed to load vehicle template '%s' for '%s'",
          desc_path.c_str(), name.c_str());
    return nullptr;
  }

  auto vehicle = std::make_unique<GameVehicle>(tmpl);
  tmpl->Release();

  vehicle->SetName(name);
  vehicle->SetWorldTransform(transform);
  return vehicle;
}

std::unique_ptr<GameObject> ParseRoad(const YAML::Node& node,
                                      abstract::VideoDevice* video) {
  const std::string name            = node["name"].as<std::string>("Road");
  const float       width           = node["width"].as<float>(8.f);
  const float       samples_per_m   = node["samples_per_metre"].as<float>(1.f);
  const std::string mat_raw         = node["material"].as<std::string>("");

  if (!node["control_points"] || !node["control_points"].IsSequence() ||
      node["control_points"].size() < 2) {
    LOG_F(WARNING,
          "MapLoader: road '%s' has fewer than 2 control_points, skipping",
          name.c_str());
    return nullptr;
  }

  auto road = std::make_unique<GameRoad>(video);
  road->SetName(name);
  road->SetWidth(width);
  road->SetSamplesPerMetre(samples_per_m);

  for (const YAML::Node& cp : node["control_points"]) {
    if (cp.IsSequence() && cp.size() >= 3) {
      road->GetSpline().AddControlPoint({
          cp[0].as<float>(0.f),
          cp[1].as<float>(0.f),
          cp[2].as<float>(0.f)});
    }
  }

  if (!mat_raw.empty()) {
    GameMaterial* mat = LoadMaterialWithFallback(mat_raw, video);
    road->SetMaterial(mat);
    mat->Release();
  }

  return road;
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
                        abstract::VideoDevice* video,
                        audio::SoundManager* sound_manager,
                        audio::ResourceManager* resource_manager) {
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

  // Parallel vector of parent names; resolved in a second pass after all
  // objects are instantiated so ordering in the file does not matter.
  std::vector<std::string> parent_names;

  // Terrain is optional; backwards-compatible if the key is absent.
  if (root["terrain"]) {
    try {
      auto gt = ParseTerrain(root["terrain"], path, video);
      if (gt) {
        result.objects.push_back(std::move(gt));
        parent_names.push_back("");
      }
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
      std::unique_ptr<GameObject> go;
      if (type == "mesh") {
        go = ParseMesh(obj, video);
      } else if (type == "light") {
        go = ParseLight(obj);
      } else if (type == "camera") {
        go = ParseCamera(obj);
      } else if (type == "pivot") {
        go = ParsePivot(obj);
      } else if (type == "player_start") {
        go = ParsePlayerStart(obj);
      } else if (type == "particle_system") {
        go = ParseParticleSystem(obj, video);
      } else if (type == "sound_emitter") {
        go = ParseSoundEmitter(obj, sound_manager, resource_manager);
      } else if (type == "vehicle") {
        go = ParseVehicle(obj, video);
      } else if (type == "road") {
        go = ParseRoad(obj, video);
      } else {
        LOG_F(WARNING, "MapLoader: unknown object type '%s', skipping",
              type.c_str());
      }
      if (go) {
        parent_names.push_back(obj["parent"].as<std::string>(""));
        result.objects.push_back(std::move(go));
      }
    } catch (const std::exception& e) {
      LOG_F(WARNING, "MapLoader: error parsing object '%s': %s",
            obj["name"].as<std::string>("?").c_str(), e.what());
    }
  }

  // Second pass: link parent-child relationships by name.
  std::unordered_map<std::string, GameObject*> by_name;
  for (const auto& go : result.objects)
    by_name[go->GetName()] = go.get();

  for (size_t i = 0; i < result.objects.size(); ++i) {
    if (parent_names[i].empty()) continue;
    auto it = by_name.find(parent_names[i]);
    if (it != by_name.end()) {
      result.objects[i]->Reparent(it->second);
    } else {
      LOG_F(WARNING, "MapLoader: object '%s' references unknown parent '%s'",
            result.objects[i]->GetName().c_str(), parent_names[i].c_str());
    }
  }

  // Third pass: generate road meshes, optionally draping onto the terrain.
  const terrain::TerrainData* terrain_data = nullptr;
  for (const auto& go : result.objects) {
    if (go->GetType() == GameObjectType::kTerrain)
      terrain_data = &(static_cast<GameTerrain*>(go.get())->GetData());
  }

  std::function<float(float, float)> height_fn;
  if (terrain_data) {
    height_fn = [terrain_data](float x, float z) {
      return terrain_data->GetHeight(x, z);
    };
  }

  for (const auto& go : result.objects) {
    if (go->GetType() == GameObjectType::kRoad) {
      static_cast<GameRoad*>(go.get())->RegenerateMesh(height_fn);
    }
  }

  return result;
}

}  // namespace game
