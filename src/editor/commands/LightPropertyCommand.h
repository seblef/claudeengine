#pragma once

#include <string_view>

#include "core/Color.h"
#include "core/Vec3f.h"
#include "editor/EditorCommand.h"

namespace game {
class GameLight;
}  // namespace game

namespace editor {

// Snapshot of all editable light properties.
//
// Fields that don't apply to a given light type are left zero-initialised;
// ApplySnapshot() only writes the fields relevant to the concrete subtype.
struct LightSnapshot {
  // Common fields (all light types).
  // cppcheck-suppress unusedStructMember
  core::Color color;
  // cppcheck-suppress unusedStructMember
  float       intensity         = 0.f;
  // cppcheck-suppress unusedStructMember
  bool        cast_shadow       = true;
  // cppcheck-suppress unusedStructMember
  int         shadow_resolution = 1024;
  // cppcheck-suppress unusedStructMember
  float       shadow_bias       = 0.005f;

  // OmniLight.
  // cppcheck-suppress unusedStructMember
  float radius = 0.f;

  // CircleSpotLight.
  // cppcheck-suppress unusedStructMember
  float inner_angle = 0.f;
  // cppcheck-suppress unusedStructMember
  float outer_angle = 0.f;

  // CircleSpotLight / RectangleSpotLight.
  // cppcheck-suppress unusedStructMember
  float       range     = 0.f;
  // cppcheck-suppress unusedStructMember
  core::Vec3f direction = {};

  // RectangleSpotLight.
  // cppcheck-suppress unusedStructMember
  float h_angle = 0.f;
  // cppcheck-suppress unusedStructMember
  float v_angle = 0.f;

  // GlobalLight.
  // cppcheck-suppress unusedStructMember
  core::Vec3f ambient_color = {};

  [[nodiscard]] bool operator==(const LightSnapshot& o) const;
  [[nodiscard]] bool operator!=(const LightSnapshot& o) const {
    return !(*this == o);
  }
};

// Reads the current state of game_light into a LightSnapshot.
[[nodiscard]] LightSnapshot CaptureSnapshot(const game::GameLight* game_light);

// Writes a previously captured snapshot back to game_light.
void ApplySnapshot(game::GameLight* game_light, const LightSnapshot& s);

// Undoable command for any light property change made in PropertiesPanel.
//
// Captures before/after snapshots at drag-start and drag-end; Execute()/Redo()
// apply after_, Undo() applies before_.
class LightPropertyCommand : public EditorCommand {
 public:
  LightPropertyCommand(game::GameLight* light,
                       LightSnapshot    before,
                       LightSnapshot    after);

  void Execute() override;
  void Undo()    override;
  void Redo()    override;
  std::string_view GetDescription() const override;

 private:
  // cppcheck-suppress unusedStructMember
  game::GameLight* light_;
  // cppcheck-suppress unusedStructMember
  LightSnapshot    before_;
  // cppcheck-suppress unusedStructMember
  LightSnapshot    after_;
};

}  // namespace editor
