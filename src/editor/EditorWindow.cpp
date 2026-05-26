#include "editor/EditorWindow.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>
#include <loguru.hpp>
#include <nfd.h>
#include <yaml-cpp/yaml.h>

#include "abstract/VideoDevice.h"
#include "core/Config.h"
#include "core/Event.h"
#include "core/YamlUtils.h"
#include "editor/EditorScene.h"
#include "editor/EditorSystem.h"
#include "editor/EditorToolbar.h"
#include "editor/EditorViewport.h"
#include "editor/LogPanel.h"
#include "editor/MapPropertiesWindow.h"
#include "editor/MapSerializer.h"
#include "editor/MaterialEditorWindow.h"
#include "editor/MeshSelectionModal.h"
#include "editor/ObjectsPanel.h"
#include "editor/PropertiesPanel.h"
#include "editor/ResourcesPanel.h"
#include "game/GameLightDesc.h"
#include "game/GameMaterial.h"
#include "game/GameTerrain.h"
#include "game/MeshTemplate.h"
#include "renderer/Light.h"
#include "renderer/MaterialDesc.h"
#include "terrain/TerrainData.h"
#include "terrain/TerrainMaterial.h"
#include "terrain/TerrainNormalMap.h"

namespace editor {

namespace {

// Maps light-creation tools to their renderer::LightType.
std::optional<renderer::LightType> ToolToLightType(EditorTool tool) {
  switch (tool) {
    case EditorTool::kCreateOmniLight:  return renderer::LightType::kOmni;
    case EditorTool::kCreateCircleSpot: return renderer::LightType::kCircleSpot;
    case EditorTool::kCreateRectSpot:   return renderer::LightType::kRectSpot;
    default:                            return std::nullopt;
  }
}

constexpr const char* kMapFilter = "map.yaml";

}  // namespace

EditorWindow::EditorWindow(abstract::VideoDevice* video)
    : video_(video),
      scene_(std::make_unique<EditorScene>(video)),
      map_properties_(std::make_unique<MapPropertiesWindow>(scene_.get())),
      toolbar_(std::make_unique<EditorToolbar>()),
      viewport_(std::make_unique<EditorViewport>(video)),
      material_editor_(std::make_unique<MaterialEditorWindow>(video)),
      mesh_modal_(std::make_unique<MeshSelectionModal>()),
      properties_panel_(std::make_unique<PropertiesPanel>()),
      resources_panel_(std::make_unique<ResourcesPanel>()),
      objects_panel_(std::make_unique<ObjectsPanel>()),
      log_panel_(std::make_unique<LogPanel>()) {
  toolbar_->SetCommandHistory(&history_);
  toolbar_->SetOnSave([this]{ SaveCurrent(); });
  viewport_->SetScene(scene_.get());
  viewport_->SetCommandHistory(&history_);
  properties_panel_->SetCommandHistory(&history_);
  material_editor_->SetCommandHistory(&history_);
  objects_panel_->SetCommandHistory(&history_);
  history_.SetOnDirty([this]{ scene_dirty_ = true; });
  viewport_->SetOnPlacementDone([this]() {
    toolbar_->SetActiveTool(EditorTool::kSelection);
    placement_active_ = false;
  });
  resources_panel_->SetOnMaterialOpen(
      [this](game::GameMaterial* mat) { material_editor_->Open(mat); });
  resources_panel_->SetOnImportMaterial([this] { ImportMaterial(); });
  resources_panel_->SetOnImportMesh([this] { ImportMesh(); });
  resources_panel_->SetOnNewMaterial([this](std::string_view name) {
    auto* mat = new game::GameMaterial(std::string(name),
                                       renderer::MaterialDesc(), video_);
    scene_->AddGameMaterial(mat);
    material_editor_->Open(mat);
  });
  loguru::add_callback("editor_log", &LogPanel::LogCallback,
                       log_panel_.get(), loguru::Verbosity_INFO);

  const auto config_path = core::Config::GetDataFolder() / "config.yaml";
  YAML::Node root = core::LoadYamlFile(config_path);
  if (auto editor = root["editor"]) {
    if (auto interval = editor["autosave_interval_seconds"])
      autosave_interval_ = interval.as<float>();
  }
}

EditorWindow::~EditorWindow() {
  loguru::remove_callback("editor_log");
}

void EditorWindow::OnEvent(const core::Event& event) {
  viewport_->OnEvent(event);
}

void EditorWindow::Render() {
  TickAutosave();

  // 1. Full-screen DockSpace — all panels dock into it.
  ImGui::DockSpaceOverViewport();

  // 2. Main menu bar.
  RenderMenuBar();

  // 3. Toolbar — update dirty state before rendering.
  toolbar_->SetDirty(scene_dirty_);
  toolbar_->Render();
  const EditorTool active_tool = toolbar_->GetActiveTool();

  // Disable object picking while any creation tool is active.
  viewport_->SetSelectionActive(active_tool == EditorTool::kSelection);
  viewport_->SetActiveTool(active_tool);

  // Cancel placement if the user switches tool while hovering to place.
  if (active_tool != prev_tool_ && placement_active_) {
    viewport_->SetPendingMeshTemplate(nullptr);
    viewport_->SetPendingLightType(std::nullopt);
    placement_active_ = false;
  }

  // Detect tool transitions into creation tools.
  if (active_tool != prev_tool_ && IsCreationTool(active_tool)) {
    if (active_tool == EditorTool::kCreateMesh) {
      mesh_modal_->Open();
    } else if (const auto light_type = ToolToLightType(active_tool)) {
      viewport_->SetPendingLightType(light_type);
      placement_active_ = true;
      LOG_F(INFO, "Light creation tool activated, click viewport to place");
    }
  }
  prev_tool_ = active_tool;

  // Mesh selection modal — open when kCreateMesh is activated.
  if (game::MeshTemplate* tmpl = mesh_modal_->Render()) {
    viewport_->SetPendingMeshTemplate(tmpl);
    placement_active_ = true;
    LOG_F(INFO, "Mesh template selected, click viewport to place");
  }

  // 4. Viewport panel.
  constexpr ImGuiWindowFlags kViewportFlags =
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
  if (ImGui::Begin("Viewport", nullptr, kViewportFlags)) {
    viewport_->Render();
  }
  ImGui::End();

  // 5. Left panel — Resources/Objects tabs (wired in issues #175/#176).
  if (ImGui::Begin("Scene")) {
    if (ImGui::BeginTabBar("##scene_tabs")) {
      if (ImGui::BeginTabItem("Resources")) {
        resources_panel_->Render();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Objects")) {
        objects_panel_->Render(*scene_);
        ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
    }
  }
  ImGui::End();

  // 6. Right panel — Properties.
  if (ImGui::Begin("Properties"))
    properties_panel_->Render(scene_->GetSelectedObject());
  ImGui::End();

  // 7. Bottom panel — Logs (wired in issue #178).
  if (ImGui::Begin("Logs")) {
    log_panel_->Render();
  }
  ImGui::End();

  // 8. Material editor — floating window, shown when a material is open.
  material_editor_->Render(*scene_);

  // 9. Map properties panel — toggled via the Map menu.
  if (show_map_props_) {
    if (ImGui::Begin("Map Properties", &show_map_props_)) {
      if (map_properties_->RenderPanel())
        scene_dirty_ = true;
    }
    ImGui::End();
  }

  // 10. New Map modal — OpenPopup must be called outside any Begin/End pair.
  if (new_map_pending_) {
    ImGui::OpenPopup("New Map##modal");
    new_map_pending_ = false;
  }
  if (map_properties_->RenderModal()) {
    const std::string         new_name  = map_properties_->GetNewMapName();
    const float               new_size  = map_properties_->GetNewMapSize();
    const game::GameLightDesc new_light = map_properties_->GetNewMapLightDesc();
    scene_ = std::make_unique<EditorScene>(video_, new_name, new_size,
                                           new_light);
    map_properties_ = std::make_unique<MapPropertiesWindow>(scene_.get());
    viewport_->SetScene(scene_.get());
    history_.Clear();
    terrain_normal_map_.reset();
    scene_dirty_ = false;
    LOG_F(INFO, "New map '%s' created (size %.1f)", new_name.c_str(),
          new_size);
  }

  // 11. Terrain creation dialog.
  if (terrain_dialog_.Render())
    CreateTerrain();

  // 12. Unsaved changes modal — OpenPopup is triggered by CheckDirtyThenRun().
  if (open_unsaved_changes_modal_) {
    ImGui::OpenPopup("Unsaved Changes##modal");
    open_unsaved_changes_modal_ = false;
  }
  RenderUnsavedChangesModal();

  // 13. Status bar — pinned to the bottom of the screen.
  const ImGuiViewport* vp = ImGui::GetMainViewport();
  constexpr float kStatusBarHeight = 22.0f;
  ImGui::SetNextWindowPos({vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - kStatusBarHeight});
  ImGui::SetNextWindowSize({vp->WorkSize.x, kStatusBarHeight});
  constexpr ImGuiWindowFlags kStatusBarFlags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar;
  ImGui::Begin("##statusbar", nullptr, kStatusBarFlags);
  ImGui::TextUnformatted(scene_dirty_ ? "● Unsaved changes" : "");
  if (autosave_msg_timer_ > 0.f) {
    ImGui::SameLine();
    ImGui::TextUnformatted(last_autosave_msg_.c_str());
  }
  ImGui::End();
}

void EditorWindow::TickAutosave() {
  const float dt = ImGui::GetIO().DeltaTime;
  autosave_timer_ += dt;

  if (autosave_msg_timer_ > 0.f) autosave_msg_timer_ -= dt;

  if (!scene_dirty_) return;
  const std::string& map_name = scene_->GetMapName();
  if (map_name.empty() || map_name == "untitled") return;
  if (autosave_timer_ < autosave_interval_) return;

  autosave_timer_ = 0.f;
  const auto path = core::Config::GetDataFolder()
                    / "maps"
                    / (map_name + ".autosave.map.yaml");
  if (MapSerializer::Save(*scene_, viewport_->GetCameraState(), path)) {
    LOG_F(INFO, "Autosaved to %s", path.string().c_str());
    last_autosave_msg_ = "Autosaved";
    autosave_msg_timer_ = 3.f;
  } else {
    LOG_F(WARNING, "Autosave failed: %s", path.string().c_str());
  }
}

void EditorWindow::RenderMenuBar() {
  if (!ImGui::BeginMainMenuBar()) return;

  // Global keyboard shortcuts — processed regardless of menu state.
  if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)) SaveCurrent();
  if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_N)) {
    CheckDirtyThenRun([this]{ new_map_pending_ = true; });
  }

  if (ImGui::BeginMenu("File")) {
    if (ImGui::MenuItem("New", "Ctrl+N")) {
      CheckDirtyThenRun([this]{ new_map_pending_ = true; });
    }
    if (ImGui::MenuItem("Load...")) {
      CheckDirtyThenRun([this]{ LoadFromFile(); });
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Save", "Ctrl+S")) {
      SaveCurrent();
    }
    if (ImGui::MenuItem("Save As...")) {
      SaveAs();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Quit")) {
      EditorSystem::Instance().Stop();
    }
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Map")) {
    if (ImGui::MenuItem("Map Properties", nullptr, show_map_props_)) {
      show_map_props_ = !show_map_props_;
    }
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Add")) {
    const auto& objs = scene_->GetObjects();
    const bool has_terrain = std::any_of(objs.begin(), objs.end(),
        [](const game::GameObject* o) {
          return o->GetType() == game::GameObjectType::kTerrain;
        });
    ImGui::BeginDisabled(has_terrain);
    if (ImGui::MenuItem("Terrain", nullptr, false, !has_terrain))
      terrain_dialog_.Open();
    ImGui::EndDisabled();
    if (has_terrain)
      ImGui::SetItemTooltip("A terrain already exists in this scene");
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Import")) {
    if (ImGui::MenuItem("Material")) {
      ImportMaterial();
    }
    if (ImGui::MenuItem("Mesh")) {
      ImportMesh();
    }
    ImGui::EndMenu();
  }

  ImGui::EndMainMenuBar();
}

void EditorWindow::SaveCurrent() {
  if (scene_->GetFilePath().empty()) {
    SaveAs();
    return;
  }
  if (!MapSerializer::Save(*scene_, viewport_->GetCameraState(),
                           scene_->GetFilePath())) {
    LOG_F(ERROR, "Failed to save map to '%s'",
          scene_->GetFilePath().string().c_str());
    return;
  }
  scene_dirty_ = false;
  LOG_F(INFO, "Map saved to '%s'", scene_->GetFilePath().string().c_str());
}

void EditorWindow::SaveAs() {
  const std::string default_name = scene_->GetMapName() + ".map.yaml";
  nfdu8char_t* out_path = nullptr;
  const nfdu8filteritem_t filter = {"Map", kMapFilter};
  const nfdresult_t result =
      NFD_SaveDialogU8(&out_path, &filter, 1, nullptr, default_name.c_str());
  if (result != NFD_OKAY) {
    if (result == NFD_ERROR)
      LOG_F(ERROR, "NFD error opening save dialog");
    return;
  }

  std::filesystem::path path(out_path);
  NFD_FreePathU8(out_path);

  if (path.extension() != ".yaml" ||
      path.stem().extension() != ".map") {
    path += ".map.yaml";
  }

  if (!MapSerializer::Save(*scene_, viewport_->GetCameraState(), path)) {
    LOG_F(ERROR, "Failed to save map to '%s'", path.string().c_str());
    return;
  }
  scene_->SetFilePath(path);
  scene_dirty_ = false;
  LOG_F(INFO, "Map saved to '%s'", path.string().c_str());
}

void EditorWindow::LoadFromFile() {
  nfdu8char_t* out_path = nullptr;
  const nfdu8filteritem_t filter = {"Map", kMapFilter};
  const nfdresult_t result =
      NFD_OpenDialogU8(&out_path, &filter, 1, nullptr);
  if (result != NFD_OKAY) {
    if (result == NFD_ERROR)
      LOG_F(ERROR, "NFD error opening load dialog");
    return;
  }

  const std::filesystem::path path(out_path);
  NFD_FreePathU8(out_path);

  auto loaded = MapSerializer::Load(path, video_);
  if (!loaded) {
    LOG_F(ERROR, "Failed to load map from '%s'", path.string().c_str());
    return;
  }

  scene_         = std::move(loaded->scene);
  map_properties_ = std::make_unique<MapPropertiesWindow>(scene_.get());
  viewport_->SetScene(scene_.get());
  viewport_->SetCameraState(loaded->camera_state);
  if (scene_->GetSelectedObject())
    viewport_->SetSelectedObject(scene_->GetSelectedObject());
  history_.Clear();
  terrain_normal_map_.reset();
  scene_dirty_ = false;
  LOG_F(INFO, "Map loaded from '%s'", path.string().c_str());
}

void EditorWindow::CheckDirtyThenRun(std::function<void()> on_proceed) {
  if (scene_dirty_) {
    pending_after_save_ = std::move(on_proceed);
    open_unsaved_changes_modal_ = true;
  } else {
    on_proceed();
  }
}

void EditorWindow::RenderUnsavedChangesModal() {
  if (!ImGui::BeginPopupModal("Unsaved Changes##modal", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }

  ImGui::TextUnformatted("You have unsaved changes. What would you like to do?");
  ImGui::Spacing();

  if (ImGui::Button("Save")) {
    SaveCurrent();
    if (pending_after_save_) pending_after_save_();
    pending_after_save_ = nullptr;
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (ImGui::Button("Discard")) {
    if (pending_after_save_) pending_after_save_();
    pending_after_save_ = nullptr;
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (ImGui::Button("Cancel")) {
    pending_after_save_ = nullptr;
    ImGui::CloseCurrentPopup();
  }

  ImGui::EndPopup();
}

void EditorWindow::ImportMaterial() {
  nfdu8char_t* out_path = nullptr;
  const nfdu8filteritem_t filter = {"Material", "yaml"};
  const nfdresult_t result = NFD_OpenDialogU8(&out_path, &filter, 1, nullptr);
  if (result == NFD_OKAY) {
    const std::string name =
        std::filesystem::path(out_path).stem().string();
    game::GameMaterial* mat = game::GameMaterial::GetOrLoad(name, video_);
    scene_->AddGameMaterial(mat);
    LOG_F(INFO, "Imported material '%s' from '%s'", name.c_str(), out_path);
    NFD_FreePathU8(out_path);
  } else if (result == NFD_ERROR) {
    LOG_F(ERROR, "NFD error opening material dialog");
  }
}

void EditorWindow::ImportMesh() {
  nfdu8char_t* out_path = nullptr;
  const nfdu8filteritem_t filter = {"Mesh", "obj,fbx,emesh"};
  const nfdresult_t result = NFD_OpenDialogU8(&out_path, &filter, 1, nullptr);
  if (result == NFD_OKAY) {
    game::MeshTemplate* tmpl =
        game::MeshTemplate::GetOrLoad(out_path, video_);
    if (tmpl) {
      scene_->AddMeshTemplate(tmpl);
      LOG_F(INFO, "Imported mesh '%s'", out_path);
    } else {
      LOG_F(ERROR, "Failed to import mesh '%s'", out_path);
    }
    NFD_FreePathU8(out_path);
  } else if (result == NFD_ERROR) {
    LOG_F(ERROR, "NFD error opening mesh dialog");
  }
}

void EditorWindow::CreateTerrain() {
  const TerrainCreationDialog::Params& p = terrain_dialog_.GetParams();

  const float safe_res = std::max(0.001f, p.resolution);
  const int   width    = std::max(2, static_cast<int>(
                             std::round(p.size_x / safe_res)));
  const int   height   = std::max(2, static_cast<int>(
                             std::round(p.size_z / safe_res)));

  // 1. Allocate TerrainData — all samples at min_height (uint16 = 0).
  std::vector<uint16_t> heights(static_cast<std::size_t>(width) * height, 0u);
  auto data = std::make_unique<terrain::TerrainData>(
      heights.data(), width, height, safe_res, p.min_height, p.max_height);

  // 2. Allocate TerrainMaterial with a single default layer.
  YAML::Node mat_node;
  YAML::Node layer0;
  layer0["albedo"] = std::string("default/diffuse.png");
  layer0["normal"] = std::string("default/normal.png");
  layer0["tiling"] = 8.f;
  mat_node["layers"].push_back(layer0);
  auto material = std::make_unique<terrain::TerrainMaterial>();
  material->Load(mat_node, video_, width, height);

  // 3. Compute initial normal map.
  terrain_normal_map_ = std::make_unique<terrain::TerrainNormalMap>();
  terrain_normal_map_->Build(*data);

  // 4. Construct GameTerrain and add it to the scene.
  //    OnAddedToScene() calls TerrainRenderer::Init() (heightmap GPU upload)
  //    and SetMaterial() (splatmap binding).
  auto terrain_obj = std::make_unique<game::GameTerrain>(
      std::move(data), std::move(material), video_);
  terrain_obj->SetName("terrain");
  scene_->AddDynamicObject(std::move(terrain_obj));

  // 5. Upload the normal map GPU texture.
  terrain_normal_map_->Upload(video_);

  scene_dirty_ = true;
  LOG_F(INFO, "Terrain created: %dx%d texels, %.2f m/texel, "
        "Y=[%.1f, %.1f]", width, height, safe_res, p.min_height, p.max_height);
}

}  // namespace editor
