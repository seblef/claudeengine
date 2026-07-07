#include "game/VehicleCrashSound.h"

#include <gtest/gtest.h>

#include "core/Vec3f.h"
#include "physics/VehicleDesc.h"

using core::Vec3f;
using game::VehicleCrashSound;

namespace {

physics::CrashSoundDesc MakeDesc() {
  physics::CrashSoundDesc desc;
  desc.min_impulse    = 100.f;
  desc.medium_impulse = 300.f;
  desc.heavy_impulse  = 600.f;
  desc.max_impulse    = 1000.f;
  desc.debounce_time  = 0.5f;
  return desc;
}

}  // namespace

// ---- Tier classification ----------------------------------------------------

TEST(VehicleCrashSoundTest, BelowMinimumClassifiesAsNone) {
  const physics::CrashSoundDesc desc = MakeDesc();
  EXPECT_EQ(VehicleCrashSound::ClassifyTier(50.f, desc), VehicleCrashSound::Tier::kNone);
  EXPECT_EQ(VehicleCrashSound::ClassifyTier(99.9f, desc), VehicleCrashSound::Tier::kNone);
}

TEST(VehicleCrashSoundTest, BetweenMinAndMediumClassifiesAsLight) {
  const physics::CrashSoundDesc desc = MakeDesc();
  EXPECT_EQ(VehicleCrashSound::ClassifyTier(100.f, desc), VehicleCrashSound::Tier::kLight);
  EXPECT_EQ(VehicleCrashSound::ClassifyTier(299.f, desc), VehicleCrashSound::Tier::kLight);
}

TEST(VehicleCrashSoundTest, BetweenMediumAndHeavyClassifiesAsMedium) {
  const physics::CrashSoundDesc desc = MakeDesc();
  EXPECT_EQ(VehicleCrashSound::ClassifyTier(300.f, desc), VehicleCrashSound::Tier::kMedium);
  EXPECT_EQ(VehicleCrashSound::ClassifyTier(599.f, desc), VehicleCrashSound::Tier::kMedium);
}

TEST(VehicleCrashSoundTest, AtOrAboveHeavyClassifiesAsHeavy) {
  const physics::CrashSoundDesc desc = MakeDesc();
  EXPECT_EQ(VehicleCrashSound::ClassifyTier(600.f, desc), VehicleCrashSound::Tier::kHeavy);
  EXPECT_EQ(VehicleCrashSound::ClassifyTier(5000.f, desc), VehicleCrashSound::Tier::kHeavy);
}

// ---- Gain scaling ------------------------------------------------------------

TEST(VehicleCrashSoundTest, GainIsProportionalToImpulse) {
  const physics::CrashSoundDesc desc = MakeDesc();
  EXPECT_FLOAT_EQ(VehicleCrashSound::ComputeGain(250.f, desc), 0.25f);
  EXPECT_FLOAT_EQ(VehicleCrashSound::ComputeGain(500.f, desc), 0.5f);
}

TEST(VehicleCrashSoundTest, GainSaturatesAtMaxImpulse) {
  const physics::CrashSoundDesc desc = MakeDesc();
  EXPECT_FLOAT_EQ(VehicleCrashSound::ComputeGain(1000.f, desc), 1.f);
  EXPECT_FLOAT_EQ(VehicleCrashSound::ComputeGain(5000.f, desc), 1.f);
}

TEST(VehicleCrashSoundTest, ZeroMaxImpulseFallsBackToFullGain) {
  physics::CrashSoundDesc desc = MakeDesc();
  desc.max_impulse = 0.f;
  EXPECT_FLOAT_EQ(VehicleCrashSound::ComputeGain(10.f, desc), 1.f);
}

// ---- Debounce ----------------------------------------------------------------

TEST(VehicleCrashSoundTest, NotDebouncedInitially) {
  VehicleCrashSound crash_sound(MakeDesc(), nullptr, nullptr);
  EXPECT_FALSE(crash_sound.IsDebounced());
}

TEST(VehicleCrashSoundTest, ImpactBelowMinimumDoesNotArmCooldown) {
  VehicleCrashSound crash_sound(MakeDesc(), nullptr, nullptr);
  crash_sound.RegisterImpact({0.f, 0.f, 0.f}, 50.f);
  EXPECT_FALSE(crash_sound.IsDebounced());
}

TEST(VehicleCrashSoundTest, QualifyingImpactArmsCooldown) {
  VehicleCrashSound crash_sound(MakeDesc(), nullptr, nullptr);
  crash_sound.RegisterImpact({0.f, 0.f, 0.f}, 800.f);
  EXPECT_TRUE(crash_sound.IsDebounced());
}

TEST(VehicleCrashSoundTest, CooldownExpiresAfterDebounceTime) {
  VehicleCrashSound crash_sound(MakeDesc(), nullptr, nullptr);
  crash_sound.RegisterImpact({0.f, 0.f, 0.f}, 800.f);
  crash_sound.Update(0.4f);
  EXPECT_TRUE(crash_sound.IsDebounced());
  crash_sound.Update(0.2f);
  EXPECT_FALSE(crash_sound.IsDebounced());
}

TEST(VehicleCrashSoundTest, ImpactWhileDebouncedDoesNotRearmCooldown) {
  VehicleCrashSound crash_sound(MakeDesc(), nullptr, nullptr);
  crash_sound.RegisterImpact({0.f, 0.f, 0.f}, 800.f);  // cooldown = 0.5s
  crash_sound.Update(0.4f);                            // remaining = 0.1s
  crash_sound.RegisterImpact({0.f, 0.f, 0.f}, 800.f);  // must be ignored (still debounced)
  crash_sound.Update(0.15f);                           // would still be debounced if rearmed
  EXPECT_FALSE(crash_sound.IsDebounced());
}
