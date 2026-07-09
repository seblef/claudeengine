#include "vfx/VFXFire.h"

#include <memory>

#include <gtest/gtest.h>

#include "core/Mat4f.h"
#include "core/Vec3f.h"
#include "vfx/VFXSystem.h"

using core::Vec3f;
using vfx::VFXFire;

// ---- Lifecycle (no Renderer/VFXSystem instanced) ---------------------------

TEST(VFXFireTest, FinishedInitially) {
  VFXFire fire(nullptr, /*heat_distortion=*/false);
  EXPECT_TRUE(fire.IsFinished());
}

TEST(VFXFireTest, PlayClearsFinished) {
  VFXFire fire(nullptr, /*heat_distortion=*/false);
  fire.Play({0.f, 0.f, 0.f}, {0.f, 1.f, 0.f});
  EXPECT_FALSE(fire.IsFinished());
}

TEST(VFXFireTest, UpdateWithNoSubsystemsDoesNotCrash) {
  VFXFire fire(nullptr, /*heat_distortion=*/false);
  fire.Play({0.f, 0.f, 0.f}, {0.f, 1.f, 0.f});
  EXPECT_NO_THROW(fire.Update(0.016f));
  EXPECT_FALSE(fire.IsFinished());
}

TEST(VFXFireTest, DoesNotSelfExpire) {
  VFXFire fire(nullptr, /*heat_distortion=*/false);
  fire.Play({0.f, 0.f, 0.f}, {0.f, 1.f, 0.f});
  for (int i = 0; i < 1000; ++i) fire.Update(1.f);
  EXPECT_FALSE(fire.IsFinished());
}

TEST(VFXFireTest, StopMarksFinished) {
  VFXFire fire(nullptr, /*heat_distortion=*/false);
  fire.Play({0.f, 0.f, 0.f}, {0.f, 1.f, 0.f});
  fire.Stop();
  EXPECT_TRUE(fire.IsFinished());
}

TEST(VFXFireTest, StopIsSafeToCallTwice) {
  VFXFire fire(nullptr, /*heat_distortion=*/false);
  fire.Play({0.f, 0.f, 0.f}, {0.f, 1.f, 0.f});
  fire.Stop();
  EXPECT_NO_THROW(fire.Stop());
}

TEST(VFXFireTest, SetWorldTransformWithNoSubsystemsDoesNotCrash) {
  VFXFire fire(nullptr, /*heat_distortion=*/false);
  fire.Play({0.f, 0.f, 0.f}, {0.f, 1.f, 0.f});
  EXPECT_NO_THROW(fire.SetWorldTransform(core::Mat4f::Translation({1.f, 2.f, 3.f})));
}

TEST(VFXFireTest, DestroyingActiveEffectDoesNotCrash) {
  auto fire = std::make_unique<VFXFire>(nullptr, /*heat_distortion=*/false);
  fire->Play({0.f, 0.f, 0.f}, {0.f, 1.f, 0.f});
  EXPECT_NO_THROW(fire.reset());
}

TEST(VFXFireTest, CarriesHeatDistortionFlag) {
  VFXFire fire(nullptr, /*heat_distortion=*/true);
  EXPECT_TRUE(fire.IsHeatDistortionEnabled());
}

// ---- Composition via VFXSystem ----------------------------------------------

TEST(VFXFireTest, SpawnThroughVFXSystemDoesNotCrashWithoutRenderer) {
  vfx::VFXSystem system;
  EXPECT_NO_THROW(system.Spawn(std::make_unique<VFXFire>(nullptr, /*heat_distortion=*/false),
                               Vec3f::kZero, Vec3f::kAxisZ));
  EXPECT_EQ(system.GetActiveEffectCount(), 1u);
}

TEST(VFXFireTest, PlayWithoutVFXSystemInstancedDoesNotCrash) {
  VFXFire fire(nullptr, /*heat_distortion=*/false);
  EXPECT_NO_THROW(fire.Play({0.f, 0.f, 0.f}, {0.f, 1.f, 0.f}));
}
