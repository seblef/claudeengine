#include "game/GameSystem.h"

#include <algorithm>
#include <chrono>

#include "core/EventManager.h"
#include "core/EventType.h"
#include "renderer/Renderer.h"

namespace game {

GameSystem::GameSystem(abstract::Devices* devices)
    : devices_(devices),
      prev_time_(std::chrono::steady_clock::now()) {}

void GameSystem::Update() {
  const auto now = std::chrono::steady_clock::now();
  const float dt = std::chrono::duration<float>(now - prev_time_).count();
  prev_time_ = now;
  elapsed_time_ += dt;

  devices_->Update();

  while (core::EventManager::Instance().HasEvents()) {
    core::Event e = core::EventManager::Instance().Consume();
    if (e.type == core::EventType::kWindowClose) {
      running_ = false;
    } else if (camera_controller_) {
      camera_controller_->OnEvent(e);
    }
  }

  if (camera_controller_) {
    camera_controller_->Update(dt);
  }

  if (active_camera_) {
    renderer::Renderer::Instance().Update(elapsed_time_,
                                          active_camera_->GetCamera());
  }
}

void GameSystem::AddObject(GameObject* obj) {
  objects_.push_back(obj);
  obj->OnAddedToScene();
}

void GameSystem::RemoveObject(GameObject* obj) {
  objects_.erase(std::remove(objects_.begin(), objects_.end(), obj),
                 objects_.end());
  obj->OnRemovedFromScene();
}

void GameSystem::SetCamera(GameCamera* camera) {
  active_camera_ = camera;
  renderer::Renderer::Instance().SetCamera(camera->GetCamera());
  if (camera_controller_) {
    camera_controller_->SetCamera(camera);
  }
}

void GameSystem::SetCameraController(ICameraController* controller) {
  camera_controller_ = controller;
  controller->SetCamera(active_camera_);
}

}  // namespace game
