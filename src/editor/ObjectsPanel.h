#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "game/GameObjectType.h"

namespace game { class GameObject; }

namespace editor {

class EditorCommandHistory;
class EditorScene;
struct ObjectGroup;

// Left panel "Objects" tab: tree view of scene objects.
//
// Objects that belong to a group are shown inside a collapsible folder for that
// group. Ungrouped objects appear in their type categories (Meshes, Lights, …).
// Clicking a leaf selects the object (or the whole group for closed groups).
// Double-clicking a leaf starts an inline rename committed on Enter / focus loss.
class ObjectsPanel {
 public:
  ObjectsPanel() = default;

  void SetCommandHistory(EditorCommandHistory* history);

  // Renders the objects tree inside the current ImGui tab item.
  void Render(EditorScene& scene);

 private:
  // Renders a collapsible type-category node (Meshes, Lights, …) containing
  // all ungrouped objects of the given type.
  static void RenderTypeGroup(const char* icon, const char* group_name,
                              game::GameObjectType type,
                              const std::vector<game::GameObject*>& objects,
                              EditorScene& scene,
                              game::GameObject*& renaming_obj,
                              char* rename_buf, std::size_t rename_buf_size,
                              bool& rename_focus_needed,
                              EditorCommandHistory* history);

  // Renders one ObjectGroup as a folder node with its members as children.
  void RenderEditorGroup(ObjectGroup& grp, EditorScene& scene);

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
