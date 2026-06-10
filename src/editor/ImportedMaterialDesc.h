#pragma once

#include <array>
#include <functional>
#include <string>

#include "renderer/TextureSlot.h"

namespace editor {

// Describes a material slot passed from the mesh import/edit workflow into
// MaterialEditorWindow::OpenWithDesc.
//
// texture_paths — resolved paths relative to data/ (e.g. "textures/body.png");
//                 empty when the texture could not be found under data/textures/.
// hint_paths    — original unresolved names from the source file for each slot;
//                 shown dimly in the editor when the resolved path is empty.
// on_saved      — fired after the material is written to disk; receives the
//                 saved material name (basename without extension).
struct ImportedMaterialDesc {
  // cppcheck-suppress unusedStructMember
  std::string slot_name;
  // cppcheck-suppress unusedStructMember
  std::array<std::string, renderer::kTextureSlotCount> texture_paths{};
  // cppcheck-suppress unusedStructMember
  std::array<std::string, renderer::kTextureSlotCount> hint_paths{};
  // cppcheck-suppress unusedStructMember
  std::function<void(const std::string&)> on_saved;
};

}  // namespace editor
