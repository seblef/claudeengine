#include "vfx/VFXExplosion.h"

#include <memory>

#include <gtest/gtest.h>

#include "core/Vec3f.h"
#include "vfx/VFXExplosionDesc.h"
#include "vfx/VFXSystem.h"

using core::Vec3f;
using vfx::VFXExplosion;
using vfx::VFXExplosionDesc;

// ---- Lifecycle (no Renderer/GameSystem/PhysicsSystem/VFXSystem instanced) ------

TEST(VFXExplosionTest, FinishedInitially) {
  VFXExplosion explosion(VFXExplosionDesc{}, nullptr);
  EXPECT_TRUE(explosion.IsFinished());
}

TEST(VFXExplosionTest, PlayClearsFinished) {
  VFXExplosion explosion(VFXExplosionDesc{}, nullptr);
  explosion.Play({0.f, 0.f, 0.f}, {0.f, 1.f, 0.f});
  EXPECT_FALSE(explosion.IsFinished());
}

TEST(VFXExplosionTest, UpdateWithNoSubsystemsDoesNotCrash) {
  VFXExplosion explosion(VFXExplosionDesc{}, nullptr);
  explosion.Play({0.f, 0.f, 0.f}, {0.f, 1.f, 0.f});
  EXPECT_NO_THROW(explosion.Update(0.016f));
  EXPECT_FALSE(explosion.IsFinished());
}

TEST(VFXExplosionTest, SelfExpiresAfterParticleDuration) {
  VFXExplosionDesc desc;
  desc.particle_duration = 1.f;
  VFXExplosion explosion(desc, nullptr);
  explosion.Play({0.f, 0.f, 0.f}, {0.f, 1.f, 0.f});

  explosion.Update(0.5f);
  EXPECT_FALSE(explosion.IsFinished());

  explosion.Update(0.6f);
  EXPECT_TRUE(explosion.IsFinished());
}

TEST(VFXExplosionTest, DestroyingActiveEffectDoesNotCrash) {
  auto explosion = std::make_unique<VFXExplosion>(VFXExplosionDesc{}, nullptr);
  explosion->Play({0.f, 0.f, 0.f}, {0.f, 1.f, 0.f});
  EXPECT_NO_THROW(explosion.reset());
}

// ---- Composition via VFXSystem --------------------------------------------------

TEST(VFXExplosionTest, SpawnThroughVFXSystemDoesNotCrashWithoutRenderer) {
  vfx::VFXSystem system;
  EXPECT_NO_THROW(system.Spawn(std::make_unique<VFXExplosion>(VFXExplosionDesc{}, nullptr),
                               Vec3f::kZero, Vec3f::kAxisZ));
  // The explosion itself plus the ScreenShake its Play() spawns alongside it.
  EXPECT_EQ(system.GetActiveEffectCount(), 2u);
}

class VFXExplosionSystemTest : public ::testing::Test {
 protected:
  void SetUp() override { new vfx::VFXSystem(); }
  void TearDown() override { vfx::VFXSystem::Shutdown(); }
};

TEST_F(VFXExplosionSystemTest, PlayingAlsoSpawnsAScreenShakeEffect) {
  VFXExplosion explosion(VFXExplosionDesc{}, nullptr);
  explosion.Play({0.f, 0.f, 0.f}, {0.f, 1.f, 0.f});

  // TriggerShake() spawns a vfx::ScreenShake directly into the VFXSystem
  // singleton (independent from `explosion`, which isn't owned by it here).
  EXPECT_EQ(vfx::VFXSystem::Instance().GetActiveEffectCount(), 1u);
}

TEST(VFXExplosionTest, PlayWithoutVFXSystemInstancedDoesNotCrash) {
  VFXExplosion explosion(VFXExplosionDesc{}, nullptr);
  EXPECT_NO_THROW(explosion.Play({0.f, 0.f, 0.f}, {0.f, 1.f, 0.f}));
}
