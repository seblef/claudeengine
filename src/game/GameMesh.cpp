#include "game/GameMesh.h"

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
  template_->Release();
}

void GameMesh::OnAddedToScene() {
  renderer::Renderer::Instance().AddRenderable(instance_.get());
}

void GameMesh::OnRemovedFromScene() {
  renderer::Renderer::Instance().RemoveRenderable(instance_.get());
}

void GameMesh::OnWorldTransformUpdated() {
  instance_->SetWorldMatrix(GetWorldTransform());
}

MeshTemplate* GameMesh::GetTemplate() const {
  return template_;
}

std::unique_ptr<game::GameObject> GameMesh::Copy(const core::Vec3f& position) const {
  auto clone = std::make_unique<GameMesh>(template_);
  clone->SetName(GetName() + " (copy)");
  core::Mat4f t = GetWorldTransform();
  t(0, 3) = position.x;
  t(1, 3) = position.y;
  t(2, 3) = position.z;
  clone->SetWorldTransform(t);
  return clone;
}

}  // namespace game
