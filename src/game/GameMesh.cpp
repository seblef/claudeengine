#include "game/GameMesh.h"

#include "renderer/Renderer.h"

namespace game {

GameMesh::GameMesh(MeshTemplate* tmpl, bool always_visible)
    : GameObject(GameObjectType::kMesh,
                 tmpl->GetRenderableMesh()->GetLocalBBox()),
      template_(tmpl),
      always_visible_(always_visible) {
  template_->AddRef();
}

GameMesh::~GameMesh() {
  template_->Release();
}

void GameMesh::OnAddedToScene() {
  instance_ = std::make_unique<renderer::RenderableMeshInstance>(
      template_->GetRenderableMesh(), GetWorldTransform(), always_visible_);
  renderer::Renderer::Instance().AddRenderable(instance_.get());
}

void GameMesh::OnRemovedFromScene() {
  renderer::Renderer::Instance().RemoveRenderable(instance_.get());
  instance_.reset();
}

void GameMesh::OnWorldTransformUpdated() {
  if (instance_) {
    instance_->SetWorldMatrix(GetWorldTransform());
  }
}

}  // namespace game
