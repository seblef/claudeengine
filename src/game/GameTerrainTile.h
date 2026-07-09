#pragma once

#include <memory>

#include "game/GameMaterial.h"
#include "game/GameObject.h"
#include "track/TileDesc.h"

namespace abstract { class VideoDevice; }
namespace physics  { class PhysicsBody; }
namespace renderer {
class GeometryData;
class Mesh;
class MeshInstance;
}  // namespace renderer

namespace game {

// A game object representing a flat surface-property overlay: a materialed
// quad placed at terrain height that overrides physics friction/restitution
// under its footprint (oil slicks, mud, ramps) without modifying the terrain.
//
// The GPU mesh and static physics body are rebuilt on every call to
// RegenerateMesh(). Call it once after construction, and again any time the
// TileDesc (size or surface override) changes.
//
// The static physics body is a thin box straddling the tile's placement
// height (see GameTerrainTile.cpp) so that vehicle wheel raycasts hit it
// ahead of the terrain heightfield beneath. Its friction/restitution come
// directly from TileDesc::surface, so Jolt's default wheel-friction combine
// (sqrt(tireFriction * bodyFriction)) already yields reduced grip on an oil
// slick; PhysicsSystem::ApplyWheelRestitution() reads the same body's
// restitution to give ramps their launch behaviour (see PhysicsSystem.cpp).
//
// Follows the same scene-lifecycle pattern as GameRoad:
//   OnAddedToScene()     — registers the MeshInstance with the renderer.
//   OnRemovedFromScene() — deregisters and destroys the physics body.
class GameTerrainTile : public GameObject {
 public:
  explicit GameTerrainTile(abstract::VideoDevice* video);
  ~GameTerrainTile() override;

  GameTerrainTile(const GameTerrainTile&)            = delete;
  GameTerrainTile& operator=(const GameTerrainTile&) = delete;
  GameTerrainTile(GameTerrainTile&&)                 = delete;
  GameTerrainTile& operator=(GameTerrainTile&&)      = delete;

  void Accept(GameObjectVisitor& visitor) override;

  // Registers the MeshInstance with the renderer (if mesh exists).
  void OnAddedToScene()     override;

  // Deregisters the MeshInstance and destroys the physics body.
  void OnRemovedFromScene() override;

  // Propagates world transform to the renderer instance and physics body.
  void OnWorldTransformUpdated() override;

  // Rebuilds the GPU quad mesh and static physics body from the current
  // TileDesc.
  void RegenerateMesh();

  [[nodiscard]] const track::TileDesc& GetTileDesc() const { return desc_; }
  void SetTileDesc(const track::TileDesc& desc) { desc_ = desc; }

  void SetMaterial(GameMaterial* mat);

  // Returns the material, or nullptr if none is set.
  [[nodiscard]] const GameMaterial* GetMaterialPtr() const { return material_; }

 private:
  void DestroyPhysicsBody();

  // cppcheck-suppress unusedStructMember
  track::TileDesc         desc_;
  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice*  video_;
  // cppcheck-suppress unusedStructMember
  GameMaterial*           material_ = nullptr;

  // GPU resources, rebuilt on RegenerateMesh().
  std::unique_ptr<renderer::GeometryData>  geometry_;
  std::unique_ptr<renderer::Mesh>          mesh_;
  std::unique_ptr<renderer::MeshInstance>  instance_;

  // Static physics body; non-owning (lifetime managed by PhysicsSystem).
  // cppcheck-suppress unusedStructMember
  physics::PhysicsBody*  physics_body_ = nullptr;
  // cppcheck-suppress unusedStructMember
  bool                    in_scene_    = false;
};

}  // namespace game
