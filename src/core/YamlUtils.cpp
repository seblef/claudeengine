#include "core/YamlUtils.h"

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
    LOG_F(WARNING, "YamlUtils::ParseMat4: invalid node, using identity");
    return Mat4f::kIdentity;
  }
  float data[16];
  for (int i = 0; i < 16; ++i)
    data[i] = node[i].as<float>(0.f);
  return Mat4f(data);
}

}  // namespace core
