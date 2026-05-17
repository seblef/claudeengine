#include "editor/EditorScene.h"

#include <algorithm>
#include <memory>

#include "core/Color.h"
#include "core/Mat4f.h"
#include "core/Vec3f.h"
#include "game/GameLightDesc.h"
#include "game/GameMaterial.h"
#include "game/GameSystem.h"
#include "game/MeshTemplate.h"
#include "renderer/GeometryUtils.h"
#include "renderer/GlobalLight.h"
#include "renderer/Light.h"
#include "renderer/MaterialDesc.h"

namespace editor {

namespace {
game::GameLightDesc DefaultLightDesc() {
  game::GameLightDesc desc;
  desc.color     = core::Color(0.9f, 0.85f, 0.7f);
  desc.intensity = 1.2f;
  desc.direction = core::Vec3f(-0.4f, -0.8f, -0.3f).Normalized();
  return desc;
}
}  // namespace

EditorScene::EditorScene(abstract::VideoDevice* video)
    : EditorScene(video, "untitled", 120.f, DefaultLightDesc()) {}

EditorScene::EditorScene(abstract::VideoDevice* video,
                         const std::string& map_name,
                         float map_size,
                         const game::GameLightDesc& light_desc)
    : map_name_(map_name), map_size_(map_size), global_light_desc_(light_desc) {
  // Floor plane — neutral grey diffuse, map_size × map_size world units.
  auto* plane_mat = new game::GameMaterial(
      "__proc_editor_floor",
      renderer::MaterialDesc().SetDiffuseColor(core::Color(0.5f, 0.5f, 0.5f)), video);
  auto* plane_tmpl = new game::MeshTemplate(
      "__proc_editor_floor", renderer::CreatePlaneMesh(video, map_size_ / 2.f), plane_mat);
  plane_mat->Release();  // plane_tmpl holds the ref
  floor_ = std::make_unique<game::GameMesh>(plane_tmpl, /*always_visible=*/true);
  floor_->SetName("Floor");
  plane_tmpl->Release();
  game::GameSystem::Instance().AddObject(floor_.get());
  objects_.push_back(floor_.get());

  // Global directional light.
  light_ = std::make_unique<game::GameLight>(renderer::LightType::kGlobal, light_desc);
  light_->SetName("Sun");
  game::GameSystem::Instance().AddObject(light_.get());
  objects_.push_back(light_.get());

  // Unit cube scaled ×2 and placed at (0,1,0) so its bottom face sits on the floor.
  // Added as the first dynamic (user-deletable) object.
  auto* cube_mat = new game::GameMaterial(
      "__proc_editor_cube",
      renderer::MaterialDesc().SetDiffuseColor(core::Color(0.7f, 0.7f, 0.7f)), video);
  auto* cube_tmpl = new game::MeshTemplate(
      "__proc_editor_cube", renderer::CreateCubeMesh(video), cube_mat);
  cube_mat->Release();  // cube_tmpl holds the ref
  auto cube = std::make_unique<game::GameMesh>(cube_tmpl);
  cube->SetName("Cube");
  cube_tmpl->Release();
  cube->SetWorldTransform(
      core::Mat4f::Translation({0.f, 1.f, 0.f}) * core::Mat4f::Scale3D({2.f, 2.f, 2.f}));
  AddDynamicObject(std::move(cube));
}

EditorScene::~EditorScene() {
  // Unregister dynamic objects first (dynamic_objects_ is declared before the
  // static unique_ptrs, so it is destroyed last — but we unregister explicitly
  // here to keep symmetry with AddDynamicObject).
  for (auto& obj : dynamic_objects_) {
    game::GameSystem::Instance().RemoveObject(obj.get());
  }
  game::GameSystem::Instance().RemoveObject(light_.get());
  game::GameSystem::Instance().RemoveObject(floor_.get());

  for (auto* mat : game_materials_) {
    mat->Release();
  }
  for (auto* tmpl : mesh_templates_) {
    tmpl->Release();
  }
}

game::GameObject* EditorScene::AddDynamicObject(std::unique_ptr<game::GameObject> obj) {
  game::GameObject* raw = obj.get();
  dynamic_objects_.push_back(std::move(obj));
  game::GameSystem::Instance().AddObject(raw);
  objects_.push_back(raw);
  return raw;
}

void EditorScene::RemoveDynamicObject(game::GameObject* obj) {
  auto it = std::find_if(dynamic_objects_.begin(), dynamic_objects_.end(),
                         [obj](const auto& p) { return p.get() == obj; });
  if (it == dynamic_objects_.end()) {
    return;  // not a dynamic object — no-op
  }

  if (selected_ == obj) {
    selected_ = nullptr;
  }

  game::GameSystem::Instance().RemoveObject(obj);

  objects_.erase(std::remove(objects_.begin(), objects_.end(), obj), objects_.end());
  dynamic_objects_.erase(it);
}

std::unique_ptr<game::GameObject> EditorScene::ReclaimDynamicObject(
    game::GameObject* obj) {
  auto it = std::find_if(dynamic_objects_.begin(), dynamic_objects_.end(),
                         [obj](const auto& p) { return p.get() == obj; });
  if (it == dynamic_objects_.end()) return nullptr;

  if (selected_ == obj) selected_ = nullptr;

  game::GameSystem::Instance().RemoveObject(obj);
  objects_.erase(std::remove(objects_.begin(), objects_.end(), obj),
                 objects_.end());

  std::unique_ptr<game::GameObject> reclaimed = std::move(*it);
  dynamic_objects_.erase(it);
  return reclaimed;
}

bool EditorScene::IsDynamic(const game::GameObject* obj) const {
  return std::any_of(dynamic_objects_.begin(), dynamic_objects_.end(),
                     [obj](const auto& p) { return p.get() == obj; });
}

const std::string& EditorScene::GetMapName() const { return map_name_; }
void EditorScene::SetMapName(const std::string& name) { map_name_ = name; }

float EditorScene::GetMapSize() const { return map_size_; }

const std::filesystem::path& EditorScene::GetFilePath() const { return file_path_; }
void EditorScene::SetFilePath(const std::filesystem::path& p) { file_path_ = p; }

// cppcheck-suppress returnByReference
game::GameLightDesc EditorScene::GetGlobalLightDesc() const { return global_light_desc_; }

void EditorScene::SetGlobalLightDesc(const game::GameLightDesc& desc) {
  global_light_desc_ = desc;
  renderer::Light* raw = light_->GetLight();
  raw->SetColor(desc.color);
  raw->SetIntensity(desc.intensity);
  raw->SetCastShadow(desc.cast_shadow);
  raw->SetShadowResolution(desc.shadow_resolution);
  raw->SetShadowBias(desc.shadow_bias);
  auto* gl = static_cast<renderer::GlobalLight*>(raw);
  gl->SetDirection(desc.direction);
  gl->SetAmbientColor(desc.ambient_color);
}

}  // namespace editor
