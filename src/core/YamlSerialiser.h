#pragma once

#include <filesystem>
#include <string_view>

#include <yaml-cpp/yaml.h>

#include "core/Color.h"
#include "core/Mat4f.h"
#include "core/Vec3f.h"

namespace core::yaml {

// All Write* helpers emit a `key: value` pair and must be called while the
// YAML::Emitter is inside an active map (between BeginMap / EndMap).

// Writes a Vec3f as a [x, y, z] flow sequence under key.
void WriteVec3f(YAML::Emitter& out, std::string_view key, const Vec3f& v);

// Writes a Color as an [r, g, b, a] flow sequence under key.
void WriteColor(YAML::Emitter& out, std::string_view key, const Color& c);

// Writes a Mat4f as a 16-element flat float flow sequence (row-major) under
// key. Round-trips with core::ParseMat4() from YamlUtils.h.
void WriteMat4(YAML::Emitter& out, std::string_view key, const Mat4f& m);

// Reads node[key] as a relative path string and resolves it against base_dir.
// Returns an empty path and logs a warning when the key is absent or not a
// scalar string.
std::filesystem::path ReadPath(const YAML::Node& node,
                               std::string_view key,
                               const std::filesystem::path& base_dir);

// Writes path as a string relative to base_dir under key.
// Falls back to the absolute path string when the two paths share no common
// root (e.g. different drives on Windows).
void WritePath(YAML::Emitter& out,
               std::string_view key,
               const std::filesystem::path& path,
               const std::filesystem::path& base_dir);

}  // namespace core::yaml
