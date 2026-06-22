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
#include "environment/EnvironmentDesc.h"
#include "game/GameCamera.h"
#include "game/GameLight.h"
#include "game/GameMaterial.h"
#include "game/GameMesh.h"
#include "game/GameObjectType.h"
#include "game/GameParticleSystem.h"
#include "game/GamePivot.h"
#include "game/GamePlayerStart.h"
#include "game/GameRoad.h"
#include "game/GameSoundEmitter.h"
#include "track/RoadSpline.h"
#include "game/GameTerrain.h"
#include "particles/ParticleSystemTemplate.h"
#include "game/MapLoader.h"
#include "physics/MotionType.h"
#include "physics/PhysicsBodyDesc.h"
#include "physics/PhysicsShapeType.h"
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

void EmitEnvironment(YAML::Emitter& out,
                     const environment::EnvironmentDesc& env) {
  const environment::EnvironmentDesc def;
  out << YAML::Key << "environment" << YAML::Value << YAML::BeginMap;
  if (env.sky_enabled   != def.sky_enabled)
    out << YAML::Key << "sky_enabled"   << YAML::Value << env.sky_enabled;
  if (env.water_enabled != def.water_enabled)
    out << YAML::Key << "water_enabled" << YAML::Value << env.water_enabled;
  if (env.water_level   != def.water_level)
    out << YAML::Key << "water_level"   << YAML::Value << env.water_level;
  if (env.water_color_r != def.water_color_r ||
      env.water_color_g != def.water_color_g ||
      env.water_color_b != def.water_color_b) {
    out << YAML::Key << "water_color" << YAML::Value
        << YAML::Flow << YAML::BeginSeq
        << env.water_color_r << env.water_color_g << env.water_color_b
        << YAML::EndSeq;
  }
  if (env.roughness            != def.roughness)
    out << YAML::Key << "roughness"            << YAML::Value << env.roughness;
  if (env.sun_intensity        != def.sun_intensity)
    out << YAML::Key << "sun_intensity"        << YAML::Value << env.sun_intensity;
  if (env.refraction_strength  != def.refraction_strength)
    out << YAML::Key << "refraction_strength"  << YAML::Value << env.refraction_strength;
  if (env.absorption_scale     != def.absorption_scale)
    out << YAML::Key << "absorption_scale"     << YAML::Value << env.absorption_scale;
  if (env.foam_height_thresh   != def.foam_height_thresh)
    out << YAML::Key << "foam_height_thresh"   << YAML::Value << env.foam_height_thresh;
  if (env.foam_shoreline_depth != def.foam_shoreline_depth)
    out << YAML::Key << "foam_shoreline_depth" << YAML::Value << env.foam_shoreline_depth;
  if (env.foam_steepness_thresh != def.foam_steepness_thresh)
    out << YAML::Key << "foam_steepness_thresh" << YAML::Value << env.foam_steepness_thresh;
  if (env.foam_speed           != def.foam_speed)
    out << YAML::Key << "foam_speed"           << YAML::Value << env.foam_speed;
  if (env.foam_scale1          != def.foam_scale1)
    out << YAML::Key << "foam_scale1"          << YAML::Value << env.foam_scale1;
  if (env.foam_scale2          != def.foam_scale2)
    out << YAML::Key << "foam_scale2"          << YAML::Value << env.foam_scale2;
  if (env.foam_scroll_speed1   != def.foam_scroll_speed1)
    out << YAML::Key << "foam_scroll_speed1"   << YAML::Value << env.foam_scroll_speed1;
  if (env.foam_scroll_speed2   != def.foam_scroll_speed2)
    out << YAML::Key << "foam_scroll_speed2"   << YAML::Value << env.foam_scroll_speed2;
  if (env.normal_scale1        != def.normal_scale1)
    out << YAML::Key << "normal_scale1"        << YAML::Value << env.normal_scale1;
  if (env.normal_scale2        != def.normal_scale2)
    out << YAML::Key << "normal_scale2"        << YAML::Value << env.normal_scale2;
  if (env.normal_scroll_speed1 != def.normal_scroll_speed1)
    out << YAML::Key << "normal_scroll_speed1" << YAML::Value << env.normal_scroll_speed1;
  if (env.normal_scroll_speed2 != def.normal_scroll_speed2)
    out << YAML::Key << "normal_scroll_speed2" << YAML::Value << env.normal_scroll_speed2;
  if (env.normal_dir1_x != def.normal_dir1_x || env.normal_dir1_y != def.normal_dir1_y) {
    out << YAML::Key << "normal_dir1" << YAML::Value
        << YAML::Flow << YAML::BeginSeq
        << env.normal_dir1_x << env.normal_dir1_y
        << YAML::EndSeq;
  }
  if (env.normal_dir2_x != def.normal_dir2_x || env.normal_dir2_y != def.normal_dir2_y) {
    out << YAML::Key << "normal_dir2" << YAML::Value
        << YAML::Flow << YAML::BeginSeq
        << env.normal_dir2_x << env.normal_dir2_y
        << YAML::EndSeq;
  }
  if (env.cloud_enabled != def.cloud_enabled)
    out << YAML::Key << "cloud_enabled" << YAML::Value << env.cloud_enabled;
  if (env.cloud_density != def.cloud_density)
    out << YAML::Key << "cloud_density" << YAML::Value << env.cloud_density;
  if (env.wind_enabled  != def.wind_enabled)
    out << YAML::Key << "wind_enabled"  << YAML::Value << env.wind_enabled;
  if (env.wind_direction.x != def.wind_direction.x ||
      env.wind_direction.y != def.wind_direction.y ||
      env.wind_direction.z != def.wind_direction.z) {
    out << YAML::Key << "wind_direction" << YAML::Value;
    EmitVec3(out, env.wind_direction);
  }
  if (env.wind_strength != def.wind_strength)
    out << YAML::Key << "wind_strength"  << YAML::Value << env.wind_strength;
  if (env.trees_enabled != def.trees_enabled)
    out << YAML::Key << "trees_enabled" << YAML::Value << env.trees_enabled;
  if (env.start_time_of_day != def.start_time_of_day)
    out << YAML::Key << "start_time_of_day" << YAML::Value << env.start_time_of_day;
  if (env.time_scale    != def.time_scale)
    out << YAML::Key << "time_scale"    << YAML::Value << env.time_scale;
  if (env.turbidity     != def.turbidity)
    out << YAML::Key << "turbidity"     << YAML::Value << env.turbidity;
  if (!env.moon_texture.empty())
    out << YAML::Key << "moon_texture"  << YAML::Value << env.moon_texture;
  if (!env.night_sky_texture.empty())
    out << YAML::Key << "night_sky_texture" << YAML::Value << env.night_sky_texture;
  if (!env.normal_map_texture1.empty())
    out << YAML::Key << "normal_map_texture1" << YAML::Value << env.normal_map_texture1;
  if (!env.normal_map_texture2.empty())
    out << YAML::Key << "normal_map_texture2" << YAML::Value << env.normal_map_texture2;
  out << YAML::EndMap;
}

std::string ShapeTypeToString(physics::PhysicsShapeType type) {
  switch (type) {
    case physics::PhysicsShapeType::Box:        return "box";
    case physics::PhysicsShapeType::Sphere:     return "sphere";
    case physics::PhysicsShapeType::Cylinder:   return "cylinder";
    case physics::PhysicsShapeType::Capsule:    return "capsule";
    case physics::PhysicsShapeType::ConvexHull: return "convex_hull";
    case physics::PhysicsShapeType::Exact:      return "exact";
    case physics::PhysicsShapeType::Terrain:    return "terrain";
  }
  return "box";
}

std::string MotionTypeToString(physics::MotionType type) {
  switch (type) {
    case physics::MotionType::Static:    return "static";
    case physics::MotionType::Kinematic: return "kinematic";
    case physics::MotionType::Dynamic:   return "dynamic";
  }
  return "static";
}

void EmitPhysicsBodyDesc(YAML::Emitter& out,
                         const physics::PhysicsBodyDesc& desc) {
  out << YAML::Key << "physics" << YAML::Value << YAML::BeginMap;
  out << YAML::Key << "shape" << YAML::Value
      << ShapeTypeToString(desc.shape.type);

  switch (desc.shape.type) {
    case physics::PhysicsShapeType::Box:
      out << YAML::Key << "half_extents" << YAML::Value;
      EmitVec3(out, desc.shape.box.half_extents);
      break;
    case physics::PhysicsShapeType::Sphere:
      out << YAML::Key << "radius" << YAML::Value << desc.shape.sphere.radius;
      break;
    case physics::PhysicsShapeType::Cylinder:
      out << YAML::Key << "radius"      << YAML::Value
          << desc.shape.cylinder.radius;
      out << YAML::Key << "half_height" << YAML::Value
          << desc.shape.cylinder.half_height;
      break;
    case physics::PhysicsShapeType::Capsule:
      out << YAML::Key << "radius"      << YAML::Value
          << desc.shape.capsule.radius;
      out << YAML::Key << "half_height" << YAML::Value
          << desc.shape.capsule.half_height;
      break;
    default:
      break;  // ConvexHull / Exact / Terrain carry no extra parameters.
  }

  const core::Vec3f zero{0.f, 0.f, 0.f};
  if (desc.shape.center_offset != zero) {
    out << YAML::Key << "center_offset" << YAML::Value;
    EmitVec3(out, desc.shape.center_offset);
  }

  out << YAML::Key << "motion_type" << YAML::Value
      << MotionTypeToString(desc.motion_type);

  const physics::PhysicsMaterialDesc def;
  if (desc.material.friction != def.friction)
    out << YAML::Key << "friction" << YAML::Value << desc.material.friction;
  if (desc.material.restitution != def.restitution)
    out << YAML::Key << "restitution" << YAML::Value
        << desc.material.restitution;
  if (desc.material.mass != def.mass)
    out << YAML::Key << "mass" << YAML::Value << desc.material.mass;
  if (desc.material.linear_damping != def.linear_damping)
    out << YAML::Key << "linear_damping" << YAML::Value
        << desc.material.linear_damping;
  if (desc.material.angular_damping != def.angular_damping)
    out << YAML::Key << "angular_damping" << YAML::Value
        << desc.material.angular_damping;
  if (desc.material.gravity_factor != def.gravity_factor)
    out << YAML::Key << "gravity_factor" << YAML::Value
        << desc.material.gravity_factor;

  if (desc.collision_layer != physics::kLayerWorld)
    out << YAML::Key << "collision_layer" << YAML::Value
        << static_cast<int>(desc.collision_layer);
  if (desc.collision_mask != 0xFFFF)
    out << YAML::Key << "collision_mask" << YAML::Value
        << static_cast<int>(desc.collision_mask);

  out << YAML::EndMap;
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

void MapSerializer::SerializeVisitor::EmitParentField() {
  if (!current_parent_name_.empty())
    out_ << YAML::Key << "parent" << YAML::Value << current_parent_name_;
}

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
  EmitParentField();
  out_ << YAML::Key << "transform" << YAML::Value;
  EmitTransform(out_, mesh.GetWorldTransform());
  out_ << YAML::Key << "mesh"      << YAML::Value << mesh_path;
  out_ << YAML::Key << "material"  << YAML::Value << mat_path;
  if (const auto& phys = mesh.GetPhysicsDesc())
    EmitPhysicsBodyDesc(out_, *phys);
  out_ << YAML::EndMap;
}

void MapSerializer::SerializeVisitor::Visit(game::GameLight& light) {
  const renderer::Light* raw = light.GetLight();
  const renderer::LightType type = raw->GetType();

  out_ << YAML::BeginMap;
  out_ << YAML::Key << "name"           << YAML::Value << light.GetName();
  out_ << YAML::Key << "type"           << YAML::Value << "light";
  EmitParentField();
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
  EmitParentField();
  out_ << YAML::Key << "transform" << YAML::Value;
  EmitTransform(out_, camera.GetWorldTransform());
  out_ << YAML::EndMap;
}

void MapSerializer::SerializeVisitor::Visit(game::GamePivot& pivot) {
  out_ << YAML::BeginMap;
  out_ << YAML::Key << "name"      << YAML::Value << pivot.GetName();
  out_ << YAML::Key << "type"      << YAML::Value << "pivot";
  EmitParentField();
  out_ << YAML::Key << "transform" << YAML::Value;
  EmitTransform(out_, pivot.GetWorldTransform());
  out_ << YAML::EndMap;
}

void MapSerializer::SerializeVisitor::Visit(
    game::GamePlayerStart& player_start) {
  out_ << YAML::BeginMap;
  out_ << YAML::Key << "name"      << YAML::Value << player_start.GetName();
  out_ << YAML::Key << "type"      << YAML::Value << "player_start";
  EmitParentField();
  out_ << YAML::Key << "transform" << YAML::Value;
  EmitTransform(out_, player_start.GetWorldTransform());
  out_ << YAML::EndMap;
}

void MapSerializer::SerializeVisitor::Visit(
    game::GameParticleSystem& particle_system) {
  const particles::ParticleSystemTemplate* tmpl = particle_system.GetTemplate();

  out_ << YAML::BeginMap;
  out_ << YAML::Key << "name"      << YAML::Value << particle_system.GetName();
  out_ << YAML::Key << "type"      << YAML::Value << "particle_system";
  EmitParentField();
  out_ << YAML::Key << "template"  << YAML::Value << tmpl->GetId();
  out_ << YAML::Key << "transform" << YAML::Value;
  EmitTransform(out_, particle_system.GetWorldTransform());
  out_ << YAML::EndMap;
}

void MapSerializer::SerializeVisitor::Visit(
    game::GameSoundEmitter& sound_emitter) {
  out_ << YAML::BeginMap;
  out_ << YAML::Key << "name"         << YAML::Value << sound_emitter.GetName();
  out_ << YAML::Key << "type"         << YAML::Value << "sound_emitter";
  EmitParentField();
  out_ << YAML::Key << "sound"        << YAML::Value << sound_emitter.GetSoundName();
  out_ << YAML::Key << "volume_scale" << YAML::Value << sound_emitter.GetVolumeScale();
  out_ << YAML::Key << "transform"    << YAML::Value;
  EmitTransform(out_, sound_emitter.GetWorldTransform());
  out_ << YAML::EndMap;
}

void MapSerializer::SerializeVisitor::Visit(game::GameRoad& road) {
  const track::RoadSpline& spline = road.GetSpline();

  out_ << YAML::BeginMap;
  out_ << YAML::Key << "name"             << YAML::Value << road.GetName();
  out_ << YAML::Key << "type"             << YAML::Value << "road";
  EmitParentField();
  out_ << YAML::Key << "width"            << YAML::Value << road.GetWidth();
  out_ << YAML::Key << "samples_per_metre" << YAML::Value
       << road.GetSamplesPerMetre();

  if (const game::GameMaterial* mat = road.GetMaterialPtr()) {
    out_ << YAML::Key << "material" << YAML::Value
         << "materials/" + mat->GetId() + ".yaml";
  }

  out_ << YAML::Key << "control_points" << YAML::Value << YAML::BeginSeq;
  for (int i = 0; i < spline.GetPointCount(); ++i) {
    const core::Vec3f& p = spline.GetControlPoint(i);
    out_ << YAML::Flow << YAML::BeginSeq << p.x << p.y << p.z << YAML::EndSeq;
  }
  out_ << YAML::EndSeq;
  out_ << YAML::EndMap;
}

void MapSerializer::SerializeVisitor::Visit(game::GameVehicle& vehicle) {
  out_ << YAML::BeginMap;
  out_ << YAML::Key << "name"      << YAML::Value << vehicle.GetName();
  out_ << YAML::Key << "type"      << YAML::Value << "vehicle";
  EmitParentField();
  out_ << YAML::Key << "desc"      << YAML::Value
       << vehicle.GetDescPath().string();
  out_ << YAML::Key << "transform" << YAML::Value;
  EmitTransform(out_, vehicle.GetWorldTransform());
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

  // Write .r16 heightmap only when the raw samples have changed.
  const std::filesystem::path r16_path =
      map_path_.parent_path() / (stem + ".r16");
  const int    hw    = td.GetTexelWidth();
  const int    hh    = td.GetTexelHeight();
  const size_t count = static_cast<size_t>(hw) * hh;
  if (td.IsDirty()) {
    std::ofstream r16(r16_path, std::ios::binary);
    if (r16) {
      r16.write(reinterpret_cast<const char*>(td.GetRawData()),
                static_cast<std::streamsize>(count * sizeof(uint16_t)));
      LOG_F(INFO, "MapSerializer: wrote heightmap '%s'",
            r16_path.string().c_str());
      td.ClearDirty();
    } else {
      LOG_F(ERROR, "MapSerializer: cannot write heightmap '%s'",
            r16_path.string().c_str());
    }
  } else {
    LOG_F(INFO, "MapSerializer: heightmap unchanged, skipping '%s'",
          r16_path.string().c_str());
  }

  // Write splatmap PNG only when the pixel buffer has changed.
  std::string splat_path = tm.GetSplatmapPath();
  if (splat_path.empty()) splat_path = stem + "_splat.png";
  const std::filesystem::path splat_out =
      data_dir_ / "textures" / splat_path;
  const int sw = tm.GetSplatWidth();
  const int sh = tm.GetSplatHeight();
  if (tm.IsDirty()) {
    std::filesystem::create_directories(splat_out.parent_path());
    if (sw > 0 && sh > 0 && !tm.GetSplatmapPixels().empty()) {
      if (stbi_write_png(splat_out.string().c_str(), sw, sh, 4,
                         tm.GetSplatmapPixels().data(), sw * 4) != 0) {
        LOG_F(INFO, "MapSerializer: wrote splatmap '%s'",
              splat_out.string().c_str());
        tm.ClearDirty();
      } else {
        LOG_F(ERROR, "MapSerializer: failed to write splatmap '%s'",
              splat_out.string().c_str());
      }
    }
  } else {
    LOG_F(INFO, "MapSerializer: splatmap unchanged, skipping '%s'",
          splat_out.string().c_str());
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

      // Write density map only when the painted data has changed.
      const std::string dm_name = stem + "_foliage" + std::to_string(fi) + ".r8";
      const std::filesystem::path dm_out = map_path_.parent_path() / dm_name;
      if (flayer->IsSaveDirty()) {
        std::ofstream dm_file(dm_out, std::ios::binary);
        if (dm_file) {
          const auto& dm = flayer->GetDensityMap();
          dm_file.write(reinterpret_cast<const char*>(dm.data()),
                        static_cast<std::streamsize>(dm.size()));
          LOG_F(INFO, "MapSerializer: wrote foliage density '%s'",
                dm_out.string().c_str());
          flayer->ClearSaveDirty();
        } else {
          LOG_F(ERROR, "MapSerializer: cannot write foliage density '%s'",
                dm_out.string().c_str());
        }
      } else {
        LOG_F(INFO, "MapSerializer: foliage density unchanged, skipping '%s'",
              dm_out.string().c_str());
      }

      out_ << YAML::BeginMap;
      out_ << YAML::Key << "name"               << YAML::Value << fdesc.name;
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

  EmitEnvironment(out, scene.GetEnvironmentDesc());

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
    const game::GameObject* parent = obj->GetParent();
    visitor.SetCurrentParentName(parent ? parent->GetName() : "");
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
  out << YAML::Key << "camera" << YAML::Value << YAML::BeginMap;
  out << YAML::Key << "focus" << YAML::Value;
  EmitVec3(out, cam.focus);
  out << YAML::Key << "azimuth"   << YAML::Value << cam.azimuth;
  out << YAML::Key << "elevation" << YAML::Value << cam.elevation;
  out << YAML::Key << "distance"  << YAML::Value << cam.distance;
  out << YAML::EndMap;
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
      video, map_data.name, map_data.map_size, map_data.global_light, false);
  result.scene->SetEnvironmentDesc(map_data.environment_desc);

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
      // New nested format: editor.camera.{ focus, azimuth, elevation, distance }
      const YAML::Node& cam_node = editor["camera"];
      if (cam_node) {
        if (cam_node["focus"]) {
          const YAML::Node& f = cam_node["focus"];
          if (f.IsSequence() && f.size() >= 3)
            cam.focus = {f[0].as<float>(), f[1].as<float>(), f[2].as<float>()};
        }
        cam.azimuth   = cam_node["azimuth"].as<float>(0.f);
        cam.elevation = cam_node["elevation"].as<float>(0.3f);
        cam.distance  = cam_node["distance"].as<float>(15.f);
      } else {
        // Backward compat: flat keys written by older saves.
        if (editor["camera_focus"]) {
          const YAML::Node& f = editor["camera_focus"];
          if (f.IsSequence() && f.size() >= 3)
            cam.focus = {f[0].as<float>(), f[1].as<float>(), f[2].as<float>()};
        }
        cam.azimuth   = editor["camera_azimuth"].as<float>(0.f);
        cam.elevation = editor["camera_elevation"].as<float>(0.3f);
        cam.distance  = editor["camera_distance"].as<float>(15.f);
      }

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
