#pragma once

#include <string_view>
#include <vector>

#include "core/Mat4f.h"
#include "editor/EditorCommand.h"

namespace game {
class GameObject;
}  // namespace game

namespace editor {

// Command that records an ImGuizmo-driven transform applied simultaneously to
// several GameObjects (multi-selection). Captures before/after world transforms
// for each object so the whole group can be undone or redone as a single step.
class MultiTransformCommand : public EditorCommand {
 public:
  struct Entry {
    // cppcheck-suppress unusedStructMember
    game::GameObject* obj;
    // cppcheck-suppress unusedStructMember
    core::Mat4f       before;
    // cppcheck-suppress unusedStructMember
    core::Mat4f       after;
  };

  explicit MultiTransformCommand(std::vector<Entry> entries);

  void Execute() override;
  void Undo()    override;
  void Redo()    override;
  std::string_view GetDescription() const override;

 private:
  // cppcheck-suppress unusedStructMember
  std::vector<Entry> entries_;
};

}  // namespace editor
