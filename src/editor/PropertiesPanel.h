#pragma once

#include <functional>
#include <optional>
#include <string>

#include "core/Mat4f.h"
#include "editor/SoundEmitterSelectionModal.h"
#include "editor/commands/LightPropertyCommand.h"
#include "physics/PhysicsBodyDesc.h"

namespace game {
class GameObject;
class GameLight;
class GameMesh;
class GameParticleSystem;
class GameSoundEmitter;
}  // namespace game

namespace editor {

class EditorCommandHistory;

// Right panel "Properties": editable fields for the currently selected object.
//
// Render() must be called inside an open ImGui::Begin("Properties") window.
// Changes are applied immediately to the underlying renderer objects and pushed
// as undoable commands when a history is provided.
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

  // Sets the callback invoked whenever the selected object's transform is
  // changed from the properties panel (so the caller can update spatial
  // acceleration structures such as the picking grid).
  void SetOnTransformChanged(std::function<void(game::GameObject*)> cb) {
    on_transform_changed_ = std::move(cb);
  }

  // Sets the callback invoked when the "Play Once" button is pressed on a
  // selected SoundEmitter. Receives the sound name and volume scale so the
  // caller can forward them to a SoundPreviewPlayer.
  void SetOnPlaySoundOnce(
      std::function<void(const std::string&, float)> cb) {
    on_play_sound_once_ = std::move(cb);
  }

  // Renders the properties UI inside the current ImGui window.
  // obj may be nullptr (no selection).
  void Render(game::GameObject* obj);

 private:
  void RenderTransformSection(game::GameObject* obj);
  void RenderLightProperties(game::GameLight* light);
  void RenderMeshProperties(game::GameMesh* mesh);
  void RenderParticleSystemProperties(const game::GameParticleSystem* ps);
  void RenderSoundEmitterProperties(game::GameSoundEmitter* emitter);

  // cppcheck-suppress unusedStructMember
  EditorCommandHistory* history_              = nullptr;
  // cppcheck-suppress unusedStructMember
  LightSnapshot         before_snapshot_      = {};
  // cppcheck-suppress unusedStructMember
  core::Mat4f           before_transform_;
  // cppcheck-suppress unusedStructMember
  std::string           before_name_;
  // cppcheck-suppress unusedStructMember
  std::optional<physics::PhysicsBodyDesc> before_physics_desc_;
  // cppcheck-suppress unusedStructMember
  std::function<void(const std::string&)>  on_open_particle_editor_;
  // cppcheck-suppress unusedStructMember
  std::function<void(game::GameObject*)>   on_transform_changed_;
  // cppcheck-suppress unusedStructMember
  std::function<void(const std::string&, float)> on_play_sound_once_;
  // cppcheck-suppress unusedStructMember
  SoundEmitterSelectionModal               sound_picker_modal_{"Change Sound"};
};

}  // namespace editor
