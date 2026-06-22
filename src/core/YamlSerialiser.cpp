#include "core/YamlSerialiser.h"

#include <loguru.hpp>

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
