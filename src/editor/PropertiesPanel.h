#pragma once

#include "editor/commands/LightPropertyCommand.h"

namespace game {
class GameObject;
class GameLight;
class GameMesh;
}  // namespace game

namespace editor {

class EditorCommandHistory;

// Right panel "Properties": editable fields for the currently selected object.
//
// Render() must be called inside an open ImGui::Begin("Properties") window.
// Changes are applied immediately to the underlying renderer objects and pushed
// as undoable LightPropertyCommands when a history is provided.
class PropertiesPanel {
 public:
  PropertiesPanel() = default;

  // Registers the undo/redo history. Must be called before the first Render().
  void SetCommandHistory(EditorCommandHistory* history) { history_ = history; }

  // Renders the properties UI inside the current ImGui window.
  // obj may be nullptr (no selection).
  void Render(game::GameObject* obj);

 private:
  void RenderLightProperties(game::GameLight* light);
  void RenderMeshProperties(const game::GameMesh* mesh);

  // cppcheck-suppress unusedStructMember
  EditorCommandHistory* history_         = nullptr;
  // cppcheck-suppress unusedStructMember
  LightSnapshot         before_snapshot_ = {};
};

}  // namespace editor
