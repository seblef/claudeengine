#include "game/GameTerrain.h"

#include <algorithm>

#include "physics/PhysicsSystem.h"
#include "renderer/FoliageRenderer.h"
#include "renderer/Renderer.h"
#include "terrain/FoliageLayer.h"
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
  // Create the TerrainRenderer singleton when no external owner has done so
  // (game app path). The editor_app pre-creates it before any scene is loaded.
  owns_terrain_renderer_ = !terrain::TerrainRenderer::IsInstanced();
  if (owns_terrain_renderer_) {
    new terrain::TerrainRenderer();
    renderer::Renderer::Instance().SetTerrainRenderer(
        &terrain::TerrainRenderer::Instance());
  }
  terrain::TerrainRenderer::Instance().Init(video_, *data_);
  terrain::TerrainRenderer::Instance().SetMaterial(material_.get());

  if (physics::PhysicsSystem::IsInstanced())
    terrain_body_ = physics::PhysicsSystem::Instance().CreateTerrainBody(
        data_.get(), GetWorldTransform());

  // Always create the FoliageRenderer singleton so the editor can add layers
  // and trigger builds interactively without needing a scene reload.
  std::vector<terrain::FoliageLayer*> ptrs;
  ptrs.reserve(foliage_layers_.size());
  std::transform(foliage_layers_.begin(), foliage_layers_.end(),
                 std::back_inserter(ptrs),
                 [](const std::unique_ptr<terrain::FoliageLayer>& l) {
                   return l.get();
                 });
  new renderer::FoliageRenderer();
  renderer::FoliageRenderer::Instance().Build(video_, *data_, ptrs);
  if (!ptrs.empty())
    renderer::Renderer::Instance().SetFoliageEnabled(true);
}

void GameTerrain::OnRemovedFromScene() {
  if (terrain_body_) {
    physics::PhysicsSystem::Instance().DestroyBody(terrain_body_);
    terrain_body_ = nullptr;
  }

  terrain::TerrainRenderer::Instance().Deinit();
  if (owns_terrain_renderer_) {
    renderer::Renderer::Instance().SetTerrainRenderer(nullptr);
    terrain::TerrainRenderer::Shutdown();
    owns_terrain_renderer_ = false;
  }
  if (renderer::FoliageRenderer::IsInstanced())
    renderer::FoliageRenderer::Shutdown();
  renderer::Renderer::Instance().SetFoliageEnabled(false);
}

void GameTerrain::OnWorldTransformUpdated() {
  if (terrain_body_)
    terrain_body_->SetWorldTransform(GetWorldTransform());
}

const terrain::TerrainData& GameTerrain::GetData() const {
  return *data_;
}

const terrain::TerrainMaterial& GameTerrain::GetMaterial() const {
  return *material_;
}

void GameTerrain::AddFoliageLayer(std::unique_ptr<terrain::FoliageLayer> layer) {
  foliage_layers_.push_back(std::move(layer));
}

void GameTerrain::RemoveFoliageLayer(int index) {
  if (index >= 0 && index < static_cast<int>(foliage_layers_.size()))
    foliage_layers_.erase(foliage_layers_.begin() + index);
}

int GameTerrain::GetFoliageLayerCount() const {
  return static_cast<int>(foliage_layers_.size());
}

terrain::FoliageLayer* GameTerrain::GetFoliageLayer(int index) const {
  if (index >= 0 && index < static_cast<int>(foliage_layers_.size()))
    return foliage_layers_[index].get();
  return nullptr;
}

}  // namespace game
