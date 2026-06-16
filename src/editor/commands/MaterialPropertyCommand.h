#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "core/Color.h"
#include "editor/EditorCommand.h"

namespace abstract {
class VideoDevice;
}  // namespace abstract

namespace game {
class GameMaterial;
}  // namespace game

namespace editor {

// Snapshot of all editable material properties.
//
// Texture slot entries are texture asset IDs (empty == slot was null).
// Color properties use four-channel Color to match renderer::Material.
struct MaterialSnapshot {
  // cppcheck-suppress unusedStructMember
  core::Color diffuse_color;
  // cppcheck-suppress unusedStructMember
  core::Color emissive_color;
  // cppcheck-suppress unusedStructMember
  core::Color ambient_color;
  // cppcheck-suppress unusedStructMember
  float       shininess   = 32.f;
  // cppcheck-suppress unusedStructMember
  float       specular    = 1.f;
  // cppcheck-suppress unusedStructMember
  bool        alpha_mask  = false;

  // Texture slot asset IDs, indexed by TextureSlot integer value.
  // cppcheck-suppress unusedStructMember
  std::vector<std::string> texture_slots;

  [[nodiscard]] bool operator==(const MaterialSnapshot& o) const;
  [[nodiscard]] bool operator!=(const MaterialSnapshot& o) const {
    return !(*this == o);
  }
};

// Reads the current material state into a MaterialSnapshot.
[[nodiscard]] MaterialSnapshot CaptureSnapshot(const game::GameMaterial* mat);

// Writes a previously captured snapshot back to mat.
// video is required to recreate texture objects from their stored IDs.
void ApplySnapshot(game::GameMaterial* mat, const MaterialSnapshot& s,
                   abstract::VideoDevice* video);

// Undoable command for any material property change in MaterialEditorWindow.
//
// Captures before/after snapshots at widget activation and deactivation;
// Execute()/Redo() apply after_, Undo() applies before_ and emits an info
// log reminding the user that the saved YAML file is unaffected.
class MaterialPropertyCommand : public EditorCommand {
 public:
  MaterialPropertyCommand(game::GameMaterial*    mat,
                          abstract::VideoDevice* video,
                          MaterialSnapshot       before,
                          MaterialSnapshot       after);

  void Execute() override;
  void Undo()    override;
  void Redo()    override;
  std::string_view GetDescription() const override;

 private:
  // cppcheck-suppress unusedStructMember
  game::GameMaterial*    mat_;
  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice* video_;
  // cppcheck-suppress unusedStructMember
  MaterialSnapshot       before_;
  // cppcheck-suppress unusedStructMember
  MaterialSnapshot       after_;
};

}  // namespace editor
