#include "game/GameVehicle.h"

#include <loguru.hpp>

#include "core/BBox3.h"
#include "game/GameMesh.h"
#include "game/GameObjectVisitor.h"
#include "game/IVehicleController.h"
#include "game/VehicleTemplate.h"
#include "physics/PhysicsSystem.h"
#include "physics/PhysicsVehicle.h"

namespace game {

namespace {

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

GameVehicle::GameVehicle(VehicleTemplate* tmpl)
    : GameObject(GameObjectType::kVehicle,
                 [&]() {
                   const core::Vec3f& he = tmpl->GetVehicleDesc().half_extents;
                   return core::BBox3(core::Vec3f(-he.x, -he.y, -he.z),
                                      core::Vec3f( he.x,  he.y,  he.z));
                 }()),
      template_(tmpl) {
  template_->AddRef();

  body_mesh_ = std::make_unique<GameMesh>(tmpl->GetBodyTemplate());
  wheel_fl_  = std::make_unique<GameMesh>(tmpl->GetFrontWheelTemplate());
  wheel_fr_  = std::make_unique<GameMesh>(tmpl->GetFrontWheelTemplate());
  wheel_rl_  = std::make_unique<GameMesh>(tmpl->GetRearWheelTemplate());
  wheel_rr_  = std::make_unique<GameMesh>(tmpl->GetRearWheelTemplate());

  AddChild(body_mesh_.get());
  AddChild(wheel_fl_.get());
  AddChild(wheel_fr_.get());
  AddChild(wheel_rl_.get());
  AddChild(wheel_rr_.get());

  const physics::VehicleDesc& vdesc = tmpl->GetVehicleDesc();
  const core::Mat4f mirror = MirrorX();
  wheel_fl_->SetLocalTransform(PositionMatrix(vdesc.front_left.position));
  wheel_fr_->SetLocalTransform(PositionMatrix(vdesc.front_right.position) * mirror);
  wheel_rl_->SetLocalTransform(PositionMatrix(vdesc.rear_left.position));
  wheel_rr_->SetLocalTransform(PositionMatrix(vdesc.rear_right.position) * mirror);
}

GameVehicle::~GameVehicle() {
  template_->Release();
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
      template_->GetVehicleDesc(), this, GetWorldTransform());
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

  GameObject::Update(dt);
}

void GameVehicle::OnBodyTransformUpdated(const core::Mat4f& transform) {
  SetWorldTransformPhysics(transform);
}

std::filesystem::path GameVehicle::GetDescPath() const {
  return std::filesystem::path(template_->GetDescPath());
}

const physics::VehicleDesc& GameVehicle::GetVehicleDesc() const {
  return template_->GetVehicleDesc();
}

}  // namespace game
