#include "editor/EditorSystem.h"

#include "core/Color.h"
#include "core/EventManager.h"
#include "core/EventType.h"
#include "editor/EditorWindow.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

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
  ImGui::StyleColorsDark();
}

EditorSystem::~EditorSystem() {
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

    editor_window_->Render();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  }
}

void EditorSystem::Stop() {
  running_ = false;
}

}  // namespace editor
