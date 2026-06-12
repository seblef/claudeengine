#include "editor/EditorWindow.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>
#include <loguru.hpp>
#include <nfd.h>
#include <yaml-cpp/yaml.h>

#include "abstract/VideoDevice.h"
#include "core/AppConfig.h"
#include "core/Config.h"
#include "core/Event.h"
#include "core/Vec3f.h"
#include "core/YamlUtils.h"
#include "editor/EditorScene.h"
#include "editor/EditorSystem.h"
#include "editor/EditorToolbar.h"
#include "editor/commands/PlaceObjectCommand.h"
#include "editor/commands/TransformCommand.h"
#include "editor/EditorViewport.h"
#include "editor/LogPanel.h"
#include "editor/MapPropertiesWindow.h"
#include "editor/MapSerializer.h"
#include "editor/MaterialEditorWindow.h"
#include "editor/MeshEditorWindow.h"
#include "editor/ParticleEditorWindow.h"
#include "editor/MeshSelectionModal.h"
#include "editor/ParticleSystemSelectionModal.h"
#include "editor/ObjectsPanel.h"
#include "editor/PropertiesPanel.h"
#include "editor/ResourcesPanel.h"
#include "game/GameLightDesc.h"
#include "game/GameMaterial.h"
#include "game/GameObjectType.h"
#include "game/GameTerrain.h"
#include "game/MeshTemplate.h"
#include "renderer/Light.h"
#include "renderer/MaterialDesc.h"
#include "renderer/PostProcessInfos.h"
#include "renderer/Renderer.h"
#include "terrain/TerrainData.h"
#include "terrain/TerrainMaterial.h"
#include "terrain/TerrainRenderer.h"

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
      mesh_editor_(std::make_unique<MeshEditorWindow>(video,
                                                       material_editor_.get())),
      particle_editor_(std::make_unique<ParticleEditorWindow>(video)),
      mesh_modal_(std::make_unique<MeshSelectionModal>()),
      particle_modal_(std::make_unique<ParticleSystemSelectionModal>()),
      properties_panel_(std::make_unique<PropertiesPanel>()),
      resources_panel_(std::make_unique<ResourcesPanel>()),
      objects_panel_(std::make_unique<ObjectsPanel>()),
      log_panel_(std::make_unique<LogPanel>()) {
  toolbar_->SetCommandHistory(&history_);
  toolbar_->SetOnSave([this]{ SaveCurrent(); });
  toolbar_->SetOnCopy([this]{ CopySelectedObject(); });
  toolbar_->SetOnPaste([this]{ PasteObject(); });
  toolbar_->SetOnFallToTerrain([this]{ FallToTerrain(); });
  toolbar_->SetOnCenterOnObject([this]{ CenterCameraOnObject(); });
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
  resources_panel_->SetOnMeshEdit([this](const game::MeshTemplate* tmpl) {
    if (tmpl) mesh_editor_->OpenEdit(tmpl->GetId());
  });
  resources_panel_->SetOnParticleOpen([this](const std::string& name) {
    show_particle_editor_ = true;
    particle_editor_->OpenTemplate(name);
  });
  properties_panel_->SetOnOpenParticleEditor([this](const std::string& name) {
    show_particle_editor_ = true;
    particle_editor_->OpenTemplate(name);
  });
  resources_panel_->SetOnNewMaterial([this](std::string_view name) {
    auto* mat = new game::GameMaterial(std::string(name),
                                       renderer::MaterialDesc(), video_);
    scene_->AddGameMaterial(mat);
    material_editor_->Open(mat);
  });
  environment_panel_.SetContext(scene_.get(), video_);
  terrain_panel_.SetOnCreateFromImport(
      [this](std::vector<uint16_t> s, int w, int h, float mn, float mx) {
        CreateTerrainFromImport(std::move(s), w, h, mn, mx);
      });

  loguru::add_callback("editor_log", &LogPanel::LogCallback,
                       log_panel_.get(), loguru::Verbosity_INFO);

  const auto config_path = core::Config::GetDataFolder() / "config.yaml";
  YAML::Node root = core::LoadYamlFile(config_path);
  if (auto editor = root["editor"]) {
    if (auto interval = editor["autosave_interval_seconds"])
      autosave_interval_ = interval.as<float>();
    if (auto recent = editor["recent_maps"]) {
      std::transform(recent.begin(), recent.end(),
                     std::back_inserter(recent_maps_),
                     [](const YAML::Node& n) { return n.as<std::string>(); });
    }
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
  environment_panel_.Tick(ImGui::GetIO().DeltaTime);
  scene_->Update(static_cast<float>(ImGui::GetTime()), ImGui::GetIO().DeltaTime);

  // 1. Full-screen DockSpace — all panels dock into it.
  ImGui::DockSpaceOverViewport();

  // 2. Main menu bar.
  RenderMenuBar();

  // 3. Toolbar — update dirty state and copy/paste availability before rendering.
  toolbar_->SetDirty(scene_dirty_);
  {
    const game::GameObject* sel = scene_->GetSelectedObject();
    const bool can_copy = sel &&
        (sel->GetType() == game::GameObjectType::kMesh ||
         sel->GetType() == game::GameObjectType::kLight);
    toolbar_->SetCanCopy(can_copy);
    toolbar_->SetCanPaste(clipboard_ != nullptr);
    const bool can_fall = terrain_panel_.IsActive() && sel &&
        sel->GetType() != game::GameObjectType::kTerrain;
    toolbar_->SetCanFallToTerrain(can_fall);
    const bool can_center = sel &&
        sel->GetType() != game::GameObjectType::kTerrain;
    toolbar_->SetCanCenterOnObject(can_center);
  }
  toolbar_->Render();
  const EditorTool active_tool = toolbar_->GetActiveTool();

  // Disable object picking while any creation tool is active.
  // Transform tools keep picking enabled so the user can select a different
  // object by clicking outside the gizmo (issue #505).
  viewport_->SetSelectionActive(active_tool == EditorTool::kSelection ||
                                IsTransformTool(active_tool));
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
    } else if (active_tool == EditorTool::kCreatePlayerStart) {
      viewport_->SetPendingPlayerStart();
      placement_active_ = true;
      LOG_F(INFO, "Player start creation tool activated, click viewport to place");
    } else if (active_tool == EditorTool::kCreateParticleSystem) {
      particle_modal_->Open();
    }
  }
  prev_tool_ = active_tool;

  // Mesh selection modal — open when kCreateMesh is activated.
  if (game::MeshTemplate* tmpl = mesh_modal_->Render()) {
    viewport_->SetPendingMeshTemplate(tmpl);
    placement_active_ = true;
    LOG_F(INFO, "Mesh template selected, click viewport to place");
  }

  // Particle system selection modal — open when kCreateParticleSystem activated.
  if (const std::string ps_name = particle_modal_->Render(); !ps_name.empty()) {
    viewport_->SetPendingParticleSystem(ps_name);
    placement_active_ = true;
    LOG_F(INFO, "Particle system '%s' selected, click viewport to place",
          ps_name.c_str());
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

  // 8a. Mesh editor — floating window, shown when a mesh import/edit is active.
  mesh_editor_->Render(scene_.get());

  // 8b. Particle editor — floating window, toggled via the Effects menu.
  if (show_particle_editor_) {
    particle_editor_->Render(
        static_cast<float>(ImGui::GetTime()),
        ImGui::GetIO().DeltaTime);
    show_particle_editor_ = particle_editor_->IsOpen();
  }

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
    scene_.reset();
    renderer::Renderer::Instance().InitVisibilitySystems(new_size);
    scene_ = std::make_unique<EditorScene>(video_, new_name, new_size,
                                           new_light);
    map_properties_ = std::make_unique<MapPropertiesWindow>(scene_.get());
    viewport_->SetScene(scene_.get());
    history_.Clear();
    WireTerrainPanel();
    environment_panel_.SetContext(scene_.get(), video_);
    scene_dirty_ = false;
    LOG_F(INFO, "New map '%s' created (size %.1f)", new_name.c_str(),
          new_size);
  }

  // 11. Terrain creation dialog.
  if (terrain_dialog_.Render())
    CreateTerrain();

  // 11a. Terrain import window — floating, opened via Terrain > Import.
  terrain_panel_.RenderImportWindow();

  // 11a''. Terrain generate window — floating, opened via Terrain > Generate.
  terrain_panel_.RenderGenerateWindow();

  // 11a'. Terrain auto-paint window — floating, opened from the Paint tab.
  terrain_panel_.RenderPainterWindow();

  // 11b. Confirm remove terrain modal — triggered by Terrain > Remove.
  if (confirm_remove_terrain_) {
    ImGui::OpenPopup("Remove Terrain?##modal");
    confirm_remove_terrain_ = false;
  }
  if (ImGui::BeginPopupModal("Remove Terrain?##modal", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted("Remove the terrain from this scene?");
    ImGui::TextWrapped("This action cannot be undone via Ctrl+Z.");
    ImGui::Spacing();
    if (ImGui::Button("Remove", {120.f, 0.f})) {
      RemoveTerrain();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", {120.f, 0.f}))
      ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }

  // 11d. Terrain editor panel — dockable, shown via Terrain menu.
  if (show_terrain_panel_) {
    if (ImGui::Begin("Terrain", &show_terrain_panel_)) {
      terrain_panel_.Render();
    }
    ImGui::End();
    // Sculpt active only while the panel is open and context is set.
    viewport_->SetSculptActive(terrain_panel_.IsActive());
  } else {
    viewport_->SetSculptActive(false);
  }

  // 11e. Environment editor panel — dockable, shown via Map > Environment.
  if (show_environment_panel_) {
    if (ImGui::Begin("Environment##panel", &show_environment_panel_)) {
      if (environment_panel_.Render()) scene_dirty_ = true;
    }
    ImGui::End();
  }

  // 11f. Post-process panel — dockable, shown via View > Post-process.
  if (show_post_process_panel_) {
    if (ImGui::Begin("Post-process##panel", &show_post_process_panel_)) {
      renderer::PostProcessInfos& pp =
          renderer::Renderer::Instance().GetPostProcessInfos();
      const auto& cfg = core::AppConfig::GetPostProcess();
      if (ImGui::CollapsingHeader("Post-process", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (!cfg.IsEyeAdaptationEnabled()) {
          ImGui::SliderFloat("Exposure", &pp.exposure, 0.1f, 10.f, "%.2f");
        }
        if (cfg.IsBloomEnabled()) {
          ImGui::SliderFloat("Bloom intensity",  &pp.bloom_intensity,  0.f, 2.f, "%.2f");
          ImGui::SliderFloat("Bloom threshold",  &pp.bloom_threshold,  0.f, 3.f, "%.2f");
        }
        if (cfg.IsEyeAdaptationEnabled()) {
          ImGui::SliderFloat("Adapt speed", &pp.adapt_speed, 0.1f, 5.f, "%.2f");
        }
      }
    }
    ImGui::End();
  }

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

  // Global keyboard shortcuts — skip when a text input widget has keyboard focus.
  if (!ImGui::GetIO().WantCaptureKeyboard) {
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)) SaveCurrent();
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_N))
      CheckDirtyThenRun([this]{ new_map_pending_ = true; });
  }

  if (ImGui::BeginMenu("File")) {
    if (ImGui::MenuItem("New", "Ctrl+N")) {
      CheckDirtyThenRun([this]{ new_map_pending_ = true; });
    }
    if (ImGui::MenuItem("Load...")) {
      CheckDirtyThenRun([this]{ LoadFromFile(); });
    }
    if (ImGui::BeginMenu("Recent", !recent_maps_.empty())) {
      for (int i = 0; i < static_cast<int>(recent_maps_.size()); ++i) {
        const std::string& p = recent_maps_[i];
        const std::string  label =
            std::filesystem::path(p).filename().string() +
            "##recent" + std::to_string(i);
        if (ImGui::MenuItem(label.c_str()))
          CheckDirtyThenRun([this, p]{ LoadMap(std::filesystem::path(p)); });
        ImGui::SetItemTooltip("%s", p.c_str());
      }
      ImGui::EndMenu();
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
    if (ImGui::MenuItem("Environment", nullptr, show_environment_panel_)) {
      show_environment_panel_ = !show_environment_panel_;
    }
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Terrain")) {
    const bool has_terrain = terrain_panel_.IsActive();

    if (!has_terrain) {
      if (ImGui::MenuItem("Add Terrain"))
        terrain_dialog_.Open();
    } else {
      ImGui::BeginDisabled();
      ImGui::MenuItem("Add Terrain");
      ImGui::EndDisabled();
      ImGui::SetItemTooltip("A terrain already exists in this scene");
    }

    ImGui::Separator();

    ImGui::BeginDisabled(!has_terrain);
    if (ImGui::MenuItem("Sculpt", nullptr, show_terrain_panel_, has_terrain))
      show_terrain_panel_ = !show_terrain_panel_;
    ImGui::EndDisabled();
    if (!has_terrain)
      ImGui::SetItemTooltip("Add a terrain first");

    if (ImGui::MenuItem("Import"))
      terrain_panel_.OpenImportWindow();
    if (ImGui::MenuItem("Generate..."))
      terrain_panel_.OpenGenerateWindow();

    ImGui::Separator();

    ImGui::BeginDisabled(!has_terrain);
    if (ImGui::MenuItem("Remove", nullptr, false, has_terrain))
      confirm_remove_terrain_ = true;
    ImGui::EndDisabled();
    if (!has_terrain)
      ImGui::SetItemTooltip("No terrain to remove");

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

  if (ImGui::BeginMenu("Effects")) {
    if (ImGui::MenuItem("Particle Editor", nullptr,
                         show_particle_editor_)) {
      show_particle_editor_ = !show_particle_editor_;
      if (show_particle_editor_) particle_editor_->Open();
    }
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("View")) {
    if (ImGui::MenuItem("Post-process", nullptr, show_post_process_panel_))
      show_post_process_panel_ = !show_post_process_panel_;
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Debug")) {
    if (ImGui::MenuItem("Terrain Wireframe", nullptr, terrain_wireframe_debug_)) {
      terrain_wireframe_debug_ = !terrain_wireframe_debug_;
      viewport_->SetTerrainWireframeDebugEnabled(terrain_wireframe_debug_);
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
  AddToRecentMaps(path);
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
  LoadMap(path);
}

void EditorWindow::LoadMap(const std::filesystem::path& path) {
  // Peek the map size so the octree can be correctly sized before objects are
  // inserted.  MapLoader will re-parse the full file; this first read is cheap.
  float map_size = 120.f;
  try {
    const YAML::Node root = YAML::LoadFile(path.string());
    map_size = root["map_size"].as<float>(120.f);
  } catch (...) {}

  // Destroy the old scene so the octree is empty before rebuilding it.
  scene_.reset();
  renderer::Renderer::Instance().InitVisibilitySystems(map_size);

  auto loaded = MapSerializer::Load(path, video_);
  if (!loaded) {
    LOG_F(ERROR, "Failed to load map from '%s'", path.string().c_str());
    scene_ = std::make_unique<EditorScene>(video_);
    map_properties_ = std::make_unique<MapPropertiesWindow>(scene_.get());
    viewport_->SetScene(scene_.get());
    return;
  }

  scene_          = std::move(loaded->scene);
  map_properties_ = std::make_unique<MapPropertiesWindow>(scene_.get());
  viewport_->SetScene(scene_.get());
  viewport_->SetCameraState(loaded->camera_state);
  if (scene_->GetSelectedObject())
    viewport_->SetSelectedObject(scene_->GetSelectedObject());
  history_.Clear();
  WireTerrainPanel();
  environment_panel_.SetContext(scene_.get(), video_);
  scene_dirty_ = false;
  AddToRecentMaps(path);
  LOG_F(INFO, "Map loaded from '%s'", path.string().c_str());
}

void EditorWindow::AddToRecentMaps(const std::filesystem::path& path) {
  const std::string path_str = path.string();
  auto it = std::find(recent_maps_.begin(), recent_maps_.end(), path_str);
  if (it != recent_maps_.end())
    recent_maps_.erase(it);
  recent_maps_.insert(recent_maps_.begin(), path_str);
  if (static_cast<int>(recent_maps_.size()) > kMaxRecentMaps)
    recent_maps_.resize(kMaxRecentMaps);
  SaveRecentMaps();
}

void EditorWindow::SaveRecentMaps() {
  const auto config_path = core::Config::GetDataFolder() / "config.yaml";
  YAML::Node root;
  try {
    root = core::LoadYamlFile(config_path);
  } catch (...) {}

  root["editor"]["recent_maps"] = recent_maps_;

  std::ofstream out(config_path);
  out << root;
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
  if (result != NFD_OKAY) {
    if (result == NFD_ERROR)
      LOG_F(ERROR, "NFD error opening mesh dialog");
    return;
  }

  const std::filesystem::path path(out_path);
  NFD_FreePathU8(out_path);

  const std::string ext = path.extension().string();
  // Lowercase extension.
  std::string ext_lower = ext;
  std::transform(ext_lower.begin(), ext_lower.end(), ext_lower.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  if (ext_lower == ".fbx" || ext_lower == ".obj") {
    mesh_editor_->OpenImport(path);
  } else if (ext_lower == ".emesh") {
    game::MeshTemplate* tmpl =
        game::MeshTemplate::GetOrLoad(path.string(), video_);
    if (tmpl) {
      scene_->AddMeshTemplate(tmpl);
      LOG_F(INFO, "Imported emesh '%s'", path.filename().string().c_str());
    } else {
      LOG_F(ERROR, "ImportMesh: failed to load emesh '%s'",
            path.string().c_str());
    }
  } else {
    LOG_F(ERROR, "ImportMesh: unsupported extension '%s'", ext.c_str());
  }
}

namespace {
// Returns the first GameTerrain object in the scene, or nullptr.
game::GameTerrain* FindTerrain(const EditorScene& scene) {
  const auto& objs = scene.GetObjects();
  const auto it = std::find_if(objs.begin(), objs.end(), [](const game::GameObject* o) {
    return o->GetType() == game::GameObjectType::kTerrain;
  });
  return it != objs.end() ? static_cast<game::GameTerrain*>(*it) : nullptr;
}
}  // namespace

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

  // 3. Construct GameTerrain and add it to the scene.
  //    OnAddedToScene() calls TerrainRenderer::Init() (heightmap GPU upload)
  //    and SetMaterial() (splatmap binding).
  auto terrain_obj = std::make_unique<game::GameTerrain>(
      std::move(data), std::move(material), video_);
  terrain_obj->SetName("terrain");
  scene_->AddDynamicObject(std::move(terrain_obj));

  // 5. Wire the terrain sculpt panel.
  WireTerrainPanel();

  scene_dirty_ = true;
  LOG_F(INFO, "Terrain created: %dx%d texels, %.2f m/texel, "
        "Y=[%.1f, %.1f]", width, height, safe_res, p.min_height, p.max_height);
}

void EditorWindow::CreateTerrainFromImport(std::vector<uint16_t> samples,
                                           int w, int h,
                                           float min_h, float max_h) {
  constexpr float kDefaultResolution = 1.f;

  auto data = std::make_unique<terrain::TerrainData>(
      samples.data(), w, h, kDefaultResolution, min_h, max_h);

  YAML::Node mat_node;
  YAML::Node layer0;
  layer0["albedo"] = std::string("default/diffuse.png");
  layer0["normal"] = std::string("default/normal.png");
  layer0["tiling"] = 8.f;
  mat_node["layers"].push_back(layer0);
  auto material = std::make_unique<terrain::TerrainMaterial>();
  material->Load(mat_node, video_, w, h);

  auto terrain_obj = std::make_unique<game::GameTerrain>(
      std::move(data), std::move(material), video_);
  terrain_obj->SetName("terrain");
  scene_->AddDynamicObject(std::move(terrain_obj));

  WireTerrainPanel();
  scene_dirty_ = true;
  LOG_F(INFO, "Terrain created from import: %dx%d texels, 1.0 m/texel, "
        "Y=[%.1f, %.1f]", w, h, min_h, max_h);
}

void EditorWindow::RemoveTerrain() {
  game::GameTerrain* gt = FindTerrain(*scene_);
  if (!gt) return;

  scene_->RemoveDynamicObject(gt);
  show_terrain_panel_ = false;
  history_.Clear();
  WireTerrainPanel();
  scene_dirty_ = true;
  LOG_F(INFO, "Terrain removed from scene");
}

void EditorWindow::CopySelectedObject() {
  const game::GameObject* obj = scene_->GetSelectedObject();
  if (!obj) return;
  const core::Mat4f& t = obj->GetWorldTransform();
  const core::Vec3f pos(t(3, 0), t(3, 1), t(3, 2));
  clipboard_ = obj->Copy(pos);
  if (clipboard_)
    LOG_F(INFO, "Copied '%s'", clipboard_->GetName().c_str());
}

void EditorWindow::PasteObject() {
  if (!clipboard_) return;
  const core::Mat4f& ct = clipboard_->GetWorldTransform();
  const core::Vec3f paste_pos(ct(3, 0) + 1.f, ct(3, 1), ct(3, 2) + 1.f);
  auto clone = clipboard_->Copy(paste_pos);
  if (!clone) return;
  history_.Push(std::make_unique<PlaceObjectCommand>(scene_.get(), std::move(clone)));
  scene_dirty_ = true;
}

void EditorWindow::FallToTerrain() {
  game::GameObject* obj = scene_->GetSelectedObject();
  if (!obj || obj->GetType() == game::GameObjectType::kTerrain) return;

  const game::GameTerrain* gt = FindTerrain(*scene_);
  if (!gt) return;

  const terrain::TerrainData& data = gt->GetData();
  const core::Mat4f& wt = obj->GetWorldTransform();

  // Object pivot XZ position.
  const float px = wt(0, 3);
  const float pz = wt(2, 3);

  // Terrain surface height and slope normal at the pivot XZ.
  const float terrain_h = data.GetHeight(px, pz);
  const core::Vec3f N   = data.GetNormal(px, pz);

  // Extract per-axis scale from the current transform's column magnitudes.
  const float sx = std::sqrt(wt(0, 0)*wt(0, 0) + wt(1, 0)*wt(1, 0) + wt(2, 0)*wt(2, 0));
  const float sy = std::sqrt(wt(0, 1)*wt(0, 1) + wt(1, 1)*wt(1, 1) + wt(2, 1)*wt(2, 1));
  const float sz = std::sqrt(wt(0, 2)*wt(0, 2) + wt(1, 2)*wt(1, 2) + wt(2, 2)*wt(2, 2));

  // Horizontal projection of the current Z-axis (forward); preserve yaw.
  core::Vec3f fwd(wt(0, 2), 0.f, wt(2, 2));
  const float fwd_len = fwd.Length();
  if (fwd_len < 1e-5f)
    fwd = core::Vec3f(0.f, 0.f, 1.f);
  else
    fwd = fwd / fwd_len;

  // Build an orthonormal frame whose Y-axis is the terrain normal (slope align).
  const core::Vec3f new_right   = N.Cross(fwd).Normalized();
  const core::Vec3f new_forward = new_right.Cross(N).Normalized();

  // Place the pivot so the local bbox bottom (min_y) touches the terrain surface.
  // The bbox-bottom offset in world space is N * local_min_y.
  const float local_min_y = obj->GetLocalBBox().GetMin().y;
  const float new_px = px - local_min_y * N.x;
  const float new_py = terrain_h - local_min_y * N.y;
  const float new_pz = pz - local_min_y * N.z;

  // Compose new world transform: (rotation * scale) in columns 0-2, position in column 3.
  const core::Mat4f new_transform(
    new_right.x * sx,   N.x * sy,   new_forward.x * sz,   new_px,
    new_right.y * sx,   N.y * sy,   new_forward.y * sz,   new_py,
    new_right.z * sx,   N.z * sy,   new_forward.z * sz,   new_pz,
    0.f,                0.f,        0.f,                   1.f);

  const core::Mat4f before = wt;
  obj->SetWorldTransform(new_transform);
  history_.Push(std::make_unique<TransformCommand>(obj, before, new_transform));
  scene_dirty_ = true;

  LOG_F(INFO, "Fell '%s' to terrain: y=%.2f, normal=(%.2f, %.2f, %.2f)",
        obj->GetName().c_str(), terrain_h, N.x, N.y, N.z);
}

void EditorWindow::CenterCameraOnObject() {
  const game::GameObject* obj = scene_->GetSelectedObject();
  if (!obj || obj->GetType() == game::GameObjectType::kTerrain) return;

  viewport_->FrameObject(obj->GetWorldBBox());
  LOG_F(INFO, "Camera centered on '%s'", obj->GetName().c_str());
}

void EditorWindow::WireTerrainPanel() {
  game::GameTerrain* gt = FindTerrain(*scene_);
  if (!gt) {
    terrain_panel_.SetContext(nullptr, nullptr, nullptr, nullptr, nullptr);
    viewport_->SetTerrainData(nullptr);
    viewport_->SetSculptActive(false);
    return;
  }

  // GameTerrain owns const data. The editor mutates them during sculpt/paint —
  // const_cast is safe because the objects are non-const.
  auto* data     = const_cast<terrain::TerrainData*>(&gt->GetData());
  auto* material = const_cast<terrain::TerrainMaterial*>(&gt->GetMaterial());
  terrain_panel_.SetContext(data, material, video_, &history_, gt);
  terrain_panel_.SetOnFoliageModified([this]{ scene_dirty_ = true; });
  viewport_->SetTerrainData(data);
  viewport_->SetOnSculptBrush([this](float wx, float wz, bool first, float dt) {
    terrain_panel_.OnBrushAt(wx, wz, first, dt);
  });
  viewport_->SetOnSculptEnd([this]() {
    terrain_panel_.OnBrushEnd();
  });
}

}  // namespace editor
