#include "editor/EditorSystem.h"

#include "core/Color.h"
#include "core/Config.h"
#include "core/EventManager.h"
#include "core/EventType.h"
#include "editor/EditorTheme.h"
#include "editor/EditorWindow.h"

#include <string>

#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <ImGuizmo.h>
#include <nfd.h>

namespace editor {

EditorSystem::EditorSystem(abstract::Devices* devices)
    : devices_(devices),
      video_(devices->GetVideoDevice()),
      editor_window_(std::make_unique<EditorWindow>(video_)) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  auto* glfw_window = static_cast<GLFWwindow*>(devices_->GetWindow());
  ImGui_ImplGlfw_InitForOpenGL(glfw_window, true);
  ImGui_ImplOpenGL3_Init("#version 460");
  ApplySlateAndTealTheme();

  // Pass explicit SizePixels so AddFontDefault does not set ImFontFlags_ImplicitRefSize,
  // which would make MergeMode incompatible with the icon font added below.
  ImFontConfig base_cfg;
  base_cfg.SizePixels = 13.0f;
  base_cfg.PixelSnapH = true;
  io.Fonts->AddFontDefault(&base_cfg);
  ImFontConfig icons_cfg;
  icons_cfg.MergeMode        = true;
  icons_cfg.PixelSnapH       = true;
  icons_cfg.GlyphMinAdvanceX = 13.0f;
  static const ImWchar kIconRanges[] = {ICON_MIN_FA, ICON_MAX_16_FA, 0};
  const std::string font_path =
      (core::Config::GetDataFolder() / "fonts/fa-solid-900.ttf").string();
  io.Fonts->AddFontFromFileTTF(font_path.c_str(), 16.0f, &icons_cfg, kIconRanges);
  io.Fonts->Build();

  NFD_Init();
}

EditorSystem::~EditorSystem() {
  NFD_Quit();
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}

void EditorSystem::Run() {
  while (running_) {
    devices_->Update();

    while (core::EventManager::Instance().HasEvents()) {
      core::Event e = core::EventManager::Instance().Consume();
      if (e.type == core::EventType::kWindowClose) running_ = false;
      editor_window_->OnEvent(e);
    }

    if (!running_) break;

    video_->BeginFrame();
    video_->ClearRenderTargets(core::Color::kBlack);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::BeginFrame();

    editor_window_->Render();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  }
}

void EditorSystem::Stop() {
  running_ = false;
}

}  // namespace editor
