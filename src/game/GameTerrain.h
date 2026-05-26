#pragma once

#include <memory>

#include "core/BBox3.h"
#include "game/GameObject.h"

namespace abstract { class VideoDevice; }
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
  // Takes ownership of data and material. video must outlive this object and
  // is stored to initialise TerrainRenderer when the object enters the scene.
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

  // Calls TerrainRenderer::Shutdown().
  void OnRemovedFromScene() override;

  // Terrain has no movable transform; this is a no-op.
  void OnWorldTransformUpdated() override {}

  [[nodiscard]] const terrain::TerrainData&     GetData()     const;
  [[nodiscard]] const terrain::TerrainMaterial& GetMaterial() const;

 private:
  std::unique_ptr<terrain::TerrainData>     data_;
  std::unique_ptr<terrain::TerrainMaterial> material_;
  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice*                    video_;
};

}  // namespace game
