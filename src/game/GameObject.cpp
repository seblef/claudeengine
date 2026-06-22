#include "game/GameObject.h"

#include <algorithm>

namespace game {

GameObject::GameObject(GameObjectType type, const core::BBox3& local_bbox)
    : type_(type),
      local_bbox_(local_bbox),
      world_bbox_(local_bbox),
      local_transform_(core::Mat4f::kIdentity),
      world_transform_(core::Mat4f::kIdentity) {}

void GameObject::SetLocalTransform(const core::Mat4f& local) {
  local_transform_ = local;
  world_transform_ = parent_ ? (parent_->world_transform_ * local_transform_)
                              : local_transform_;
  world_bbox_ = local_bbox_ * world_transform_;
  OnWorldTransformUpdated();
  for (auto* child : children_)
    child->UpdateWorldTransform();
}

void GameObject::SetWorldTransform(const core::Mat4f& transform) {
  if (parent_)
    SetLocalTransform(parent_->world_transform_.Inverse() * transform);
  else
    SetLocalTransform(transform);
}

void GameObject::SetWorldTransformPhysics(const core::Mat4f& transform) {
  world_transform_ = transform;
  local_transform_ = parent_ ? (parent_->world_transform_.Inverse() * transform)
                              : transform;
  world_bbox_ = local_bbox_ * world_transform_;
  OnWorldTransformUpdated();
  for (auto* child : children_)
    child->UpdateWorldTransform();
}

void GameObject::UpdateWorldTransform() {
  world_transform_ = parent_ ? (parent_->world_transform_ * local_transform_)
                              : local_transform_;
  world_bbox_ = local_bbox_ * world_transform_;
  OnWorldTransformUpdated();
  for (auto* child : children_)
    child->UpdateWorldTransform();
}

void GameObject::AddChild(GameObject* child) {
  child->parent_ = this;
  child->local_transform_ = world_transform_.Inverse() * child->world_transform_;
  children_.push_back(child);
}

void GameObject::RemoveChild(GameObject* child) {
  auto it = std::find(children_.begin(), children_.end(), child);
  if (it == children_.end()) return;
  (*it)->local_transform_ = (*it)->world_transform_;
  (*it)->parent_ = nullptr;
  children_.erase(it);
}

void GameObject::Reparent(GameObject* new_parent) {
  if (parent_)
    parent_->RemoveChild(this);
  if (new_parent)
    new_parent->AddChild(this);
}

const core::Mat4f& GameObject::GetLocalTransform() const {
  return local_transform_;
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

GameObject* GameObject::GetParent() const {
  return parent_;
}

const std::vector<GameObject*>& GameObject::GetChildren() const {
  return children_;
}

void GameObject::SetName(const std::string& name) {
  name_ = name;
}

const std::string& GameObject::GetName() const {
  return name_;
}

void GameObject::Update(float dt) {
  for (GameObject* child : children_)
    child->Update(dt);
}

std::unique_ptr<GameObject> GameObject::Copy(const core::Vec3f&) const {
  return nullptr;
}

}  // namespace game
