#include "editor/MapSerializer.h"

#include <algorithm>
#include <fstream>
#include <string>
#include <unordered_set>

#include <loguru.hpp>
#include <stb_image_write.h>
#include <yaml-cpp/yaml.h>

#include "abstract/VideoDevice.h"
#include "core/Config.h"
#include "game/GameCamera.h"
#include "game/GameLight.h"
#include "game/GameMaterial.h"
#include "game/GameMesh.h"
#include "game/GameObjectType.h"
#include "game/GameTerrain.h"
#include "game/MapLoader.h"
#include "renderer/CircleSpotLight.h"
#include "renderer/GlobalLight.h"
#include "renderer/Light.h"
#include "renderer/OmniLight.h"
#include "renderer/RectangleSpotLight.h"
#include "terrain/FoliageLayer.h"
#include "terrain/TerrainData.h"
#include "terrain/TerrainMaterial.h"
#include "terrain/TerrainMaterialLayer.h"

namespace editor {

namespace {

void EmitVec3(YAML::Emitter& out, const core::Vec3f& v) {
  out << YAML::Flow << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
}

void EmitColor(YAML::Emitter& out, const core::Color& c) {
  out << YAML::Flow << YAML::BeginSeq << c.r << c.g << c.b << YAML::EndSeq;
}

void EmitTransform(YAML::Emitter& out, const core::Mat4f& m) {
  out << YAML::Flow << YAML::BeginSeq;
  const float* data = m.Data();
  for (int i = 0; i < 16; ++i) out << data[i];
  out << YAML::EndSeq;
}

std::string LightTypeToString(renderer::LightType type) {
  switch (type) {
    case renderer::LightType::kGlobal:     return "global";
    case renderer::LightType::kOmni:       return "omni";
    case renderer::LightType::kCircleSpot: return "circle_spot";
    case renderer::LightType::kRectSpot:   return "rect_spot";
  }
  return "global";
}

}  // namespace

// ---- SerializeVisitor -------------------------------------------------------

MapSerializer::SerializeVisitor::SerializeVisitor(
    YAML::Emitter& out,
    const std::filesystem::path& map_path,
    const std::filesystem::path& data_dir)
    : out_(out), map_path_(map_path), data_dir_(data_dir) {}

void MapSerializer::SerializeVisitor::Visit(game::GameMesh& mesh) {
  const game::MeshTemplate* tmpl = mesh.GetTemplate();
  if (tmpl->GetId().rfind("__proc_", 0) == 0) return;

  const game::GameMaterial* mat  = tmpl->GetMaterial();
  const std::string mesh_path =
      std::filesystem::relative(tmpl->GetId(), data_dir_).string();
  const std::string mat_path =
      "materials/" + mat->GetId() + ".yaml";

  out_ << YAML::BeginMap;
  out_ << YAML::Key << "name"      << YAML::Value << mesh.GetName();
  out_ << YAML::Key << "type"      << YAML::Value << "mesh";
  out_ << YAML::Key << "transform" << YAML::Value;
  EmitTransform(out_, mesh.GetWorldTransform());
  out_ << YAML::Key << "mesh"      << YAML::Value << mesh_path;
  out_ << YAML::Key << "material"  << YAML::Value << mat_path;
  out_ << YAML::EndMap;
}

void MapSerializer::SerializeVisitor::Visit(game::GameLight& light) {
  const renderer::Light* raw = light.GetLight();
  const renderer::LightType type = raw->GetType();

  out_ << YAML::BeginMap;
  out_ << YAML::Key << "name"           << YAML::Value << light.GetName();
  out_ << YAML::Key << "type"           << YAML::Value << "light";
  out_ << YAML::Key << "light_type"     << YAML::Value << LightTypeToString(type);
  out_ << YAML::Key << "transform"      << YAML::Value;
  EmitTransform(out_, light.GetWorldTransform());
  out_ << YAML::Key << "color"          << YAML::Value;
  EmitColor(out_, raw->GetColor());
  out_ << YAML::Key << "intensity"      << YAML::Value << raw->GetIntensity();
  out_ << YAML::Key << "cast_shadow"    << YAML::Value << raw->GetCastShadow();
  out_ << YAML::Key << "shadow_resolution" << YAML::Value << raw->GetShadowResolution();
  out_ << YAML::Key << "shadow_bias"    << YAML::Value << raw->GetShadowBias();

  switch (type) {
    case renderer::LightType::kGlobal: {
      const auto* gl = static_cast<const renderer::GlobalLight*>(raw);
      out_ << YAML::Key << "direction" << YAML::Value;
      EmitVec3(out_, gl->GetDirection());
      out_ << YAML::Key << "ambient_color" << YAML::Value;
      EmitVec3(out_, gl->GetAmbientColor());
      break;
    }
    case renderer::LightType::kOmni: {
      const auto* ol = static_cast<const renderer::OmniLight*>(raw);
      out_ << YAML::Key << "radius" << YAML::Value << ol->GetRadius();
      break;
    }
    case renderer::LightType::kCircleSpot: {
      const auto* cs = static_cast<const renderer::CircleSpotLight*>(raw);
      out_ << YAML::Key << "direction" << YAML::Value;
      EmitVec3(out_, cs->GetDirection());
      out_ << YAML::Key << "inner_angle" << YAML::Value << cs->GetInnerAngle();
      out_ << YAML::Key << "outer_angle" << YAML::Value << cs->GetOuterAngle();
      out_ << YAML::Key << "range"       << YAML::Value << cs->GetRange();
      break;
    }
    case renderer::LightType::kRectSpot: {
      const auto* rs = static_cast<const renderer::RectangleSpotLight*>(raw);
      out_ << YAML::Key << "direction" << YAML::Value;
      EmitVec3(out_, rs->GetDirection());
      out_ << YAML::Key << "h_angle" << YAML::Value << rs->GetHAngle();
      out_ << YAML::Key << "v_angle" << YAML::Value << rs->GetVAngle();
      out_ << YAML::Key << "range"   << YAML::Value << rs->GetRange();
      break;
    }
  }

  out_ << YAML::EndMap;
}

void MapSerializer::SerializeVisitor::Visit(game::GameCamera& camera) {
  out_ << YAML::BeginMap;
  out_ << YAML::Key << "name"      << YAML::Value << camera.GetName();
  out_ << YAML::Key << "type"      << YAML::Value << "camera";
  out_ << YAML::Key << "transform" << YAML::Value;
  EmitTransform(out_, camera.GetWorldTransform());
  out_ << YAML::EndMap;
}

void MapSerializer::SerializeVisitor::EmitTerrain(
    const game::GameTerrain* terrain) {
  if (!terrain) return;

  const terrain::TerrainData&     td = terrain->GetData();
  const terrain::TerrainMaterial& tm = terrain->GetMaterial();

  // Derive stem: "foo.map.yaml" -> "foo"
  const std::string stem =
      std::filesystem::path(map_path_.stem()).stem().string();

  // Write .r16 heightmap (raw little-endian uint16_t, row-major).
  const std::filesystem::path r16_path =
      map_path_.parent_path() / (stem + ".r16");
  const int    hw    = td.GetTexelWidth();
  const int    hh    = td.GetTexelHeight();
  const size_t count = static_cast<size_t>(hw) * hh;
  {
    std::ofstream r16(r16_path, std::ios::binary);
    if (r16) {
      r16.write(reinterpret_cast<const char*>(td.GetRawData()),
                static_cast<std::streamsize>(count * sizeof(uint16_t)));
      LOG_F(INFO, "MapSerializer: wrote heightmap '%s'",
            r16_path.string().c_str());
    } else {
      LOG_F(ERROR, "MapSerializer: cannot write heightmap '%s'",
            r16_path.string().c_str());
    }
  }

  // Write splatmap PNG via stb_image_write.
  std::string splat_path = tm.GetSplatmapPath();
  if (splat_path.empty()) splat_path = stem + "_splat.png";
  const std::filesystem::path splat_out =
      data_dir_ / "textures" / splat_path;
  std::filesystem::create_directories(splat_out.parent_path());
  const int sw = tm.GetSplatWidth();
  const int sh = tm.GetSplatHeight();
  if (sw > 0 && sh > 0 && !tm.GetSplatmapPixels().empty()) {
    if (stbi_write_png(splat_out.string().c_str(), sw, sh, 4,
                       tm.GetSplatmapPixels().data(), sw * 4) != 0) {
      LOG_F(INFO, "MapSerializer: wrote splatmap '%s'",
            splat_out.string().c_str());
    } else {
      LOG_F(ERROR, "MapSerializer: failed to write splatmap '%s'",
            splat_out.string().c_str());
    }
  }

  // Emit root-level "terrain:" block.
  out_ << YAML::Key << "terrain" << YAML::Value << YAML::BeginMap;
  out_ << YAML::Key << "heightmap"        << YAML::Value << (stem + ".r16");
  out_ << YAML::Key << "heightmap_width"  << YAML::Value << hw;
  out_ << YAML::Key << "heightmap_height" << YAML::Value << hh;
  out_ << YAML::Key << "meters_per_texel" << YAML::Value << td.GetMetersPerTexel();
  out_ << YAML::Key << "min_height"       << YAML::Value << td.GetMinHeight();
  out_ << YAML::Key << "max_height"       << YAML::Value << td.GetMaxHeight();

  out_ << YAML::Key << "material" << YAML::Value << YAML::BeginMap;
  out_ << YAML::Key << "layers" << YAML::Value << YAML::BeginSeq;
  for (int i = 0; i < tm.GetLayerCount(); ++i) {
    const terrain::TerrainMaterialLayer& layer = tm.GetLayer(i);
    out_ << YAML::BeginMap;
    out_ << YAML::Key << "albedo" << YAML::Value << layer.albedo_path;
    out_ << YAML::Key << "normal" << YAML::Value << layer.normal_path;
    out_ << YAML::Key << "tiling" << YAML::Value << layer.tiling;
    out_ << YAML::EndMap;
  }
  out_ << YAML::EndSeq;
  out_ << YAML::Key << "splatmap" << YAML::Value << splat_path;
  out_ << YAML::EndMap;

  // Emit foliage_layers sequence.
  if (terrain->GetFoliageLayerCount() > 0) {
    out_ << YAML::Key << "foliage_layers" << YAML::Value << YAML::BeginSeq;
    for (int fi = 0; fi < terrain->GetFoliageLayerCount(); ++fi) {
      const terrain::FoliageLayer* flayer = terrain->GetFoliageLayer(fi);
      const terrain::FoliageLayerDesc& fdesc = flayer->GetDesc();

      // Write density map as raw .r8 file (single channel uint8, row-major).
      const std::string dm_name = stem + "_foliage" + std::to_string(fi) + ".r8";
      const std::filesystem::path dm_out = map_path_.parent_path() / dm_name;
      {
        std::ofstream dm_file(dm_out, std::ios::binary);
        if (dm_file) {
          const auto& dm = flayer->GetDensityMap();
          dm_file.write(reinterpret_cast<const char*>(dm.data()),
                        static_cast<std::streamsize>(dm.size()));
          LOG_F(INFO, "MapSerializer: wrote foliage density '%s'",
                dm_out.string().c_str());
        } else {
          LOG_F(ERROR, "MapSerializer: cannot write foliage density '%s'",
                dm_out.string().c_str());
        }
      }

      out_ << YAML::BeginMap;
      out_ << YAML::Key << "mesh"               << YAML::Value << fdesc.mesh_path;
      out_ << YAML::Key << "texture"            << YAML::Value << fdesc.texture_path;
      out_ << YAML::Key << "density_map"        << YAML::Value << dm_name;
      out_ << YAML::Key << "density_width"      << YAML::Value << flayer->GetMapWidth();
      out_ << YAML::Key << "density_height"     << YAML::Value << flayer->GetMapHeight();
      out_ << YAML::Key << "spacing_min"        << YAML::Value << fdesc.spacing_min;
      out_ << YAML::Key << "spacing_max"        << YAML::Value << fdesc.spacing_max;
      out_ << YAML::Key << "scale_min"          << YAML::Value << fdesc.scale_min;
      out_ << YAML::Key << "scale_max"          << YAML::Value << fdesc.scale_max;
      out_ << YAML::Key << "cull_distance"      << YAML::Value << fdesc.cull_distance;
      out_ << YAML::Key << "billboard_distance" << YAML::Value << fdesc.billboard_distance;
      out_ << YAML::EndMap;
    }
    out_ << YAML::EndSeq;
  }

  out_ << YAML::EndMap;
}

// ---- Save -------------------------------------------------------------------

bool MapSerializer::Save(const EditorScene& scene,
                         const EditorCameraController::CameraState& cam,
                         const std::filesystem::path& path) {
  const std::filesystem::path data_dir = core::Config::GetDataFolder();
  YAML::Emitter out;
  out << YAML::BeginMap;

  // Map metadata.
  out << YAML::Key << "name"     << YAML::Value << scene.GetMapName();
  out << YAML::Key << "map_size" << YAML::Value << scene.GetMapSize();

  // Global directional light.
  const game::GameLightDesc& ld = scene.GetGlobalLightDesc();
  out << YAML::Key << "global_light" << YAML::Value << YAML::BeginMap;
  out << YAML::Key << "direction" << YAML::Value;
  EmitVec3(out, ld.direction);
  out << YAML::Key << "color" << YAML::Value;
  EmitColor(out, ld.color);
  out << YAML::Key << "intensity"         << YAML::Value << ld.intensity;
  out << YAML::Key << "ambient_color"     << YAML::Value;
  EmitVec3(out, ld.ambient_color);
  out << YAML::Key << "cast_shadow"       << YAML::Value << ld.cast_shadow;
  out << YAML::Key << "shadow_resolution" << YAML::Value << ld.shadow_resolution;
  out << YAML::Key << "shadow_bias"       << YAML::Value << ld.shadow_bias;
  out << YAML::EndMap;

  // Scene objects — only dynamic objects; procedural meshes are skipped by
  // the visitor. GameTerrain objects are skipped from the sequence and
  // serialised into a root-level "terrain:" block instead.
  out << YAML::Key << "objects" << YAML::Value << YAML::BeginSeq;
  SerializeVisitor visitor(out, path, data_dir);
  const game::GameTerrain* terrain_obj = nullptr;
  for (game::GameObject* obj : scene.GetObjects()) {
    if (!scene.IsDynamic(obj)) continue;
    if (obj->GetType() == game::GameObjectType::kTerrain) {
      terrain_obj = static_cast<const game::GameTerrain*>(obj);
      continue;
    }
    obj->Accept(visitor);
  }
  out << YAML::EndSeq;

  // Terrain root block (written after objects so the emitter is back at the
  // root mapping level).
  visitor.EmitTerrain(terrain_obj);

  // Editor camera + selection state.
  const game::GameObject* sel = scene.GetSelectedObject();
  out << YAML::Key << "editor" << YAML::Value << YAML::BeginMap;
  out << YAML::Key << "selected_object" << YAML::Value;
  if (sel) {
    out << sel->GetName();
  } else {
    out << YAML::Null;
  }
  out << YAML::Key << "camera_focus" << YAML::Value;
  EmitVec3(out, cam.focus);
  out << YAML::Key << "camera_azimuth"   << YAML::Value << cam.azimuth;
  out << YAML::Key << "camera_elevation" << YAML::Value << cam.elevation;
  out << YAML::Key << "camera_distance"  << YAML::Value << cam.distance;
  out << YAML::Key << "snap_grid"        << YAML::Value << 0.0f;
  out << YAML::EndMap;

  out << YAML::EndMap;

  std::ofstream file(path);
  if (!file) {
    LOG_F(ERROR, "MapSerializer: cannot open '%s' for writing",
          path.string().c_str());
    return false;
  }
  file << out.c_str();
  return file.good();
}

// ---- Load -------------------------------------------------------------------

std::optional<MapLoadResult> MapSerializer::Load(
    const std::filesystem::path& path, abstract::VideoDevice* video) {
  game::MapData map_data = game::MapLoader::Load(path, video);
  if (map_data.name.empty()) {
    LOG_F(ERROR, "MapSerializer: failed to load map '%s'", path.string().c_str());
    return std::nullopt;
  }

  MapLoadResult result;
  result.scene = std::make_unique<EditorScene>(
      video, map_data.name, map_data.map_size, map_data.global_light);

  // Transfer objects and their resource ownership to the scene.
  std::unordered_set<std::string> added_templates;
  std::unordered_set<std::string> added_materials;

  for (auto& obj : map_data.objects) {
    if (obj->GetType() == game::GameObjectType::kMesh) {
      auto* mesh = static_cast<game::GameMesh*>(obj.get());
      game::MeshTemplate* tmpl = mesh->GetTemplate();
      game::GameMaterial* mat  = tmpl->GetMaterial();

      if (added_templates.insert(tmpl->GetId()).second) {
        tmpl->AddRef();
        result.scene->AddMeshTemplate(tmpl);
      }
      if (mat && added_materials.insert(mat->GetId()).second) {
        mat->AddRef();
        result.scene->AddGameMaterial(mat);
      }
    }
    result.scene->AddDynamicObject(std::move(obj));
  }

  // Parse editor state from the YAML file.
  EditorCameraController::CameraState cam{};
  try {
    YAML::Node root = YAML::LoadFile(path.string());
    const YAML::Node& editor = root["editor"];
    if (editor) {
      if (editor["camera_focus"]) {
        const YAML::Node& f = editor["camera_focus"];
        if (f.IsSequence() && f.size() >= 3)
          cam.focus = {f[0].as<float>(), f[1].as<float>(), f[2].as<float>()};
      }
      cam.azimuth   = editor["camera_azimuth"].as<float>(0.f);
      cam.elevation = editor["camera_elevation"].as<float>(0.3f);
      cam.distance  = editor["camera_distance"].as<float>(15.f);

      const YAML::Node& sel_node = editor["selected_object"];
      if (sel_node && !sel_node.IsNull()) {
        const std::string sel_name = sel_node.as<std::string>("");
        if (!sel_name.empty()) {
          const auto& objs = result.scene->GetObjects();
          auto it = std::find_if(objs.begin(), objs.end(),
              [&sel_name](const game::GameObject* o) {
                return o->GetName() == sel_name;
              });
          if (it != objs.end()) result.scene->SetSelectedObject(*it);
        }
      }
    }
  } catch (const std::exception& e) {
    LOG_F(WARNING, "MapSerializer: failed to parse editor state from '%s': %s",
          path.string().c_str(), e.what());
  }

  result.camera_state = cam;
  result.scene->SetFilePath(path);
  return result;
}

}  // namespace editor
