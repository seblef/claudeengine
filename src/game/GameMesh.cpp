#include "game/GameMesh.h"

#include <loguru.hpp>

#include "physics/MotionType.h"
#include "physics/PhysicsBody.h"
#include "physics/PhysicsShapeType.h"
#include "physics/PhysicsSystem.h"
#include "renderer/Renderer.h"

namespace game {

GameMesh::GameMesh(MeshTemplate* tmpl, bool always_visible)
    : GameObject(GameObjectType::kMesh, tmpl->GetLocalBBox()),
      template_(tmpl),
      always_visible_(always_visible),
      instance_(std::make_unique<renderer::MeshInstance>(
          tmpl->GetMesh(), core::Mat4f::kIdentity, always_visible)) {
  template_->AddRef();
}

GameMesh::~GameMesh() {
  DestroyPhysicsBody();
  template_->Release();
}

void GameMesh::OnAddedToScene() {
  renderer::Renderer::Instance().AddRenderable(instance_.get());
  in_scene_ = true;
  if (physics_desc_) {
    CreatePhysicsBody();
  }
}

void GameMesh::OnRemovedFromScene() {
  DestroyPhysicsBody();
  renderer::Renderer::Instance().RemoveRenderable(instance_.get());
  in_scene_ = false;
}

void GameMesh::OnWorldTransformUpdated() {
  instance_->SetWorldMatrix(GetWorldTransform());
  if (physics_body_ &&
      physics_body_->GetMotionType() != physics::MotionType::Dynamic) {
    physics_body_->SetWorldTransform(GetWorldTransform());
  }
}

void GameMesh::SetPhysicsDesc(const physics::PhysicsBodyDesc& desc) {
  physics_desc_ = desc;
  if (in_scene_) {
    DestroyPhysicsBody();
    CreatePhysicsBody();
  }
}

void GameMesh::ClearPhysicsDesc() {
  if (in_scene_)
    DestroyPhysicsBody();
  physics_desc_.reset();
}

void GameMesh::OnBodyTransformUpdated(const core::Mat4f& transform) {
  SetWorldTransformPhysics(transform);
}

MeshTemplate* GameMesh::GetTemplate() const {
  return template_;
}

std::unique_ptr<game::GameObject> GameMesh::Copy(
    const core::Vec3f& position) const {
  auto clone = std::make_unique<GameMesh>(template_);
  clone->SetName(GetName());
  core::Mat4f t = GetWorldTransform();
  t(0, 3) = position.x;
  t(1, 3) = position.y;
  t(2, 3) = position.z;
  clone->SetWorldTransform(t);
  if (physics_desc_)
    clone->SetPhysicsDesc(*physics_desc_);
  return clone;
}

void GameMesh::CreatePhysicsBody() {
  if (!physics_desc_) return;

  auto& sys = physics::PhysicsSystem::Instance();
  const physics::PhysicsShapeType shape = physics_desc_->shape.type;

  if (shape == physics::PhysicsShapeType::ConvexHull ||
      shape == physics::PhysicsShapeType::Exact) {
    const auto& positions = template_->GetCPUPositions();
    const auto& indices   = template_->GetCPUIndices();
    if (positions.empty() || indices.empty()) {
      LOG_F(WARNING,
            "GameMesh: ConvexHull/Exact body requested but no CPU mesh data "
            "available for '%s'", GetName().c_str());
      return;
    }
    physics_body_ = sys.CreateBodyWithMesh(
        *physics_desc_, this, GetWorldTransform(),
        reinterpret_cast<const float*>(positions.data()),
        static_cast<int>(positions.size()),
        indices.data(),
        static_cast<int>(indices.size()),
        template_);
  } else if (shape == physics::PhysicsShapeType::Terrain) {
    LOG_F(WARNING,
          "GameMesh: Terrain shape type is not supported; "
          "use GameTerrain instead.");
  } else {
    physics_body_ = sys.CreateBody(*physics_desc_, this, GetWorldTransform());
  }
}

void GameMesh::DestroyPhysicsBody() {
  if (physics_body_) {
    physics::PhysicsSystem::Instance().DestroyBody(physics_body_);
    physics_body_ = nullptr;
  }
}

}  // namespace game
