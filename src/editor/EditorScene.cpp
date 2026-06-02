#include "editor/EditorScene.h"

#include <algorithm>
#include <memory>

#include "core/BBox3.h"
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
                         const game::GameLightDesc& light_desc,
                         bool add_default_objects)
    : map_name_(map_name), map_size_(map_size), global_light_desc_(light_desc),
      light_(std::make_unique<game::GameLight>(renderer::LightType::kGlobal,
                                               light_desc)) {
  light_->SetName("Sun");
  game::GameSystem::Instance().AddObject(light_.get());
  objects_.push_back(light_.get());

  if (!add_default_objects) return;

  // Floor plane — neutral grey diffuse, map_size × map_size world units.
  // Added as a dynamic (user-deletable) object so it can be selected, moved,
  // or removed when a terrain is present.
  auto* plane_mat = new game::GameMaterial(
      "__proc_editor_floor",
      renderer::MaterialDesc().SetDiffuseColor(core::Color(0.5f, 0.5f, 0.5f)), video);
  auto* plane_tmpl = new game::MeshTemplate(
      "__proc_editor_floor", renderer::CreatePlaneMesh(video, map_size_ / 2.f), plane_mat);
  plane_mat->Release();  // plane_tmpl holds the ref
  auto floor = std::make_unique<game::GameMesh>(plane_tmpl);
  floor->SetName("Floor");
  plane_tmpl->Release();
  AddDynamicObject(std::move(floor));

  // Unit cube scaled ×2 and placed at (0,1,0) so its bottom face sits on the floor.
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
  for (auto& obj : dynamic_objects_)
    game::GameSystem::Instance().RemoveObject(obj.get());
  game::GameSystem::Instance().RemoveObject(light_.get());

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
  if (on_object_added_) on_object_added_(raw);
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

  if (on_object_removed_) on_object_removed_(obj);
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

  if (on_object_removed_) on_object_removed_(obj);
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

core::BBox3 EditorScene::GetBounds() const {
  core::BBox3 result;
  bool has_any = false;
  for (const game::GameObject* obj : objects_) {
    const core::BBox3& bbox = obj->GetWorldBBox();
    if (!has_any) {
      result   = bbox;
      has_any  = true;
    } else {
      result << bbox;
    }
  }
  if (!has_any)
    return core::BBox3({-5.f, -5.f, -5.f}, {5.f, 5.f, 5.f});

  // Ensure minimum diagonal of 10 world units.
  constexpr float kMinHalf = 5.f;
  const core::Vec3f center = result.GetCenter();
  const core::Vec3f half   = result.GetSize() * 0.5f;
  const core::Vec3f expanded(
      std::max(half.x, kMinHalf),
      std::max(half.y, kMinHalf),
      std::max(half.z, kMinHalf));
  return core::BBox3(center - expanded, center + expanded);
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
