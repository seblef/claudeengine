#pragma once

#include <string>

namespace editor {

class EditorScene;

// Generates a unique name for a new scene object.
//
// Scans existing scene objects for names matching "<base_name>_NNN" (where NNN
// is a run of digits), finds the highest used index, and returns
// "<base_name>_NNN+1" zero-padded to three digits.
// Returns "<base_name>_001" when no existing object matches the prefix.
std::string GenerateObjectName(const EditorScene& scene,
                               const std::string& base_name);

}  // namespace editor
