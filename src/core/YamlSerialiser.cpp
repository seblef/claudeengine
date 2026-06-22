#include "core/YamlSerialiser.h"

#include <stdexcept>
#include <vector>

#include <loguru.hpp>

namespace core {

YAML::Node LoadYamlFile(const std::filesystem::path& path) {
  try {
    return YAML::LoadFile(path.string());
  } catch (const YAML::Exception& e) {
    throw std::runtime_error("Failed to load YAML '" + path.string() + "': " + e.what());
  }
}

Vec3f ParseVec3(const YAML::Node& node, const Vec3f& fallback) {
  if (!node || !node.IsSequence() || node.size() < 3)
    return fallback;
  return {node[0].as<float>(fallback.x),
          node[1].as<float>(fallback.y),
          node[2].as<float>(fallback.z)};
}

Color ParseColor(const YAML::Node& node, const Color& fallback) {
  if (!node || !node.IsSequence() || node.size() == 0)
    return fallback;
  const auto v = node.as<std::vector<float>>();
  if (v.size() >= 4) return {v[0], v[1], v[2], v[3]};
  if (v.size() >= 3) return {v[0], v[1], v[2]};
  if (v.size() == 1) return Color{v[0]};
  return fallback;
}

Mat4f ParseMat4(const YAML::Node& node) {
  if (!node || !node.IsSequence() || node.size() != 16) {
    LOG_F(WARNING, "YamlSerialiser::ParseMat4: invalid node, using identity");
    return Mat4f::kIdentity;
  }
  float data[16];
  for (int i = 0; i < 16; ++i)
    data[i] = node[i].as<float>(0.f);
  return Mat4f(data);
}

}  // namespace core

namespace core::yaml {

void WriteVec3f(YAML::Emitter& out, std::string_view key, const Vec3f& v) {
  out << YAML::Key << std::string(key) << YAML::Value;
  out << YAML::Flow << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
}

void WriteColor(YAML::Emitter& out, std::string_view key, const Color& c) {
  out << YAML::Key << std::string(key) << YAML::Value;
  out << YAML::Flow << YAML::BeginSeq << c.r << c.g << c.b << c.a << YAML::EndSeq;
}

void WriteMat4(YAML::Emitter& out, std::string_view key, const Mat4f& m) {
  out << YAML::Key << std::string(key) << YAML::Value;
  out << YAML::Flow << YAML::BeginSeq;
  for (int i = 0; i < 16; ++i)
    out << m.Data()[i];
  out << YAML::EndSeq;
}

std::filesystem::path ReadPath(const YAML::Node& node,
                               std::string_view key,
                               const std::filesystem::path& base_dir) {
  const YAML::Node entry = node[std::string(key)];
  if (!entry || !entry.IsScalar()) {
    LOG_F(WARNING, "YamlSerialiser::ReadPath: missing or non-scalar key '%.*s'",
          static_cast<int>(key.size()), key.data());
    return {};
  }
  return base_dir / entry.as<std::string>();
}

void WritePath(YAML::Emitter& out,
               std::string_view key,
               const std::filesystem::path& path,
               const std::filesystem::path& base_dir) {
  const std::filesystem::path rel = path.lexically_relative(base_dir);
  const std::string s = rel.empty() ? path.string() : rel.string();
  out << YAML::Key << std::string(key) << YAML::Value << s;
}

}  // namespace core::yaml
