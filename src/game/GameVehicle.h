#pragma once

#include <filesystem>
#include <memory>

#include "game/DamageZone.h"
#include "game/GameObject.h"
#include "game/IVehicleDamageListener.h"
#include "physics/IPhysicsBodyListener.h"
#include "physics/IPhysicsCollisionListener.h"
#include "physics/VehicleDesc.h"

namespace abstract { class VideoDevice; }

namespace audio {
class ResourceManager;
class SoundManager;
}  // namespace audio

namespace physics {
class PhysicsVehicle;
}  // namespace physics

namespace game {

class GameMesh;
class IVehicleController;
class IVehicleFireListener;
class IVehicleScrapeListener;
class IVehicleWreckListener;
class MeshTemplate;
class VehicleCrashSound;
class VehicleDamage;
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
class GameVehicle : public GameObject,
                    public physics::IPhysicsBodyListener,
                    public physics::IPhysicsCollisionListener,
                    public IVehicleDamageListener {
 public:
  // Constructs the vehicle from a pre-loaded VehicleTemplate (AddRef'd on entry).
  // Instantiates body and wheel GameMesh children from the template's mesh templates.
  // sound_manager and resource_manager may be null (crash sounds become silent).
  explicit GameVehicle(VehicleTemplate* tmpl,
                      audio::SoundManager* sound_manager = nullptr,
                      audio::ResourceManager* resource_manager = nullptr);

  ~GameVehicle() override;

  GameVehicle(const GameVehicle&)            = delete;
  GameVehicle& operator=(const GameVehicle&) = delete;
  GameVehicle(GameVehicle&&)                 = delete;
  GameVehicle& operator=(GameVehicle&&)      = delete;

  void Accept(GameObjectVisitor& visitor) override;

  // Returns a new GameVehicle from the same template, placed at position.
  [[nodiscard]] std::unique_ptr<GameObject> Copy(
      const core::Vec3f& position) const override;

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

  // --- IPhysicsCollisionListener -----------------------------------------------

  // Converts world_point to body-local space and forwards it to damage_.
  void OnCollision(const core::Vec3f& world_point, float impulse) override;

  // Forwards to scrape_listener_, if one is set.
  void OnSustainedContact(const core::Vec3f& world_point,
                          const core::Vec3f& world_normal,
                          const core::Vec3f& relative_velocity) override;

  // --- Damage / wreck ----------------------------------------------------------

  [[nodiscard]] VehicleDamage& GetDamage() const { return *damage_; }

  // game::IVehicleDamageListener. Registered on damage_ by the constructor
  // (this vehicle listens to its own damage model). The first crossing of the
  // wreck threshold (conventionally 1.0) transitions the vehicle into the
  // wrecked state: control input is cut off, wheel transforms stop updating.
  // A no-op once already wrecked, so a later zone independently reaching 1.0
  // cannot re-wreck it.
  //
  // This is called synchronously from within physics::PhysicsSystem::Step()
  // (Jolt's OnContactAdded contact callback, itself invoked while Jolt holds
  // internal body/broadphase locks — see PhysicsSystem.cpp's
  // VehicleContactListener). wreck_listener_ is therefore NOT notified here:
  // vfx::VehicleWreckEffect's explosion calls physics::PhysicsSystem::
  // SphereOverlap(), which takes Jolt's locking narrow-phase query — calling
  // that reentrantly from the same thread mid-Step() deadlocks against the
  // lock Jolt already holds. The notification is deferred to the next
  // Update() instead, which always runs before that frame's Step() (see
  // GameSystem::Update()'s ordering) and is therefore guaranteed to be
  // outside any Jolt callback.
  void OnDamageThresholdCrossed(DamageZone zone, float threshold, float fraction) override;

  // True once the vehicle has been wrecked (see OnDamageThresholdCrossed()).
  [[nodiscard]] bool IsWrecked() const { return drive_state_ == DriveState::kWrecked; }

  // --- Controller ------------------------------------------------------------

  // Non-owning pointer. Must be set by the caller (e.g. PlayModeManager) before
  // Activate(). May be nullptr — the vehicle will be simulated without inputs.
  void SetVehicleController(IVehicleController* ctrl) { controller_ = ctrl; }

  // Non-owning pointer. The caller (e.g. PlayModeManager, main.cpp's standalone
  // vehicle spawn) owns the concrete listener (typically a
  // vfx::VehicleScrapeEffect) and must keep it alive at least as long as this
  // vehicle. May be nullptr — sustained-contact events are then simply dropped.
  void SetScrapeListener(IVehicleScrapeListener* listener) { scrape_listener_ = listener; }

  // Non-owning pointer. The caller (e.g. PlayModeManager, main.cpp's standalone
  // vehicle spawn) owns the concrete listener (typically a
  // vfx::VehicleFireEffect) and must keep it alive at least as long as this
  // vehicle. May be nullptr — the per-frame transform is then simply not
  // forwarded anywhere.
  void SetFireListener(IVehicleFireListener* listener) { fire_listener_ = listener; }

  // Non-owning pointer. The caller (e.g. PlayModeManager, main.cpp's standalone
  // vehicle spawn) owns the concrete listener (typically a
  // vfx::VehicleWreckEffect) and must keep it alive at least as long as this
  // vehicle. May be nullptr — the wreck notification is then simply dropped.
  void SetWreckListener(IVehicleWreckListener* listener) { wreck_listener_ = listener; }

  /// True while the vehicle is actively driving in reverse.
  [[nodiscard]] bool IsReversing() const {
    return drive_state_ == DriveState::kReverse;
  }

  [[nodiscard]] std::filesystem::path       GetDescPath()      const;
  [[nodiscard]] const physics::VehicleDesc& GetVehicleDesc()   const;
  // Non-owning; used by PickingUtils for ray-triangle intersection.
  [[nodiscard]] MeshTemplate*               GetBodyTemplate()  const;

 private:
  enum class DriveState { kForward, kBraking, kReverse, kFlipped, kRecovering, kWrecked };

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

  // Non-owning; set by the caller. See SetScrapeListener().
  // cppcheck-suppress unusedStructMember
  IVehicleScrapeListener*   scrape_listener_ = nullptr;

  // Non-owning; set by the caller. See SetFireListener().
  // cppcheck-suppress unusedStructMember
  IVehicleFireListener*     fire_listener_   = nullptr;

  // Non-owning; set by the caller. See SetWreckListener().
  // cppcheck-suppress unusedStructMember
  IVehicleWreckListener*    wreck_listener_  = nullptr;

  // True from the moment OnDamageThresholdCrossed() wrecks the vehicle until
  // the next Update() call delivers the deferred wreck_listener_ notification
  // (see OnDamageThresholdCrossed()'s doc comment for why it can't be called
  // immediately).
  // cppcheck-suppress unusedStructMember
  bool                      wreck_notify_pending_ = false;
  // World-space position captured at the moment of wrecking, passed to
  // wreck_listener_ once the deferred notification fires.
  // cppcheck-suppress unusedStructMember
  core::Vec3f               wreck_position_       = core::Vec3f::kZero;

  // cppcheck-suppress unusedStructMember
  std::unique_ptr<VehicleDamage> damage_;

  // cppcheck-suppress unusedStructMember
  std::unique_ptr<VehicleCrashSound> crash_sound_;

  // Non-owning; retained only to reconstruct crash_sound_ in Copy().
  // cppcheck-suppress unusedStructMember
  audio::SoundManager*    sound_manager_    = nullptr;
  // cppcheck-suppress unusedStructMember
  audio::ResourceManager* resource_manager_ = nullptr;

  // Snaps the vehicle upright: computes an upright pose from the current
  // transform, lifts the body, and zeroes velocities.
  void SelfRight();

  // Sets the visibility of the body mesh and all four wheel meshes.
  void SetMeshesVisible(bool visible);

  // cppcheck-suppress unusedStructMember
  DriveState                drive_state_     = DriveState::kForward;
  // cppcheck-suppress unusedStructMember
  float                     reverse_timer_   = 0.f;
  // cppcheck-suppress unusedStructMember
  float                     flip_timer_      = 0.f;
  // cppcheck-suppress unusedStructMember
  float                     recovery_timer_  = 0.f;
};

}  // namespace game
