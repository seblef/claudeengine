#pragma once

#include "abstract/VideoDevice.h"
#include "editor/MeshPreview.h"
#include "editor/commands/MaterialPropertyCommand.h"
#include "renderer/TextureSlot.h"

namespace game {
class GameMaterial;
class MeshTemplate;
}

namespace editor {

class EditorCommandHistory;
class EditorScene;

// Floating window for inspecting and editing a game::GameMaterial.
//
// Layout:
//   Left column  — MeshPreview (kPreviewSize×kPreviewSize) with Cube/Sphere
//                  geometry toggle buttons below.
//   Right column — Collapsible sections: Rendering (textures, colors,
//                  shininess), Sound (placeholder), Physics (placeholder).
//   Bottom bar   — Save to data/materials/{name}.yaml, Apply to the selected
//                  GameMesh in the scene.
//
// Usage:
//   Call Open(mat) when the user double-clicks a material in the resource
//   panel. Call Render(scene) every ImGui frame.
class MaterialEditorWindow {
 public:
  explicit MaterialEditorWindow(abstract::VideoDevice* video);
  ~MaterialEditorWindow();

  MaterialEditorWindow(const MaterialEditorWindow&)            = delete;
  MaterialEditorWindow& operator=(const MaterialEditorWindow&) = delete;

  // Opens (or re-focuses) the window for mat. Pass nullptr to hide.
  void Open(game::GameMaterial* mat);

  // Renders the window if a material is currently open.
  void Render(const EditorScene& scene);

  // Registers the undo/redo history. Must be called before the first Render().
  void SetCommandHistory(EditorCommandHistory* history) { history_ = history; }

 private:
  enum class PreviewGeometry { kCube, kSphere };

  void RenderPreviewColumn();
  void RenderPropertiesColumn();
  void RenderRenderingSection();
  void RenderTextureSlot(renderer::TextureSlot slot, const char* label);
  void RenderBottomBar(const EditorScene& scene);
  void Save();
  void ApplyToSelection(const EditorScene& scene);

  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice*  video_;
  // cppcheck-suppress unusedStructMember
  EditorCommandHistory*   history_         = nullptr;
  // cppcheck-suppress unusedStructMember
  game::GameMaterial*     material_        = nullptr;
  // cppcheck-suppress unusedStructMember
  bool                    open_            = false;
  PreviewGeometry         preview_geo_     = PreviewGeometry::kCube;
  // cppcheck-suppress unusedStructMember
  MaterialSnapshot        before_snapshot_ = {};
  // True while any color or shininess widget is actively being edited.
  // Used to detect when editing stops so the command can be pushed.
  // cppcheck-suppress unusedStructMember
  bool                    editing_         = false;

  // Procedural preview geometry templates owned by this window.
  // Release()'d in the destructor after clearing the preview instance.
  // cppcheck-suppress unusedStructMember
  game::MeshTemplate*    cube_tmpl_   = nullptr;
  // cppcheck-suppress unusedStructMember
  game::MeshTemplate*    sphere_tmpl_ = nullptr;

  MeshPreview            preview_;
};

}  // namespace editor
