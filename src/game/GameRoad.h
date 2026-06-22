#pragma once

#include <functional>
#include <memory>

#include "game/GameObject.h"
#include "game/GameMaterial.h"
#include "track/RoadSpline.h"

namespace abstract { class VideoDevice; }
namespace physics  { class PhysicsBody; }
namespace renderer {
class GeometryData;
class Mesh;
class MeshInstance;
}  // namespace renderer

namespace game {

// A game object representing a driveable road surface built from a looping
// Catmull-Rom spline.
//
// The GPU mesh and static physics body are rebuilt on every call to
// RegenerateMesh(). Call it once after adding control points, and again any
// time the spline or width changes.
//
// Follows the same scene-lifecycle pattern as GameMesh:
//   OnAddedToScene()    — registers the MeshInstance with the renderer.
//   OnRemovedFromScene() — deregisters and destroys the physics body.
class GameRoad : public GameObject {
 public:
  explicit GameRoad(abstract::VideoDevice* video);
  ~GameRoad() override;

  GameRoad(const GameRoad&)            = delete;
  GameRoad& operator=(const GameRoad&) = delete;
  GameRoad(GameRoad&&)                 = delete;
  GameRoad& operator=(GameRoad&&)      = delete;

  void Accept(GameObjectVisitor& visitor) override;

  // Registers the MeshInstance with the renderer (if mesh exists).
  void OnAddedToScene()     override;

  // Deregisters the MeshInstance and destroys the physics body.
  void OnRemovedFromScene() override;

  // Propagates world transform to the physics body (road is static, so
  // typically only needed if the road is repositioned before play).
  void OnWorldTransformUpdated() override;

  // Rebuilds the GPU mesh and static physics body from the current spline.
  //
  // height_fn maps (x, z) world coordinates to terrain Y; pass nullptr for a
  // flat road that follows the control-point Y values.
  // No-op when the spline has fewer than 2 control points.
  void RegenerateMesh(const std::function<float(float, float)>& height_fn);

  [[nodiscard]] track::RoadSpline& GetSpline() { return spline_; }
  [[nodiscard]] const track::RoadSpline& GetSpline() const { return spline_; }

  [[nodiscard]] float GetWidth()           const { return width_; }
  void                SetWidth(float w)          { width_ = w; }

  [[nodiscard]] float GetSamplesPerMetre() const { return samples_per_m_; }
  void                SetSamplesPerMetre(float s)  { samples_per_m_ = s; }

  void SetMaterial(GameMaterial* mat);

  // Returns the material, or nullptr if none is set.
  [[nodiscard]] const GameMaterial* GetMaterialPtr() const { return material_; }

 private:
  void DestroyPhysicsBody();

  track::RoadSpline              spline_;
  // cppcheck-suppress unusedStructMember
  float                          width_         = 8.f;
  // cppcheck-suppress unusedStructMember
  float                          samples_per_m_ = 1.f;
  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice*         video_;
  // cppcheck-suppress unusedStructMember
  GameMaterial*                  material_      = nullptr;

  // GPU resources, rebuilt on RegenerateMesh().
  std::unique_ptr<renderer::GeometryData>  geometry_;
  std::unique_ptr<renderer::Mesh>          mesh_;
  std::unique_ptr<renderer::MeshInstance>  instance_;

  // Static physics body; non-owning (lifetime managed by PhysicsSystem).
  // cppcheck-suppress unusedStructMember
  physics::PhysicsBody*          physics_body_  = nullptr;
  // cppcheck-suppress unusedStructMember
  bool                           in_scene_      = false;
};

}  // namespace game
