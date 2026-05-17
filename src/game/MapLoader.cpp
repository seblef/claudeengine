#include "game/MapLoader.h"

#include <string>

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
#include "game/MeshTemplate.h"
#include "renderer/GeometryUtils.h"
#include "renderer/Light.h"
#include "renderer/MaterialDesc.h"

namespace game {

namespace {

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

  if (root["global_light"])
    result.global_light = ParseGlobalLightDesc(root["global_light"]);

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
