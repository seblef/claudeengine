#pragma once

#include "core/Event.h"

namespace game {

// Abstract interface for vehicle controllers.
//
// GameSystem (or a future VehicleSystem) calls OnEvent() for each queued
// input event, then calls Update() once per frame. After Update() the
// accessor values are stable and can be consumed by PhysicsVehicle.
//
// Throttle  — [-1, 1]: positive = forward, negative = reverse.
// Steer     — [-1, 1]: negative = left, positive = right.
// Brake     — [0, 1]:  0 = no brake, 1 = full brake.
// Handbrake — boolean: true while the handbrake key is held.
class IVehicleController {
 public:
  virtual ~IVehicleController() = default;

  IVehicleController(const IVehicleController&)            = delete;
  IVehicleController& operator=(const IVehicleController&) = delete;

  // Called for every input event before Update().
  virtual void OnEvent(const core::Event& event) = 0;

  // Advances the controller state by dt seconds.
  virtual void Update(float dt) = 0;

  // Accessors — read after Update().
  [[nodiscard]] virtual float GetThrottle()  const = 0;  // [-1, 1]
  [[nodiscard]] virtual float GetSteer()     const = 0;  // [-1, 1], negative = left
  [[nodiscard]] virtual float GetBrake()     const = 0;  // [0, 1]
  [[nodiscard]] virtual bool  GetHandbrake() const = 0;

 protected:
  IVehicleController() = default;
};

}  // namespace game
