#pragma once

#include <filesystem>
#include <stdexcept>
#include <string_view>

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

namespace core::yaml {

// All Write* helpers emit a `key: value` pair and must be called while the
// YAML::Emitter is inside an active map (between BeginMap / EndMap).

// Writes a Vec3f as a [x, y, z] flow sequence under key.
void WriteVec3f(YAML::Emitter& out, std::string_view key, const Vec3f& v);

// Writes a Color as an [r, g, b, a] flow sequence under key.
void WriteColor(YAML::Emitter& out, std::string_view key, const Color& c);

// Writes a Mat4f as a 16-element flat float flow sequence (row-major) under
// key. Round-trips with core::ParseMat4().
void WriteMat4(YAML::Emitter& out, std::string_view key, const Mat4f& m);

// Reads node[key] as a relative path string and resolves it against base_dir.
// Returns an empty path and logs a warning when the key is absent or not a
// scalar string.
std::filesystem::path ReadPath(const YAML::Node& node,
                               std::string_view key,
                               const std::filesystem::path& base_dir);

// Writes path as a string relative to base_dir under key.
// Falls back to the absolute path string when base_dir and path share no
// common root.
void WritePath(YAML::Emitter& out,
               std::string_view key,
               const std::filesystem::path& path,
               const std::filesystem::path& base_dir);

}  // namespace core::yaml
