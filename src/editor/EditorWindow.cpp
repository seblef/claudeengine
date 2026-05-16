#include "editor/EditorWindow.h"

#include <filesystem>

#include <imgui.h>
#include <loguru.hpp>
#include <nfd.h>

#include "core/Event.h"
#include "editor/EditorScene.h"
#include "editor/EditorSystem.h"
#include "editor/EditorToolbar.h"
#include "editor/EditorViewport.h"
#include "editor/LogPanel.h"
#include "editor/MaterialEditorWindow.h"
#include "editor/MeshSelectionModal.h"
#include "editor/ObjectsPanel.h"
#include "editor/PropertiesPanel.h"
#include "editor/ResourcesPanel.h"
#include "game/GameMaterial.h"
#include "game/MeshTemplate.h"
#include "renderer/Light.h"

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

}  // namespace

EditorWindow::EditorWindow(abstract::VideoDevice* video)
    : video_(video),
      scene_(std::make_unique<EditorScene>(video)),
      toolbar_(std::make_unique<EditorToolbar>()),
      viewport_(std::make_unique<EditorViewport>(video)),
      material_editor_(std::make_unique<MaterialEditorWindow>(video)),
      mesh_modal_(std::make_unique<MeshSelectionModal>()),
      properties_panel_(std::make_unique<PropertiesPanel>()),
      resources_panel_(std::make_unique<ResourcesPanel>()),
      objects_panel_(std::make_unique<ObjectsPanel>()),
      log_panel_(std::make_unique<LogPanel>()) {
  toolbar_->SetCommandHistory(&history_);
  viewport_->SetScene(scene_.get());
  viewport_->SetOnPlacementDone([this]() {
    toolbar_->SetActiveTool(EditorTool::kSelection);
    placement_active_ = false;
  });
  resources_panel_->SetOnMaterialOpen(
      [this](game::GameMaterial* mat) { material_editor_->Open(mat); });
  loguru::add_callback("editor_log", &LogPanel::LogCallback,
                       log_panel_.get(), loguru::Verbosity_INFO);
}

EditorWindow::~EditorWindow() {
  loguru::remove_callback("editor_log");
}

void EditorWindow::OnEvent(const core::Event& event) {
  viewport_->OnEvent(event);
}

void EditorWindow::Render() {
  // 1. Full-screen DockSpace — all panels dock into it.
  ImGui::DockSpaceOverViewport();

  // 2. Main menu bar.
  RenderMenuBar();

  // 3. Toolbar.
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

  // 9. Status bar — pinned to the bottom of the screen.
  const ImGuiViewport* vp = ImGui::GetMainViewport();
  constexpr float kStatusBarHeight = 22.0f;
  ImGui::SetNextWindowPos({vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - kStatusBarHeight});
  ImGui::SetNextWindowSize({vp->WorkSize.x, kStatusBarHeight});
  constexpr ImGuiWindowFlags kStatusBarFlags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar;
  ImGui::Begin("##statusbar", nullptr, kStatusBarFlags);
  ImGui::TextUnformatted("");
  ImGui::End();
}

void EditorWindow::RenderMenuBar() {
  if (!ImGui::BeginMainMenuBar()) return;

  if (ImGui::BeginMenu("File")) {
    if (ImGui::MenuItem("Quit")) {
      EditorSystem::Instance().Stop();
    }
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

}  // namespace editor
