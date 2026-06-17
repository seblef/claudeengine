#pragma once

#include <optional>
#include <string_view>

#include "editor/EditorCommand.h"
#include "physics/PhysicsBodyDesc.h"

namespace game {
class GameMesh;
}  // namespace game

namespace editor {

// Undoable command for physics body property changes made in PropertiesPanel.
//
// Stores before/after optional<PhysicsBodyDesc>: nullopt represents "no physics
// body" so enabling and disabling physics are both reversible.
class PhysicsPropertyCommand : public EditorCommand {
 public:
  PhysicsPropertyCommand(game::GameMesh*                          mesh,
                         std::optional<physics::PhysicsBodyDesc>  before,
                         std::optional<physics::PhysicsBodyDesc>  after);

  void Execute() override;
  void Undo()    override;
  void Redo()    override;
  std::string_view GetDescription() const override;

 private:
  void Apply(const std::optional<physics::PhysicsBodyDesc>& desc);

  // cppcheck-suppress unusedStructMember
  game::GameMesh*                         mesh_;
  // cppcheck-suppress unusedStructMember
  std::optional<physics::PhysicsBodyDesc> before_;
  // cppcheck-suppress unusedStructMember
  std::optional<physics::PhysicsBodyDesc> after_;
};

}  // namespace editor
