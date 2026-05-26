#include "game/GameTerrain.h"

#include "terrain/TerrainData.h"
#include "terrain/TerrainMaterial.h"
#include "terrain/TerrainRenderer.h"

namespace game {

GameTerrain::GameTerrain(std::unique_ptr<terrain::TerrainData> data,
                         std::unique_ptr<terrain::TerrainMaterial> material,
                         abstract::VideoDevice* video)
    : GameObject(GameObjectType::kTerrain, core::BBox3{}),
      data_(std::move(data)),
      material_(std::move(material)),
      video_(video) {}

void GameTerrain::OnAddedToScene() {
  terrain::TerrainRenderer::Instance().Init(video_, *data_);
  terrain::TerrainRenderer::Instance().SetMaterial(material_.get());
}

void GameTerrain::OnRemovedFromScene() {
  terrain::TerrainRenderer::Instance().SetMaterial(nullptr);
}

const terrain::TerrainData& GameTerrain::GetData() const {
  return *data_;
}

const terrain::TerrainMaterial& GameTerrain::GetMaterial() const {
  return *material_;
}

}  // namespace game
