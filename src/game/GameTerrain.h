#pragma once

#include <memory>
#include <vector>

#include "core/BBox3.h"
#include "game/GameObject.h"
#include "terrain/FoliageLayer.h"

namespace abstract { class VideoDevice; }
namespace physics { class PhysicsBody; }
namespace terrain {
class TerrainData;
class TerrainMaterial;
}  // namespace terrain

namespace game {

// A game object that owns a TerrainData and TerrainMaterial and registers them
// with TerrainRenderer on scene entry.
//
// Only one GameTerrain should be added to the scene at a time; TerrainRenderer
// asserts if Init() is called twice without an intervening Shutdown().
// GameTerrain does not participate in the octree visibility system — terrain
// rendering is handled exclusively by TerrainRenderer.
class GameTerrain : public GameObject {
 public:
  // Takes ownership of data, material, and foliage layers.
  // video must outlive this object; it is used to initialise TerrainRenderer
  // and FoliageRenderer when the object enters the scene.
  GameTerrain(std::unique_ptr<terrain::TerrainData> data,
              std::unique_ptr<terrain::TerrainMaterial> material,
              abstract::VideoDevice* video);

  ~GameTerrain() override = default;

  GameTerrain(const GameTerrain&)            = delete;
  GameTerrain& operator=(const GameTerrain&) = delete;
  GameTerrain(GameTerrain&&)                 = delete;
  GameTerrain& operator=(GameTerrain&&)      = delete;

  void Accept(GameObjectVisitor& visitor) override { visitor.Visit(*this); }

  // Calls TerrainRenderer::Init() and SetMaterial().
  void OnAddedToScene() override;

  // Calls TerrainRenderer::Deinit() and FoliageRenderer::Shutdown().
  void OnRemovedFromScene() override;

  // Propagates any transform change to the physics body.
  void OnWorldTransformUpdated() override;

  [[nodiscard]] const terrain::TerrainData&     GetData()     const;
  [[nodiscard]] const terrain::TerrainMaterial& GetMaterial() const;

  // Foliage layer ownership and access.
  void AddFoliageLayer(std::unique_ptr<terrain::FoliageLayer> layer);
  void RemoveFoliageLayer(int index);
  [[nodiscard]] int                        GetFoliageLayerCount() const;
  [[nodiscard]] terrain::FoliageLayer*     GetFoliageLayer(int index) const;

 private:
  std::unique_ptr<terrain::TerrainData>                data_;
  std::unique_ptr<terrain::TerrainMaterial>            material_;
  // cppcheck-suppress unusedStructMember
  std::vector<std::unique_ptr<terrain::FoliageLayer>>  foliage_layers_;
  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice*                               video_;
  // True when OnAddedToScene() created the TerrainRenderer singleton because
  // no external owner had already done so (game app vs. editor app).
  // The paired OnRemovedFromScene() then owns the Shutdown() call.
  // cppcheck-suppress unusedStructMember
  bool owns_terrain_renderer_ = false;
  // Non-owning; lifetime is managed by PhysicsSystem.
  // cppcheck-suppress unusedStructMember
  physics::PhysicsBody* terrain_body_ = nullptr;
};

}  // namespace game
