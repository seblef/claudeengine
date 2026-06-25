#include "game/GameSystem.h"

#include <algorithm>
#include <chrono>

#include "audio/SoundManager.h"
#include "core/EventManager.h"
#include "core/EventType.h"
#include "game/GameParticleSystem.h"
#include "physics/PhysicsSystem.h"
#include "renderer/Renderer.h"
#include "track/TireTrackSystem.h"

namespace game {

GameSystem::GameSystem(abstract::Devices* devices)
    : devices_(devices),
      prev_time_(std::chrono::steady_clock::now()),
      particle_renderer_(std::make_unique<particles::ParticleRenderer>(
          devices->GetVideoDevice())),
      hud_screen_(std::make_unique<ui::HUDScreen>()),
      loading_screen_(std::make_unique<ui::LoadingScreen>()) {
  abstract::VideoDevice* video = devices->GetVideoDevice();
  renderer::Renderer::Instance().SetParticleRenderer(particle_renderer_.get());
  tire_track_system_ = std::make_unique<track::TireTrackSystem>(video);
  renderer::Renderer::Instance().SetTireTrackSystem(tire_track_system_.get());

  const char* kTrackTexPaths[physics::kSurfaceTypeCount] = {
      "tracks/track_generic.png",
      "tracks/track_terrain.png",
      "tracks/track_road.png",
  };
  for (int t = 0; t < physics::kSurfaceTypeCount; ++t) {
    track_textures_[t] = video->CreateTexture(kTrackTexPaths[t]);
    tire_track_system_->SetTrackTexture(
        static_cast<physics::SurfaceType>(t), track_textures_[t]);
  }
  hud_screen_->Build(video);
  loading_screen_->Build(video);
}

GameSystem::~GameSystem() {
  for (abstract::Texture* tex : track_textures_) {
    if (tex) tex->Release();
  }
}

void GameSystem::Update() {
  const auto now = std::chrono::steady_clock::now();
  const float dt = std::chrono::duration<float>(now - prev_time_).count();
  prev_time_ = now;
  elapsed_time_ += dt;

  devices_->Update();

  while (core::EventManager::Instance().HasEvents()) {
    core::Event e = core::EventManager::Instance().Consume();
    if (event_callback_) {
      event_callback_(e);
    }
    if (e.type == core::EventType::kWindowClose) {
      running_ = false;
    } else if (camera_controller_) {
      camera_controller_->OnEvent(e);
    }
  }

  if (camera_controller_) {
    camera_controller_->Update(dt);
  }

  for (GameObject* obj : objects_) {
    if (obj->GetParent() == nullptr)
      obj->Update(dt);
  }

  if (physics::PhysicsSystem::IsInstanced()) {
    physics::PhysicsSystem::Instance().Step(dt);
  }

  if (tire_track_system_) {
    tire_track_system_->Update(dt);
  }

  for (GameObject* obj : objects_) {
    if (obj->GetType() == GameObjectType::kParticleSystem) {
      static_cast<GameParticleSystem*>(obj)->Update(elapsed_time_, dt);
    }
  }

  if (sound_manager_ && active_camera_) {
    sound_manager_->SetListenerTransform(active_camera_->GetWorldTransform());
    sound_manager_->Update(dt);
  }

  if (active_camera_) {
    renderer::Renderer::Instance().Update(elapsed_time_,
                                          active_camera_->GetCamera());
  }
}

void GameSystem::SetSoundManager(audio::SoundManager* sound_manager) {
  sound_manager_ = sound_manager;
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

void GameSystem::SetEventCallback(
    std::function<void(const core::Event&)> cb) {
  event_callback_ = std::move(cb);
}

void GameSystem::ResetTimer() {
  prev_time_ = std::chrono::steady_clock::now();
}

}  // namespace game
