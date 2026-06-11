#pragma once

#include <functional>
#include <string>

#include "editor/commands/LightPropertyCommand.h"

namespace game {
class GameObject;
class GameLight;
class GameMesh;
class GameParticleSystem;
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

  // Sets the callback invoked when "Open in Particle Editor" is clicked.
  // Receives the template name (basename without extension).
  void SetOnOpenParticleEditor(std::function<void(const std::string&)> cb) {
    on_open_particle_editor_ = std::move(cb);
  }

  // Renders the properties UI inside the current ImGui window.
  // obj may be nullptr (no selection).
  void Render(game::GameObject* obj);

 private:
  void RenderLightProperties(game::GameLight* light);
  static void RenderMeshProperties(const game::GameMesh* mesh);
  void RenderParticleSystemProperties(const game::GameParticleSystem* ps);

  // cppcheck-suppress unusedStructMember
  EditorCommandHistory* history_         = nullptr;
  // cppcheck-suppress unusedStructMember
  LightSnapshot         before_snapshot_ = {};
  // cppcheck-suppress unusedStructMember
  std::string           before_name_;
  // cppcheck-suppress unusedStructMember
  std::function<void(const std::string&)> on_open_particle_editor_;
};

}  // namespace editor
