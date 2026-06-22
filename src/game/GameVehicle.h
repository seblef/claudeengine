#pragma once

#include <filesystem>
#include <memory>

#include "game/GameObject.h"
#include "physics/IPhysicsBodyListener.h"
#include "physics/VehicleDesc.h"

namespace abstract { class VideoDevice; }

namespace physics {
class PhysicsVehicle;
}  // namespace physics

namespace game {

class GameMesh;
class IVehicleController;
class VehicleTemplate;

// A wheeled vehicle scene object driven by a PhysicsVehicle simulation.
//
// In editor mode the vehicle is inactive: body and wheel meshes are visible but
// no physics simulation runs.  PlayModeManager calls Activate() — after assigning
// a controller via SetVehicleController() — to start the simulation.
//
// Scene hierarchy created by the constructor:
//   GameVehicle          ← world transform driven by PhysicsVehicle body
//     ├── body_mesh_     ← local transform = identity
//     ├── wheel_fl_      ← local transform updated from physics each frame
//     ├── wheel_fr_      ← local transform updated from physics (mirrored X)
//     ├── wheel_rl_      ← local transform updated from physics each frame
//     └── wheel_rr_      ← local transform updated from physics (mirrored X)
class GameVehicle : public GameObject, public physics::IPhysicsBodyListener {
 public:
  // Constructs the vehicle from a pre-loaded VehicleTemplate (AddRef'd on entry).
  // Instantiates body and wheel GameMesh children from the template's mesh templates.
  explicit GameVehicle(VehicleTemplate* tmpl);

  ~GameVehicle() override;

  GameVehicle(const GameVehicle&)            = delete;
  GameVehicle& operator=(const GameVehicle&) = delete;
  GameVehicle(GameVehicle&&)                 = delete;
  GameVehicle& operator=(GameVehicle&&)      = delete;

  void Accept(GameObjectVisitor& visitor) override;

  // --- Scene lifecycle -------------------------------------------------------

  // Registers body and wheel meshes with the renderer. Does NOT create physics.
  void OnAddedToScene()     override;
  // Calls Deactivate(), then unregisters meshes from the renderer.
  void OnRemovedFromScene() override;
  // Children's transforms are updated by the base class; no extra work needed here.
  void OnWorldTransformUpdated() override {}

  // --- Physics activation ----------------------------------------------------

  // Creates a PhysicsVehicle via PhysicsSystem::CreateVehicle().
  // Must only be called while the vehicle is in scene.
  void Activate();
  // Destroys the PhysicsVehicle. Safe to call when already inactive.
  void Deactivate();
  [[nodiscard]] bool IsActive() const { return physics_vehicle_ != nullptr; }

  // --- Per-frame tick --------------------------------------------------------

  // 1. Forwards controller inputs to physics_vehicle_.
  // 2. Updates wheel local transforms from the last physics simulation step.
  // 3. Propagates the tick to child GameMesh objects.
  void Update(float dt) override;

  // --- IPhysicsBodyListener --------------------------------------------------

  // Propagates the body's simulated world transform through the scene hierarchy.
  void OnBodyTransformUpdated(const core::Mat4f& transform) override;

  // --- Controller ------------------------------------------------------------

  // Non-owning pointer. Must be set by the caller (e.g. PlayModeManager) before
  // Activate(). May be nullptr — the vehicle will be simulated without inputs.
  void SetVehicleController(IVehicleController* ctrl) { controller_ = ctrl; }

  [[nodiscard]] std::filesystem::path       GetDescPath()    const;
  [[nodiscard]] const physics::VehicleDesc& GetVehicleDesc() const;

 private:
  // cppcheck-suppress unusedStructMember
  VehicleTemplate*          template_;

  // cppcheck-suppress unusedStructMember
  std::unique_ptr<GameMesh> body_mesh_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<GameMesh> wheel_fl_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<GameMesh> wheel_fr_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<GameMesh> wheel_rl_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<GameMesh> wheel_rr_;

  // Non-owning; lifetime managed by PhysicsSystem.
  // cppcheck-suppress unusedStructMember
  physics::PhysicsVehicle*  physics_vehicle_ = nullptr;

  // Non-owning; set by the caller before Activate().
  // cppcheck-suppress unusedStructMember
  IVehicleController*       controller_      = nullptr;
};

}  // namespace game
