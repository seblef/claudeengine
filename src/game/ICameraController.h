#pragma once

#include "core/Event.h"

namespace game {

class GameCamera;

// Abstract interface for camera controllers.
//
// GameSystem calls OnEvent() for each queued input event, then calls Update()
// once per frame. Implementations must not consume events from EventManager
// directly; all routing goes through GameSystem.
class ICameraController {
 public:
  virtual ~ICameraController() = default;

  ICameraController(const ICameraController&)            = delete;
  ICameraController& operator=(const ICameraController&) = delete;

  // Sets the camera this controller will drive.
  virtual void SetCamera(GameCamera* camera) = 0;

  // Handles a single input event. Called for every event before Update().
  virtual void OnEvent(const core::Event& event) = 0;

  // Advances the controller by dt seconds and pushes the new transform to the
  // camera.
  virtual void Update(float dt) = 0;

 protected:
  ICameraController() = default;
};

}  // namespace game
