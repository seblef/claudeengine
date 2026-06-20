#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "game/GameObjectType.h"

namespace game {
class GameObject;
class GamePivot;
}  // namespace game

namespace editor {

class EditorCommandHistory;
class EditorScene;

// Left panel "Objects" tab: hierarchy view of scene objects.
//
// Root-level GamePivot nodes appear as collapsible tree folders with their
// children nested underneath. Non-pivot root objects are listed under their
// type category (Meshes, Lights, …). Objects parented to a pivot are shown
// only under that pivot's folder — not repeated in the type categories.
//
// A collapsed pivot acts as a group: clicking any child in the viewport
// selects the pivot. An expanded pivot allows individual child selection.
// Expand/collapse is toggled by clicking the arrow on the pivot's folder.
//
// Double-clicking a leaf starts an inline rename committed on Enter / focus loss.
class ObjectsPanel {
 public:
  ObjectsPanel() = default;

  void SetCommandHistory(EditorCommandHistory* history);

  // Renders the objects tree inside the current ImGui tab item.
  void Render(EditorScene& scene);

 private:
  // Renders a GamePivot as a collapsible folder with all its children inside.
  // Recursively handles nested pivot subtrees.
  void RenderPivotNode(game::GamePivot* pivot, EditorScene& scene);

  // Renders a collapsible type-category node containing all root-level
  // (un-parented) objects of the given type.
  static void RenderTypeGroup(const char* icon, const char* group_name,
                              game::GameObjectType type,
                              const std::vector<game::GameObject*>& objects,
                              EditorScene& scene,
                              game::GameObject*& renaming_obj,
                              char* rename_buf, std::size_t rename_buf_size,
                              bool& rename_focus_needed,
                              EditorCommandHistory* history);

  // Renders a single object leaf; handles selection, rename, double-click.
  void RenderObjectLeaf(const char* icon, game::GameObject* obj,
                        EditorScene& scene);

  // Returns the icon string for the given object type.
  static const char* IconForType(game::GameObjectType type);

  game::GameObject*     renaming_obj_        = nullptr;
  // cppcheck-suppress unusedStructMember
  char                  rename_buf_[256]      = {};
  bool                  rename_focus_needed_  = false;
  EditorCommandHistory* history_              = nullptr;
};

}  // namespace editor
