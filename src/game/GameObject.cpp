#include "game/GameObject.h"

namespace game {

GameObject::GameObject(GameObjectType type, const core::BBox3& local_bbox)
    : type_(type),
      local_bbox_(local_bbox),
      world_bbox_(local_bbox),
      world_transform_(core::Mat4f::kIdentity) {}

void GameObject::SetWorldTransform(const core::Mat4f& transform) {
  world_transform_ = transform;
  world_bbox_      = local_bbox_ * transform;
  OnWorldTransformUpdated();
}

const core::Mat4f& GameObject::GetWorldTransform() const {
  return world_transform_;
}

const core::BBox3& GameObject::GetLocalBBox() const {
  return local_bbox_;
}

const core::BBox3& GameObject::GetWorldBBox() const {
  return world_bbox_;
}

GameObjectType GameObject::GetType() const {
  return type_;
}

void GameObject::SetName(const std::string& name) {
  name_ = name;
}

const std::string& GameObject::GetName() const {
  return name_;
}

}  // namespace game
