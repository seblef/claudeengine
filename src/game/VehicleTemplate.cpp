#include "game/VehicleTemplate.h"

#include <loguru.hpp>
#include <yaml-cpp/yaml.h>

#include "core/Config.h"
#include "core/YamlSerialiser.h"
#include "game/MeshTemplate.h"

namespace game {

namespace {

physics::WheelDesc ParseWheelDesc(const YAML::Node& n) {
  physics::WheelDesc d;
  if (!n) return d;
  if (n["position"]) d.position = core::ParseVec3(n["position"], d.position);
  d.radius                = n["radius"].as<float>(d.radius);
  d.width                 = n["width"].as<float>(d.width);
  d.suspension_min_length = n["suspension_min_length"].as<float>(d.suspension_min_length);
  d.suspension_max_length = n["suspension_max_length"].as<float>(d.suspension_max_length);
  d.suspension_stiffness  = n["suspension_stiffness"].as<float>(d.suspension_stiffness);
  d.suspension_damping    = n["suspension_damping"].as<float>(d.suspension_damping);
  d.is_driven  = n["is_driven"].as<bool>(d.is_driven);
  d.is_steered = n["is_steered"].as<bool>(d.is_steered);
  return d;
}

}  // namespace

VehicleTemplate::VehicleTemplate(const std::string& desc_path,
                                 abstract::VideoDevice* video)
    : Resource(desc_path) {
  const auto full_path = core::Config::GetDataFolder() / desc_path;

  YAML::Node root;
  try {
    root = core::LoadYamlFile(full_path);
  } catch (const std::exception& e) {
    LOG_F(ERROR, "VehicleTemplate: failed to parse '%s': %s",
          full_path.string().c_str(), e.what());
    return;
  }

  const std::string body_mesh_str        = root["body_mesh"].as<std::string>("");
  const std::string front_wheel_mesh_str = root["front_wheel_mesh"].as<std::string>("");
  const std::string rear_wheel_mesh_str  = root["rear_wheel_mesh"].as<std::string>("");

  if (body_mesh_str.empty() || front_wheel_mesh_str.empty() ||
      rear_wheel_mesh_str.empty()) {
    LOG_F(ERROR, "VehicleTemplate: '%s' missing body_mesh, front_wheel_mesh, "
          "or rear_wheel_mesh", desc_path.c_str());
    return;
  }

  // Support both the new nested format (physics: / wheels: sections) and the
  // legacy flat format for backward compatibility.
  const YAML::Node phys = root["physics"];
  const YAML::Node& pd  = phys ? phys : root;

  vehicle_desc_.mass              = pd["mass"].as<float>(vehicle_desc_.mass);
  vehicle_desc_.max_engine_torque =
      pd["max_engine_torque"].as<float>(vehicle_desc_.max_engine_torque);
  vehicle_desc_.max_steer_angle   =
      pd["max_steer_angle"].as<float>(vehicle_desc_.max_steer_angle);
  vehicle_desc_.brake_torque      =
      pd["brake_torque"].as<float>(vehicle_desc_.brake_torque);
  vehicle_desc_.handbrake_torque  =
      pd["handbrake_torque"].as<float>(vehicle_desc_.handbrake_torque);
  if (pd["half_extents"])
    vehicle_desc_.half_extents =
        core::ParseVec3(pd["half_extents"], vehicle_desc_.half_extents);
  if (pd["com_offset"])
    vehicle_desc_.com_offset =
        core::ParseVec3(pd["com_offset"], vehicle_desc_.com_offset);

  if (phys) {
    // New format: per-axle suspension under physics.suspension.front / .rear.
    if (const YAML::Node susp = phys["suspension"]) {
      auto apply_axle = [](physics::WheelDesc& w, const YAML::Node& n) {
        w.suspension_min_length = n["min_length"].as<float>(w.suspension_min_length);
        w.suspension_max_length = n["max_length"].as<float>(w.suspension_max_length);
        w.suspension_stiffness  = n["stiffness"].as<float>(w.suspension_stiffness);
        w.suspension_damping    = n["damping"].as<float>(w.suspension_damping);
      };
      if (const YAML::Node front = susp["front"]) {
        apply_axle(vehicle_desc_.front_left,  front);
        apply_axle(vehicle_desc_.front_right, front);
      }
      if (const YAML::Node rear = susp["rear"]) {
        apply_axle(vehicle_desc_.rear_left,  rear);
        apply_axle(vehicle_desc_.rear_right, rear);
      }
    }

    // New format: wheel positions under wheels.front_left / etc.
    const YAML::Node wheels = root["wheels"];
    auto read_pos = [](physics::WheelDesc& w, const YAML::Node& n) {
      if (n && n["position"])
        w.position = core::ParseVec3(n["position"], w.position);
    };
    if (wheels) {
      read_pos(vehicle_desc_.front_left,  wheels["front_left"]);
      read_pos(vehicle_desc_.front_right, wheels["front_right"]);
      read_pos(vehicle_desc_.rear_left,   wheels["rear_left"]);
      read_pos(vehicle_desc_.rear_right,  wheels["rear_right"]);
    }

    // Set conventional driven/steered flags (front steer, rear drive).
    vehicle_desc_.front_left.is_steered  = true;
    vehicle_desc_.front_right.is_steered = true;
    vehicle_desc_.rear_left.is_driven    = true;
    vehicle_desc_.rear_right.is_driven   = true;
  } else {
    // Legacy flat format: per-wheel nodes at root level.
    vehicle_desc_.front_left  = ParseWheelDesc(root["front_left"]);
    vehicle_desc_.front_right = ParseWheelDesc(root["front_right"]);
    vehicle_desc_.rear_left   = ParseWheelDesc(root["rear_left"]);
    vehicle_desc_.rear_right  = ParseWheelDesc(root["rear_right"]);
  }

  const std::filesystem::path data_root = core::Config::GetDataFolder();
  body_tmpl_        = MeshTemplate::GetOrLoad((data_root / body_mesh_str).string(), video);
  front_wheel_tmpl_ = MeshTemplate::GetOrLoad((data_root / front_wheel_mesh_str).string(), video);
  rear_wheel_tmpl_  = MeshTemplate::GetOrLoad((data_root / rear_wheel_mesh_str).string(), video);

  if (!body_tmpl_->IsInitialized() || !front_wheel_tmpl_->IsInitialized() ||
      !rear_wheel_tmpl_->IsInitialized()) {
    LOG_F(ERROR, "VehicleTemplate: failed to load one or more mesh templates for '%s'",
          desc_path.c_str());
    body_tmpl_->Release();
    front_wheel_tmpl_->Release();
    rear_wheel_tmpl_->Release();
    body_tmpl_ = front_wheel_tmpl_ = rear_wheel_tmpl_ = nullptr;
    return;
  }

  initialized_ = true;
  LOG_F(INFO, "VehicleTemplate: loaded '%s'", desc_path.c_str());
}

VehicleTemplate::~VehicleTemplate() {
  if (body_tmpl_)        body_tmpl_->Release();
  if (front_wheel_tmpl_) front_wheel_tmpl_->Release();
  if (rear_wheel_tmpl_)  rear_wheel_tmpl_->Release();
}

// static
VehicleTemplate* VehicleTemplate::GetOrLoad(const std::string& desc_path,
                                             abstract::VideoDevice* video) {
  VehicleTemplate* existing = Get(desc_path);
  if (existing) {
    existing->AddRef();
    return existing;
  }
  auto* tmpl = new VehicleTemplate(desc_path, video);
  if (!tmpl->IsInitialized()) {
    tmpl->Release();
    return nullptr;
  }
  return tmpl;
}

// static
std::map<std::string, VehicleTemplate*> VehicleTemplate::GetAll() {
  std::map<std::string, VehicleTemplate*> result;
  for (const auto& kv : Resource::GetRegistry())
    result[kv.first] = kv.second;
  return result;
}

const std::string& VehicleTemplate::GetDescPath() const {
  return GetId();
}

const physics::VehicleDesc& VehicleTemplate::GetVehicleDesc() const {
  return vehicle_desc_;
}

MeshTemplate* VehicleTemplate::GetBodyTemplate() const {
  return body_tmpl_;
}

MeshTemplate* VehicleTemplate::GetFrontWheelTemplate() const {
  return front_wheel_tmpl_;
}

MeshTemplate* VehicleTemplate::GetRearWheelTemplate() const {
  return rear_wheel_tmpl_;
}

}  // namespace game
