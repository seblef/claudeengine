#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "game/GameObjectType.h"

namespace game { class GameObject; }

namespace editor {

class EditorCommandHistory;
class EditorScene;

// Left panel "Objects" tab: tree view of scene objects grouped by type.
//
// Meshes, lights, and cameras are shown in separate collapsible groups with
// coloured inline icons. Clicking a leaf item selects the object via
// EditorScene::SetSelectedObject(). Double-clicking a leaf starts an inline
// rename; the rename is committed (and pushed to history) on Enter or focus
// loss, and cancelled on Escape.
class ObjectsPanel {
 public:
  ObjectsPanel() = default;

  void SetCommandHistory(EditorCommandHistory* history);

  // Renders the objects tree inside the current ImGui tab item.
  void Render(EditorScene& scene);

 private:
  static void RenderGroup(const char* icon, const char* group_name,
                          game::GameObjectType type,
                          const std::vector<game::GameObject*>& objects,
                          EditorScene& scene,
                          game::GameObject*& renaming_obj,
                          char* rename_buf, std::size_t rename_buf_size,
                          bool& rename_focus_needed,
                          EditorCommandHistory* history);

  void CommitRename(game::GameObject* obj, const std::string& old_name);

  game::GameObject*     renaming_obj_        = nullptr;
  // cppcheck-suppress unusedStructMember
  char                  rename_buf_[256]      = {};
  bool                  rename_focus_needed_  = false;
  EditorCommandHistory* history_              = nullptr;
};

}  // namespace editor
