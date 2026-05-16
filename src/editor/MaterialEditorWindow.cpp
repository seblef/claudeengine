#include "editor/MaterialEditorWindow.h"

#include <imgui.h>

#include "game/GameMaterial.h"
#include "game/MeshTemplate.h"

namespace editor {

namespace {
constexpr int kPreviewWidth  = 256;
constexpr int kPreviewHeight = 256;
}  // namespace

MaterialEditorWindow::MaterialEditorWindow(abstract::VideoDevice* video)
    : preview_(video, kPreviewWidth, kPreviewHeight) {
  // Default preview geometry — the procedural cube created by EditorScene.
  game::MeshTemplate* cube = game::MeshTemplate::Get("__proc_editor_cube");
  if (cube) preview_.SetTemplate(cube);
}

MaterialEditorWindow::~MaterialEditorWindow() = default;

void MaterialEditorWindow::Open(game::GameMaterial* mat) {
  material_ = mat;
  open_     = (mat != nullptr);
}

void MaterialEditorWindow::Render() {
  if (!open_ || !material_) return;

  const std::string title = "Material: " + material_->GetId();
  if (!ImGui::Begin(title.c_str(), &open_)) {
    ImGui::End();
    return;
  }

  preview_.Render(static_cast<float>(ImGui::GetTime()));

  ImGui::End();
}

}  // namespace editor
