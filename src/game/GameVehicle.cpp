#include "game/GameVehicle.h"

#include <loguru.hpp>
#include <yaml-cpp/yaml.h>

#include "core/BBox3.h"
#include "core/Config.h"
#include "core/YamlSerialiser.h"
#include "game/GameMesh.h"
#include "game/GameObjectVisitor.h"
#include "game/IVehicleController.h"
#include "game/MeshTemplate.h"
#include "physics/PhysicsSystem.h"
#include "physics/PhysicsVehicle.h"

namespace game {

namespace {

physics::WheelDesc ParseWheelDesc(const YAML::Node& n) {
  physics::WheelDesc d;
  if (!n) return d;
  if (n["position"]) d.position = core::ParseVec3(n["position"], d.position);
  d.radius                 = n["radius"].as<float>(d.radius);
  d.width                  = n["width"].as<float>(d.width);
  d.suspension_min_length  = n["suspension_min_length"].as<float>(d.suspension_min_length);
  d.suspension_max_length  = n["suspension_max_length"].as<float>(d.suspension_max_length);
  d.suspension_stiffness   = n["suspension_stiffness"].as<float>(d.suspension_stiffness);
  d.suspension_damping     = n["suspension_damping"].as<float>(d.suspension_damping);
  d.is_driven  = n["is_driven"].as<bool>(d.is_driven);
  d.is_steered = n["is_steered"].as<bool>(d.is_steered);
  return d;
}

core::Mat4f MirrorX() {
  core::Mat4f m = core::Mat4f::kIdentity;
  m(0, 0) = -1.f;
  return m;
}

core::Mat4f PositionMatrix(const core::Vec3f& p) {
  core::Mat4f m = core::Mat4f::kIdentity;
  m(0, 3) = p.x;
  m(1, 3) = p.y;
  m(2, 3) = p.z;
  return m;
}

}  // namespace

GameVehicle::GameVehicle(const core::BBox3& bbox)
    : GameObject(GameObjectType::kVehicle, bbox) {}

// static
std::unique_ptr<GameVehicle> GameVehicle::Create(
    const std::filesystem::path& desc_path,
    abstract::VideoDevice* video) {
  const std::filesystem::path full_path =
      core::Config::GetDataFolder() / desc_path;

  YAML::Node root;
  try {
    root = core::LoadYamlFile(full_path);
  } catch (const std::exception& e) {
    LOG_F(ERROR, "GameVehicle::Create: failed to parse '%s': %s",
          full_path.string().c_str(), e.what());
    return nullptr;
  }

  const std::string body_mesh_str        = root["body_mesh"].as<std::string>("");
  const std::string front_wheel_mesh_str = root["front_wheel_mesh"].as<std::string>("");
  const std::string rear_wheel_mesh_str  = root["rear_wheel_mesh"].as<std::string>("");

  if (body_mesh_str.empty() || front_wheel_mesh_str.empty() ||
      rear_wheel_mesh_str.empty()) {
    LOG_F(ERROR, "GameVehicle::Create: '%s' missing body_mesh, front_wheel_mesh, "
          "or rear_wheel_mesh", desc_path.string().c_str());
    return nullptr;
  }

  physics::VehicleDesc vdesc;
  vdesc.mass              = root["mass"].as<float>(vdesc.mass);
  vdesc.max_engine_torque = root["max_engine_torque"].as<float>(vdesc.max_engine_torque);
  vdesc.max_steer_angle   = root["max_steer_angle"].as<float>(vdesc.max_steer_angle);
  vdesc.brake_torque      = root["brake_torque"].as<float>(vdesc.brake_torque);
  vdesc.handbrake_torque  = root["handbrake_torque"].as<float>(vdesc.handbrake_torque);
  if (root["half_extents"])
    vdesc.half_extents = core::ParseVec3(root["half_extents"], vdesc.half_extents);
  if (root["com_offset"])
    vdesc.com_offset = core::ParseVec3(root["com_offset"], vdesc.com_offset);
  vdesc.front_left  = ParseWheelDesc(root["front_left"]);
  vdesc.front_right = ParseWheelDesc(root["front_right"]);
  vdesc.rear_left   = ParseWheelDesc(root["rear_left"]);
  vdesc.rear_right  = ParseWheelDesc(root["rear_right"]);

  const core::Vec3f& he = vdesc.half_extents;
  const core::BBox3  bbox(core::Vec3f(-he.x, -he.y, -he.z),
                           core::Vec3f( he.x,  he.y,  he.z));

  auto v = std::unique_ptr<GameVehicle>(new GameVehicle(bbox));
  v->desc_path_             = desc_path;
  v->vehicle_desc_          = vdesc;
  v->front_wheel_mesh_path_ = front_wheel_mesh_str;
  v->rear_wheel_mesh_path_  = rear_wheel_mesh_str;

  const std::string body_full =
      (core::Config::GetDataFolder() / body_mesh_str).string();
  const std::string front_full =
      (core::Config::GetDataFolder() / front_wheel_mesh_str).string();
  const std::string rear_full =
      (core::Config::GetDataFolder() / rear_wheel_mesh_str).string();

  MeshTemplate* body_tmpl  = MeshTemplate::GetOrLoad(body_full, video);
  MeshTemplate* front_tmpl = MeshTemplate::GetOrLoad(front_full, video);
  MeshTemplate* rear_tmpl  = MeshTemplate::GetOrLoad(rear_full, video);

  if (!body_tmpl || !front_tmpl || !rear_tmpl) {
    LOG_F(ERROR, "GameVehicle::Create: failed to load mesh templates for '%s'",
          desc_path.string().c_str());
    if (body_tmpl)  body_tmpl->Release();
    if (front_tmpl) front_tmpl->Release();
    if (rear_tmpl)  rear_tmpl->Release();
    return nullptr;
  }

  v->body_mesh_ = std::make_unique<GameMesh>(body_tmpl);
  body_tmpl->Release();

  v->wheel_fl_ = std::make_unique<GameMesh>(front_tmpl);
  v->wheel_fr_ = std::make_unique<GameMesh>(front_tmpl);
  front_tmpl->Release();

  v->wheel_rl_ = std::make_unique<GameMesh>(rear_tmpl);
  v->wheel_rr_ = std::make_unique<GameMesh>(rear_tmpl);
  rear_tmpl->Release();

  // Build the child hierarchy. AddChild preserves current world transforms as
  // local transforms (all are identity at this point).
  v->AddChild(v->body_mesh_.get());
  v->AddChild(v->wheel_fl_.get());
  v->AddChild(v->wheel_fr_.get());
  v->AddChild(v->wheel_rl_.get());
  v->AddChild(v->wheel_rr_.get());

  // Place wheels at the positions defined in the descriptor.
  // Right-side wheels are mirrored along X so the tread faces outward.
  const core::Mat4f mirror = MirrorX();
  v->wheel_fl_->SetLocalTransform(PositionMatrix(vdesc.front_left.position));
  v->wheel_fr_->SetLocalTransform(
      PositionMatrix(vdesc.front_right.position) * mirror);
  v->wheel_rl_->SetLocalTransform(PositionMatrix(vdesc.rear_left.position));
  v->wheel_rr_->SetLocalTransform(
      PositionMatrix(vdesc.rear_right.position) * mirror);

  return v;
}

void GameVehicle::Accept(GameObjectVisitor& visitor) {
  visitor.Visit(*this);
}

void GameVehicle::OnAddedToScene() {
  body_mesh_->OnAddedToScene();
  wheel_fl_->OnAddedToScene();
  wheel_fr_->OnAddedToScene();
  wheel_rl_->OnAddedToScene();
  wheel_rr_->OnAddedToScene();
}

void GameVehicle::OnRemovedFromScene() {
  Deactivate();
  body_mesh_->OnRemovedFromScene();
  wheel_fl_->OnRemovedFromScene();
  wheel_fr_->OnRemovedFromScene();
  wheel_rl_->OnRemovedFromScene();
  wheel_rr_->OnRemovedFromScene();
}

void GameVehicle::Activate() {
  if (physics_vehicle_) return;
  if (!physics::PhysicsSystem::IsInstanced()) {
    LOG_F(WARNING, "GameVehicle::Activate: PhysicsSystem not available");
    return;
  }
  physics_vehicle_ = physics::PhysicsSystem::Instance().CreateVehicle(
      vehicle_desc_, this, GetWorldTransform());
}

void GameVehicle::Deactivate() {
  if (!physics_vehicle_) return;
  if (physics::PhysicsSystem::IsInstanced())
    physics::PhysicsSystem::Instance().DestroyVehicle(physics_vehicle_);
  physics_vehicle_ = nullptr;
}

void GameVehicle::Update(float dt) {
  if (physics_vehicle_ && controller_) {
    controller_->Update(dt);
    physics_vehicle_->SetThrottle(controller_->GetThrottle());
    physics_vehicle_->SetSteer(controller_->GetSteer());
    physics_vehicle_->SetBrake(controller_->GetBrake());
    physics_vehicle_->SetHandbrake(controller_->GetHandbrake());
  }

  if (physics_vehicle_) {
    const core::Mat4f body_world_inv = GetWorldTransform().Inverse();
    const core::Mat4f mirror         = MirrorX();

    GameMesh* wheels[4] = {
        wheel_fl_.get(), wheel_fr_.get(), wheel_rl_.get(), wheel_rr_.get()
    };
    const bool mirrored[4] = {false, true, false, true};

    for (int i = 0; i < 4; ++i) {
      const core::Mat4f wheel_world = physics_vehicle_->GetWheelWorldTransform(i);
      const core::Mat4f wheel_local = body_world_inv * wheel_world;
      wheels[i]->SetLocalTransform(
          mirrored[i] ? wheel_local * mirror : wheel_local);
    }
  }

  // Propagate the tick to child objects (body_mesh_, wheels).
  GameObject::Update(dt);
}

void GameVehicle::OnBodyTransformUpdated(const core::Mat4f& transform) {
  SetWorldTransformPhysics(transform);
}

const std::filesystem::path& GameVehicle::GetDescPath() const {
  return desc_path_;
}

const physics::VehicleDesc& GameVehicle::GetVehicleDesc() const {
  return vehicle_desc_;
}

}  // namespace game
