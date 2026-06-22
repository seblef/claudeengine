#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "abstract/VideoDevice.h"
#include "editor/EditorCameraController.h"
#include "game/GameObjectVisitor.h"

namespace physics { class PhysicsBody; }

namespace game {
class FPSCameraController;
class GameMesh;
class GamePlayerStart;
class GameTerrain;
}  // namespace game

namespace editor {

class EditorScene;
class EditorToolbar;
class EditorViewport;

// Orchestrates the full Play/Stop lifecycle for Play-in-Editor mode.
//
// Enter() validates the scene (saved path, player start presence), auto-saves,
// installs an FPS camera at the GamePlayerStart world position, registers static
// physics bodies for all scene meshes, and hides editor panels/tools.
//
// Exit() tears down play-time physics bodies, reloads the scene from disk,
// restores the editor camera and panels.
//
// Tick(dt) must be called every frame while IsPlaying() to step physics and
// update the FPS camera. It must be called before EditorViewport::Render() so
// the camera position is current when the renderer draws the frame.
//
// Owned by EditorWindow as std::unique_ptr<PlayModeManager>.
class PlayModeManager {
 public:
  PlayModeManager(EditorScene* scene,
                  EditorToolbar* toolbar,
                  EditorViewport* viewport,
                  abstract::VideoDevice* video);

  // If still playing, calls Exit() to clean up physics bodies.
  ~PlayModeManager();

  // Registers a callback invoked by Exit() to reload the scene.
  //
  // The callback receives the map file path and the saved camera state. The
  // caller is responsible for:
  //   1. Destroying the current scene FIRST (so GameTerrain::OnRemovedFromScene
  //      shuts down FoliageRenderer before the new scene is loaded).
  //   2. Loading the new scene via MapSerializer::Load().
  //   3. Replacing the scene pointer and re-wiring subsystems.
  //
  // Splitting the "reset old scene" and "set new scene" steps inside the caller
  // avoids a FoliageRenderer double-instantiation that would occur if the new
  // EditorScene were constructed while the old one (and its FoliageRenderer)
  // was still alive.
  void SetOnExit(
      std::function<void(std::filesystem::path,
                         EditorCameraController::CameraState)> cb);

  // Registers a callback invoked when a transient status-bar message should be
  // shown (e.g. validation failures, save errors).
  void SetOnStatusMessage(std::function<void(std::string)> cb);

  // Attempts to enter play mode. Validates the scene (file path, player start),
  // auto-saves, hides panels, installs the FPS camera, and registers physics.
  // No-op and shows a status-bar warning if preconditions are not met.
  // No-op if already playing.
  void Enter();

  // Exits play mode: tears down play-time physics bodies, restores viewport and
  // toolbar state, and fires the on_exit_ callback so the caller can reset the
  // old scene and load a fresh one. Panels and tools are restored even if the
  // reload in the callback fails. No-op if not playing.
  void Exit();

  // Steps physics and updates the FPS camera. Must be called every frame while
  // IsPlaying() returns true, and before EditorViewport::Render().
  void Tick(float dt);

  // Forwards a platform event to the FPS camera controller while playing.
  // Call from EditorWindow::OnEvent() instead of EditorViewport::OnEvent() while
  // IsPlaying() returns true.
  void OnEvent(const core::Event& event);

  [[nodiscard]] bool IsPlaying()       const { return playing_; }
  [[nodiscard]] bool AreToolsFrozen()  const { return tools_frozen_; }
  [[nodiscard]] bool ArePanelsHidden() const { return panels_hidden_; }

 private:
  // Visitor used in Enter() to locate the GamePlayerStart, GameTerrain, and all
  // GameMesh instances that need static play-time physics bodies.
  class EnterVisitor : public game::GameObjectVisitor {
   public:
    game::GamePlayerStart* player_start = nullptr;
    game::GameTerrain*     terrain      = nullptr;

    void Visit(game::GameCamera&)            override {}
    void Visit(game::GameLight&)             override {}
    void Visit(game::GameMesh& mesh)         override;
    void Visit(game::GameParticleSystem&)    override {}
    void Visit(game::GamePivot&)             override {}
    void Visit(game::GamePlayerStart& ps)    override;
    void Visit(game::GameSoundEmitter&)      override {}
    void Visit(game::GameTerrain& t)         override;

    // Physics bodies created for meshes without an existing physics descriptor.
    // Filled during the visit and transferred to play_bodies_ after traversal.
    // cppcheck-suppress unusedStructMember
    std::vector<physics::PhysicsBody*> created_bodies;
  };

  void SetStatusMessage(const std::string& msg);

  // cppcheck-suppress unusedStructMember
  static constexpr double kProfilerInterval = 2.0;

  // cppcheck-suppress unusedStructMember
  EditorScene*           scene_;
  // cppcheck-suppress unusedStructMember
  EditorToolbar*         toolbar_;
  // cppcheck-suppress unusedStructMember
  EditorViewport*        viewport_;
  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice* video_;

  std::unique_ptr<game::FPSCameraController> fps_controller_;

  // Editor camera state saved on Enter() and restored on Exit().
  // cppcheck-suppress unusedStructMember
  EditorCameraController::CameraState saved_camera_state_;

  // Static physics bodies created in Enter() for meshes without a physics
  // descriptor. Destroyed in Exit() before the scene is reloaded.
  // cppcheck-suppress unusedStructMember
  std::vector<physics::PhysicsBody*> play_bodies_;

  // cppcheck-suppress unusedStructMember
  bool playing_       = false;
  // cppcheck-suppress unusedStructMember
  bool tools_frozen_  = false;
  // cppcheck-suppress unusedStructMember
  bool panels_hidden_ = false;

  // File path saved during Enter() so Exit() can pass it to on_exit_ after the
  // old scene pointer has been invalidated by the callback.
  // cppcheck-suppress unusedStructMember
  std::filesystem::path saved_file_path_;

  // cppcheck-suppress unusedStructMember
  std::function<void(std::filesystem::path,
                     EditorCameraController::CameraState)> on_exit_;
  // cppcheck-suppress unusedStructMember
  std::function<void(std::string)> on_status_message_;
};

}  // namespace editor
