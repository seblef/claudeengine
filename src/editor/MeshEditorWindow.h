#pragma once

#include <array>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "abstract/VideoDevice.h"
#include "core/Mat3f.h"
#include "editor/MaterialEditorWindow.h"
#include "editor/MeshPreview.h"
#include "mesh/MeshData.h"
#include "renderer/TextureSlot.h"

namespace game {
class GameMaterial;
class MeshTemplate;
}

namespace editor {

class EditorScene;

// Interactive window for importing mesh files and editing existing emesh files.
//
// Two modes:
//   Import — opened by EditorWindow::ImportMesh() for .fbx or .obj sources.
//            Scale is pre-filled from FBX unit metadata; rotation is accumulated
//            via six 90° buttons.  [Import] bakes scale × rotation into vertex
//            positions/normals/tangents/binormals, writes data/meshes/{name}.emesh
//            (v3 format), and adds the resulting MeshTemplate to the scene.
//
//   Edit   — opened via right-click → "Edit…" on a mesh in ResourcesPanel.
//            Reads vertex data directly from the emesh file.  [Save] overwrites
//            the existing file and reloads the live MeshTemplate in the scene.
//
// Material slots (one per SubMeshRange) each have an [Edit] button that opens
// MaterialEditorWindow pre-filled with the slot name and any textures resolved
// from data/textures/ by filename stem.  When the user saves in the material
// editor the slot row updates to show the saved material name.
//
// The MeshPreview on the left shows the full mesh with all submesh materials
// (axes gizmo) and reflects scale / rotation live.
class MeshEditorWindow {
 public:
  MeshEditorWindow(abstract::VideoDevice* video,
                   MaterialEditorWindow* material_editor);
  ~MeshEditorWindow();

  MeshEditorWindow(const MeshEditorWindow&)            = delete;
  MeshEditorWindow& operator=(const MeshEditorWindow&) = delete;

  // Opens in import mode for a .fbx or .obj source file.
  void OpenImport(const std::filesystem::path& source_path);

  // Opens in edit mode for an existing .emesh file (path is the full filesystem path).
  void OpenEdit(const std::filesystem::path& emesh_path);

  // Renders the window. scene is used to register the new template on import.
  void Render(EditorScene* scene);

  [[nodiscard]] bool IsOpen() const { return open_; }

 private:
  enum class Mode { kImport, kEdit };

  // Per-material-slot state.
  struct MatSlot {
    // cppcheck-suppress unusedStructMember
    std::string original_name;
    // Filesystem path after the user saves in MaterialEditorWindow
    // (relative to data/, e.g. "materials/body.yaml").
    // cppcheck-suppress unusedStructMember
    std::string saved_material_path;
    // cppcheck-suppress unusedStructMember
    std::array<std::string, renderer::kTextureSlotCount> hint_textures{};
    // User-editable material name; initialised from original_name (or "slot_N").
    // cppcheck-suppress unusedStructMember
    char name_buf[256] = {};
  };

  // Loads source mesh data (import mode).
  bool LoadSourceFile();

  // Loads existing emesh data (edit mode).
  bool LoadEmeshFile();

  // Searches data/textures/ recursively for a file whose stem matches name.
  // Returns the path relative to data/ (e.g. "textures/vehicles/body.png")
  // or an empty string if not found.
  // cppcheck-suppress functionStatic
  std::string ResolveTexture(const std::string& name) const;

  // Resolves texture hints for all slots.
  void ResolveAllTextures();

  // Applies accumulated scale * rotation to a copy of original_mesh_.
  mesh::MeshData BuildTransformedMesh() const;

  // Rebuilds preview_tmpl_ from the current transform state.
  void RebuildPreview();

  // Releases preview resources safely.
  void ClearPreview();

  // Writes the emesh and (import mode) adds the template to scene.
  void Commit(EditorScene* scene);

  void RenderMaterialSlots();
  void RenderTransformControls();
  void RenderBottomBar(EditorScene* scene);
  // Renders the "Pick material" popup anchored near the triggering slot button.
  void RenderPickMaterialPopup();

  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice*   video_;
  // cppcheck-suppress unusedStructMember
  MaterialEditorWindow*    material_editor_;

  // cppcheck-suppress unusedStructMember
  bool                     open_           = false;
  // cppcheck-suppress unusedStructMember
  Mode                     mode_           = Mode::kImport;
  // cppcheck-suppress unusedStructMember
  std::filesystem::path    source_path_;
  // cppcheck-suppress unusedStructMember
  char                     mesh_name_buf_[256] = {};

  // Original, untransformed mesh data as loaded from disk.
  // cppcheck-suppress unusedStructMember
  mesh::MeshData           original_mesh_;

  // Accumulated transform controls.
  float                    scale_          = 1.0f;
  // cppcheck-suppress unusedStructMember
  core::Mat3f              rotation_       = core::Mat3f::kIdentity;

  // cppcheck-suppress unusedStructMember
  std::vector<MatSlot>     mat_slots_;

  // Preview resources — rebuilt whenever the transform changes.
  // cppcheck-suppress unusedStructMember
  bool                     preview_dirty_  = false;
  // cppcheck-suppress unusedStructMember
  MeshPreview              preview_;
  // cppcheck-suppress unusedStructMember
  game::MeshTemplate*      preview_tmpl_   = nullptr;
  // cppcheck-suppress unusedStructMember
  std::vector<game::GameMaterial*> preview_mats_;

  // Per-session material cache: maps material name → GameMaterial* (extra ref).
  // Keeps materials alive in the global registry across preview rebuilds so that
  // unchanged slots are never reloaded from disk. Cleared when a new import/edit
  // session starts or the window is destroyed.
  // cppcheck-suppress unusedStructMember
  std::map<std::string, game::GameMaterial*> session_mat_cache_;

  // Releases all entries in session_mat_cache_ and clears the map.
  void ClearSessionMaterialCache();

  // Stems of all .yaml files in data/materials/ — populated once when the
  // Pick popup opens.
  // cppcheck-suppress unusedStructMember
  std::vector<std::string> material_stems_;

  // Index of the slot that triggered the Pick popup (-1 = popup closed).
  // cppcheck-suppress unusedStructMember
  int pick_popup_slot_        = -1;

  // True for exactly one frame: triggers ImGui::OpenPopup at window level so
  // the popup ID is not scoped under PushID(i).
  // cppcheck-suppress unusedStructMember
  bool pick_popup_requested_  = false;
};

}  // namespace editor
