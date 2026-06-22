#include "core/YamlSerialiser.h"

#include <filesystem>
#include <sstream>
#include <string>

#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

#include "core/Color.h"
#include "core/Mat4f.h"
#include "core/Vec3f.h"

using namespace core;
namespace fs = std::filesystem;

namespace {

// Emits a single-key map using fn and parses the result back to a YAML node.
template <typename Fn>
YAML::Node EmitAndParse(Fn fn) {
  YAML::Emitter out;
  out << YAML::BeginMap;
  fn(out);
  out << YAML::EndMap;
  return YAML::Load(out.c_str());
}

}  // namespace

// ---- WriteVec3f -------------------------------------------------------------

TEST(YamlSerialiserTest, WriteVec3fRoundTrip) {
  const Vec3f v{1.f, 2.f, 3.f};
  auto node = EmitAndParse([&](YAML::Emitter& e) {
    core::yaml::WriteVec3f(e, "pos", v);
  });
  ASSERT_TRUE(node["pos"].IsSequence());
  ASSERT_EQ(node["pos"].size(), 3u);
  EXPECT_FLOAT_EQ(node["pos"][0].as<float>(), 1.f);
  EXPECT_FLOAT_EQ(node["pos"][1].as<float>(), 2.f);
  EXPECT_FLOAT_EQ(node["pos"][2].as<float>(), 3.f);
}

TEST(YamlSerialiserTest, WriteVec3fZero) {
  auto node = EmitAndParse([](YAML::Emitter& e) {
    core::yaml::WriteVec3f(e, "v", Vec3f::kZero);
  });
  ASSERT_EQ(node["v"].size(), 3u);
  EXPECT_FLOAT_EQ(node["v"][0].as<float>(), 0.f);
}

// ---- WriteColor -------------------------------------------------------------

TEST(YamlSerialiserTest, WriteColorRoundTrip) {
  const Color c{0.1f, 0.2f, 0.3f, 0.4f};
  auto node = EmitAndParse([&](YAML::Emitter& e) {
    core::yaml::WriteColor(e, "col", c);
  });
  ASSERT_TRUE(node["col"].IsSequence());
  ASSERT_EQ(node["col"].size(), 4u);
  EXPECT_FLOAT_EQ(node["col"][0].as<float>(), 0.1f);
  EXPECT_FLOAT_EQ(node["col"][1].as<float>(), 0.2f);
  EXPECT_FLOAT_EQ(node["col"][2].as<float>(), 0.3f);
  EXPECT_FLOAT_EQ(node["col"][3].as<float>(), 0.4f);
}

TEST(YamlSerialiserTest, WriteColorWhite) {
  auto node = EmitAndParse([](YAML::Emitter& e) {
    core::yaml::WriteColor(e, "c", Color::kWhite);
  });
  ASSERT_EQ(node["c"].size(), 4u);
  EXPECT_FLOAT_EQ(node["c"][3].as<float>(), 1.f);
}

// ---- WriteMat4 --------------------------------------------------------------

TEST(YamlSerialiserTest, WriteMat4Identity) {
  auto node = EmitAndParse([](YAML::Emitter& e) {
    core::yaml::WriteMat4(e, "mat", Mat4f::kIdentity);
  });
  ASSERT_TRUE(node["mat"].IsSequence());
  ASSERT_EQ(node["mat"].size(), 16u);
  // Diagonal elements are 1; off-diagonal are 0.
  EXPECT_FLOAT_EQ(node["mat"][0].as<float>(),  1.f);
  EXPECT_FLOAT_EQ(node["mat"][1].as<float>(),  0.f);
  EXPECT_FLOAT_EQ(node["mat"][5].as<float>(),  1.f);
  EXPECT_FLOAT_EQ(node["mat"][10].as<float>(), 1.f);
  EXPECT_FLOAT_EQ(node["mat"][15].as<float>(), 1.f);
}

TEST(YamlSerialiserTest, WriteMat4RoundTrip) {
  Mat4f m{1.f,  2.f,  3.f,  4.f,
          5.f,  6.f,  7.f,  8.f,
          9.f,  10.f, 11.f, 12.f,
          13.f, 14.f, 15.f, 16.f};
  auto node = EmitAndParse([&](YAML::Emitter& e) {
    core::yaml::WriteMat4(e, "m", m);
  });
  ASSERT_EQ(node["m"].size(), 16u);
  for (int i = 0; i < 16; ++i)
    EXPECT_FLOAT_EQ(node["m"][i].as<float>(), static_cast<float>(i + 1));
}

// ---- ReadPath ---------------------------------------------------------------

TEST(YamlSerialiserTest, ReadPathValid) {
  YAML::Node node = YAML::Load("mesh: models/car.obj");
  const fs::path base{"/data/vehicles"};
  const fs::path result = core::yaml::ReadPath(node, "mesh", base);
  EXPECT_EQ(result, base / "models/car.obj");
}

TEST(YamlSerialiserTest, ReadPathMissingKeyReturnsEmpty) {
  YAML::Node node = YAML::Load("other: value");
  const fs::path result = core::yaml::ReadPath(node, "mesh", "/data");
  EXPECT_TRUE(result.empty());
}

TEST(YamlSerialiserTest, ReadPathNonScalarReturnsEmpty) {
  YAML::Node node = YAML::Load("mesh: [a, b]");
  const fs::path result = core::yaml::ReadPath(node, "mesh", "/data");
  EXPECT_TRUE(result.empty());
}

// ---- WritePath / ReadPath round-trip ----------------------------------------

TEST(YamlSerialiserTest, WritePathRelative) {
  const fs::path base{"/data/vehicles"};
  const fs::path abs_path{"/data/vehicles/models/car.obj"};
  auto node = EmitAndParse([&](YAML::Emitter& e) {
    core::yaml::WritePath(e, "mesh", abs_path, base);
  });
  ASSERT_TRUE(node["mesh"].IsScalar());
  EXPECT_EQ(node["mesh"].as<std::string>(), "models/car.obj");
}

TEST(YamlSerialiserTest, WritePathReadPathRoundTrip) {
  const fs::path base{"/data/vehicles"};
  const fs::path original{"/data/vehicles/textures/diffuse.png"};
  auto node = EmitAndParse([&](YAML::Emitter& e) {
    core::yaml::WritePath(e, "tex", original, base);
  });
  const fs::path recovered = core::yaml::ReadPath(node, "tex", base);
  EXPECT_EQ(recovered, original);
}
