#include "vfx/VFXSystem.h"

#include <memory>

#include <gtest/gtest.h>

#include "core/Vec3f.h"

using core::Vec3f;

namespace {

// Minimal concrete stub — finishes after a fixed number of seconds have
// elapsed since Play(), with no rendering/audio/physics plumbing.
class NoOpEffect : public vfx::IVFXEffect {
 public:
  explicit NoOpEffect(float lifetime) : lifetime_(lifetime) {}

  void Play(const Vec3f&, const Vec3f&) override { elapsed_ = 0.f; }
  void Update(float dt) override { elapsed_ += dt; }
  [[nodiscard]] bool IsFinished() const override { return elapsed_ >= lifetime_; }

 private:
  float lifetime_;
  float elapsed_ = 0.f;
};

}  // namespace

TEST(VFXSystemTest, SpawnRegistersActiveEffect) {
  vfx::VFXSystem system;
  system.Spawn(std::make_unique<NoOpEffect>(1.f), Vec3f::kZero, Vec3f::kAxisZ);
  EXPECT_EQ(system.GetActiveEffectCount(), 1u);
}

TEST(VFXSystemTest, UpdateReapsFinishedEffect) {
  vfx::VFXSystem system;
  system.Spawn(std::make_unique<NoOpEffect>(1.f), Vec3f::kZero, Vec3f::kAxisZ);

  system.Update(0.5f);
  EXPECT_EQ(system.GetActiveEffectCount(), 1u);

  system.Update(0.6f);
  EXPECT_EQ(system.GetActiveEffectCount(), 0u);
}

TEST(VFXSystemTest, MultipleEffectsExpireIndependently) {
  vfx::VFXSystem system;
  system.Spawn(std::make_unique<NoOpEffect>(0.2f), Vec3f::kZero, Vec3f::kAxisZ);
  system.Spawn(std::make_unique<NoOpEffect>(1.f), Vec3f::kZero, Vec3f::kAxisZ);
  EXPECT_EQ(system.GetActiveEffectCount(), 2u);

  system.Update(0.3f);
  EXPECT_EQ(system.GetActiveEffectCount(), 1u);
}
