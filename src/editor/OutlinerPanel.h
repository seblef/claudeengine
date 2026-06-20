#pragma once

#include <cstddef>
#include <string>

#include "game/GameObjectType.h"

namespace game {
class GameObject;
class GamePivot;
}  // namespace game

namespace editor {

class EditorCommandHistory;
class EditorScene;

// Outliner panel: tree view of the full scene hierarchy.
//
// Objects are displayed in DFS pre-order. Root-level objects (no parent)
// appear at the top level; GamePivot nodes are collapsible folders whose
// children are nested underneath.
//
// Features:
//   - Single-click: set selection; Ctrl+click: toggle selection membership.
//   - Bidirectional selection: viewport picks are reflected here each frame.
//   - Drag-and-drop reparenting (undoable via ReparentCommand).
//   - Right-click context menu: Rename, Add child pivot, Unparent,
//     Delete subtree (with confirmation), Group selection.
//   - Empty-area context menu: New pivot.
//   - Inline rename: double-click a node or choose Rename from the menu.
class OutlinerPanel {
 public:
  OutlinerPanel() = default;

  void SetCommandHistory(EditorCommandHistory* history);

  // Renders the outliner tree inside the current ImGui window.
  void Render(EditorScene& scene);

 private:
  // Renders one node and, for pivots with children, recurses.
  void RenderNode(game::GameObject* obj, EditorScene& scene);

  // Attaches drag-source and drop-target to the last rendered item.
  // Returns the dragged object when a drop is accepted; nullptr otherwise.
  static game::GameObject* HandleDragDrop(game::GameObject* obj);

  // Renders the per-node right-click context menu.
  void RenderNodeContextMenu(game::GameObject* obj, EditorScene& scene);

  // Renders an inline InputText rename field in place of the normal node label.
  void RenderRenameField(game::GameObject* obj);

  // Returns the FontAwesome icon string for the given object type.
  static const char* IconForType(game::GameObjectType type);

  // Generates a unique pivot name of the form "Pivot N".
  static std::string MakePivotName(const EditorScene& scene);

  game::GameObject*     renaming_obj_          = nullptr;
  // cppcheck-suppress unusedStructMember
  char                  rename_buf_[256]        = {};
  bool                  rename_focus_needed_    = false;

  // Object awaiting confirmation before deletion; non-null between the frame
  // the user clicks "Delete subtree" and the frame they confirm or cancel.
  // cppcheck-suppress unusedStructMember
  game::GameObject*     confirm_delete_         = nullptr;
  // Set to true for one frame to call ImGui::OpenPopup for the delete modal.
  // cppcheck-suppress unusedStructMember
  bool                  open_delete_modal_      = false;

  EditorCommandHistory* history_                = nullptr;
};

}  // namespace editor
