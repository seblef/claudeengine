#pragma once

#include <filesystem>
#include <memory>
#include <optional>

#include <yaml-cpp/yaml.h>

#include "editor/EditorCameraController.h"
#include "editor/EditorScene.h"
#include "game/GameCamera.h"
#include "game/GameLight.h"
#include "game/GameMesh.h"
#include "game/GameObjectVisitor.h"
#include "game/GameParticleSystem.h"
#include "game/GamePlayerStart.h"
#include "game/GameSoundEmitter.h"
#include "game/GameTerrain.h"

namespace abstract { class VideoDevice; }

namespace editor {

// Aggregates the result of a successful map load.
struct MapLoadResult {
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<EditorScene>            scene;
  // cppcheck-suppress unusedStructMember
  EditorCameraController::CameraState     camera_state;
};

// Saves and loads an EditorScene to/from a .map.yaml file.
//
// Save() serialises the scene's objects and editor camera state into a
// human-readable YAML document. Load() delegates parsing to MapLoader and then
// reconstructs an EditorScene with ownership of all loaded assets.
class MapSerializer {
 public:
  // Serialises scene + camera state to path. Returns false on I/O failure.
  static bool Save(const EditorScene& scene,
                   const EditorCameraController::CameraState& cam,
                   const std::filesystem::path& path);

  // Parses a .map.yaml file into a new EditorScene + editor camera state.
  // Returns std::nullopt on parse or I/O failure (logs error).
  static std::optional<MapLoadResult> Load(const std::filesystem::path& path,
                                           abstract::VideoDevice* video);

 private:
  // Visitor used by Save(): emits one YAML mapping per object type.
  // GameTerrain is serialised as a root-level "terrain:" block; the visitor
  // skips it from the objects sequence and stores it for post-loop emission.
  class SerializeVisitor : public game::GameObjectVisitor {
   public:
    SerializeVisitor(YAML::Emitter& out,
                     const std::filesystem::path& map_path,
                     const std::filesystem::path& data_dir);

    void Visit(game::GameCamera& camera)                override;
    void Visit(game::GameLight& light)                  override;
    void Visit(game::GameMesh& mesh)                    override;
    void Visit(game::GameParticleSystem& particle_system) override;
    void Visit(game::GamePlayerStart& player_start)     override;
    void Visit(game::GameSoundEmitter& sound_emitter)   override;
    // Skipped from the objects sequence — terrain is at root level.
    void Visit(game::GameTerrain& terrain)              override {}

    // Emits the "terrain:" root-level block and writes binary side-car files.
    // Must be called after the YAML objects sequence is closed.
    void EmitTerrain(const game::GameTerrain* terrain);

   private:
    YAML::Emitter&        out_;
    std::filesystem::path map_path_;
    std::filesystem::path data_dir_;
  };
};

}  // namespace editor
