#pragma once

#include <string>
#include <string_view>

#include "editor/EditorCommand.h"

namespace game { class GameObject; }

namespace editor {

// Command that renames a GameObject, with full undo/redo support.
class RenameObjectCommand : public EditorCommand {
 public:
  RenameObjectCommand(game::GameObject* obj,
                      std::string old_name,
                      std::string new_name);

  void Execute() override;
  void Undo()    override;
  std::string_view GetDescription() const override;

 private:
  // cppcheck-suppress unusedStructMember
  game::GameObject* obj_;
  // cppcheck-suppress unusedStructMember
  std::string       old_name_;
  // cppcheck-suppress unusedStructMember
  std::string       new_name_;
};

}  // namespace editor
