#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <vector>

#include <array>

#include "abstract/Devices.h"
#include "abstract/Texture.h"
#include "core/Event.h"
#include "physics/SurfaceMaterial.h"
#include "core/Singleton.h"
#include "game/GameCamera.h"
#include "game/GameObject.h"
#include "game/ICameraController.h"
#include "particles/ParticleRenderer.h"
#include "track/TireTrackSystem.h"
#include "ui/HUDScreen.h"
#include "ui/LoadingScreen.h"

namespace audio { class SoundManager; }

namespace game {

// Singleton orchestrating the game loop.
//
// Each call to Update() advances one frame:
//   1. Computes dt from the monotonic clock.
//   2. Pumps OS events via Devices::Update().
//   3. Drains EventManager: routes kWindowClose to the running flag, all other
//      events to the active camera controller.
//   4. Calls camera_controller_->Update(dt) if a controller is set.
//   5. Calls Renderer::Update() if a camera is active.
//
// Object lifecycle:
//   AddObject()    → OnAddedToScene()
//   RemoveObject() → OnRemovedFromScene()
//
// Lifecycle: new GameSystem(devices) → loop while IsRunning() → Shutdown().
class GameSystem : public core::Singleton<GameSystem> {
 public:
  // devices must outlive this GameSystem.
  explicit GameSystem(abstract::Devices* devices);
  ~GameSystem();

  // Advances the simulation and renders one frame.
  void Update();

  // Adds obj to the scene and calls obj->OnAddedToScene().
  void AddObject(GameObject* obj);

  // Removes obj from the scene and calls obj->OnRemovedFromScene().
  void RemoveObject(GameObject* obj);

  // Sets the active camera and registers it with the Renderer.
  // Calls controller->SetCamera(camera) if a controller is already set.
  void SetCamera(GameCamera* camera);

  // Sets the active camera controller and binds the current camera to it.
  void SetCameraController(ICameraController* controller);

  // Registers a callback invoked for every event drained from the EventManager,
  // before the event is routed to the camera controller. Use this to handle
  // application-level events (e.g. debug key toggles) without competing for
  // the shared queue.
  void SetEventCallback(std::function<void(const core::Event&)> cb);

  // Returns false after a kWindowClose event has been received.
  [[nodiscard]] bool  IsRunning()     const { return running_; }

  // Returns the total elapsed time in seconds since construction.
  [[nodiscard]] float GetElapsedTime() const { return elapsed_time_; }

  // Resets the frame timer to now so the next Update() dt is near zero.
  // Call this after all setup (map loading, object placement) and just before
  // entering the main loop to prevent an artificially large first-frame dt from
  // causing the physics character to overshoot the player-start position.
  void ResetTimer();

  // Registers the SoundManager to be ticked each frame.
  // SetListenerTransform (from the active camera) and Update(dt) are called
  // automatically from Update(). Pass nullptr to detach.
  void SetSoundManager(audio::SoundManager* sound_manager);

  // Returns the in-game HUD overlay (crosshair, health, ammo).
  // Build() has already been called; call Show()/Hide() as needed.
  [[nodiscard]] ui::HUDScreen*     GetHUDScreen()     const { return hud_screen_.get(); }

  // Returns the level-transition loading overlay (black background, progress bar).
  // Build() has already been called; call Show()/Hide() as needed.
  [[nodiscard]] ui::LoadingScreen* GetLoadingScreen() const { return loading_screen_.get(); }

 private:
  // cppcheck-suppress unusedStructMember
  abstract::Devices*  devices_;
  // cppcheck-suppress unusedStructMember
  std::vector<GameObject*> objects_;
  // cppcheck-suppress unusedStructMember
  GameCamera*         active_camera_      = nullptr;
  // cppcheck-suppress unusedStructMember
  ICameraController*  camera_controller_  = nullptr;
  // cppcheck-suppress unusedStructMember
  bool                running_            = true;
  // cppcheck-suppress unusedStructMember
  float               elapsed_time_       = 0.f;
  std::chrono::steady_clock::time_point prev_time_;
  // cppcheck-suppress unusedStructMember
  std::function<void(const core::Event&)> event_callback_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<particles::ParticleRenderer> particle_renderer_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<track::TireTrackSystem>      tire_track_system_;
  // Track textures owned by GameSystem (ref-counted; released in destructor).
  // cppcheck-suppress unusedStructMember
  std::array<abstract::Texture*, physics::kSurfaceTypeCount> track_textures_ = {};
  // cppcheck-suppress unusedStructMember
  audio::SoundManager* sound_manager_ = nullptr;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<ui::HUDScreen>     hud_screen_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<ui::LoadingScreen> loading_screen_;
};

}  // namespace game
