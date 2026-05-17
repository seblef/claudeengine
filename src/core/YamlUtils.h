#pragma once

#include <filesystem>

#include <yaml-cpp/yaml.h>

#include "core/Color.h"
#include "core/Mat4f.h"
#include "core/Vec3f.h"

namespace core {

// Loads and parses a YAML file. Throws std::runtime_error on failure.
YAML::Node LoadYamlFile(const std::filesystem::path& path);

// Parses a 3-element YAML sequence into Vec3f.
// Returns fallback when the node is absent, not a sequence, or has fewer than
// 3 elements.
Vec3f ParseVec3(const YAML::Node& node, const Vec3f& fallback = Vec3f{});

// Parses a YAML sequence into Color.
// Supports 1 element (broadcast to RGB, alpha=1), 3 elements (RGB, alpha=1),
// or 4 elements (RGBA). Returns fallback when the node is absent, not a
// sequence, or empty.
Color ParseColor(const YAML::Node& node, const Color& fallback = Color{});

// Parses a 16-element YAML float sequence into a row-major Mat4f.
// Returns identity and logs a warning when the node is absent or malformed.
Mat4f ParseMat4(const YAML::Node& node);

}  // namespace core
