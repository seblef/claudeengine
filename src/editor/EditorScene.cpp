#include "editor/EditorScene.h"

#include <memory>

#include "core/Color.h"
#include "core/Mat4f.h"
#include "core/Vec3f.h"
#include "game/GameLightDesc.h"
#include "game/GameMaterial.h"
#include "game/GameSystem.h"
#include "game/MeshTemplate.h"
#include "renderer/GeometryUtils.h"
#include "renderer/Light.h"
#include "renderer/MaterialDesc.h"

namespace editor {

EditorScene::EditorScene(abstract::VideoDevice* video) {
  // Floor plane — neutral grey diffuse, 120×120 world units.
  auto* plane_mat = new game::GameMaterial(
      "__proc_editor_floor",
      renderer::MaterialDesc().SetDiffuseColor(core::Color(0.5f, 0.5f, 0.5f)), video);
  materials_["floor_grey"] = plane_mat->GetMaterial();
  auto* plane_tmpl = new game::MeshTemplate(
      "__proc_editor_floor", renderer::CreatePlaneMesh(video, 60.f), plane_mat);
  plane_mat->Release();  // plane_tmpl holds the ref
  floor_ = std::make_unique<game::GameMesh>(plane_tmpl, /*always_visible=*/true);
  floor_->SetName("Floor");
  plane_tmpl->Release();
  game::GameSystem::Instance().AddObject(floor_.get());
  objects_.push_back(floor_.get());

  // Unit cube scaled ×2 and placed at (0,1,0) so its bottom face sits on the floor.
  auto* cube_mat = new game::GameMaterial(
      "__proc_editor_cube",
      renderer::MaterialDesc().SetDiffuseColor(core::Color(0.7f, 0.7f, 0.7f)), video);
  materials_["default"] = cube_mat->GetMaterial();
  auto* cube_tmpl = new game::MeshTemplate(
      "__proc_editor_cube", renderer::CreateCubeMesh(video), cube_mat);
  cube_mat->Release();  // cube_tmpl holds the ref
  cube_ = std::make_unique<game::GameMesh>(cube_tmpl);
  cube_->SetName("Cube");
  cube_tmpl->Release();
  cube_->SetWorldTransform(
      core::Mat4f::Translation({0.f, 1.f, 0.f}) * core::Mat4f::Scale3D({2.f, 2.f, 2.f}));
  game::GameSystem::Instance().AddObject(cube_.get());
  objects_.push_back(cube_.get());

  // Global directional light — warm sun-like colour from the upper-left.
  game::GameLightDesc desc;
  desc.color     = core::Color(0.9f, 0.85f, 0.7f);
  desc.intensity = 1.2f;
  desc.direction = core::Vec3f(-0.4f, -0.8f, -0.3f).Normalized();
  light_ = std::make_unique<game::GameLight>(renderer::LightType::kGlobal, desc);
  light_->SetName("Sun");
  game::GameSystem::Instance().AddObject(light_.get());
  objects_.push_back(light_.get());
}

EditorScene::~EditorScene() {
  for (auto* obj : objects_) {
    game::GameSystem::Instance().RemoveObject(obj);
  }
  for (auto* tmpl : mesh_templates_) {
    tmpl->Release();
  }
}

}  // namespace editor
