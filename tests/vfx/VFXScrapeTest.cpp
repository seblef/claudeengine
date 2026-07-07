#include "vfx/VFXScrape.h"

#include <gtest/gtest.h>

#include "core/Vec3f.h"
#include "physics/VehicleDesc.h"

using core::Vec3f;
using vfx::VFXScrape;

namespace {

physics::ScrapeDesc MakeDesc() {
  physics::ScrapeDesc desc;
  desc.min_speed          = 1.f;
  desc.max_speed          = 11.f;
  desc.base_emission_rate = 100.f;
  desc.base_gain          = 1.f;
  desc.contact_grace_time = 0.2f;
  return desc;
}

}  // namespace

// ---- ComputeIntensity ---------------------------------------------------------

TEST(VFXScrapeTest, IntensityIsZeroBelowMinSpeed) {
  const physics::ScrapeDesc desc = MakeDesc();
  EXPECT_FLOAT_EQ(VFXScrape::ComputeIntensity(0.f, desc), 0.f);
  EXPECT_FLOAT_EQ(VFXScrape::ComputeIntensity(1.f, desc), 0.f);
}

TEST(VFXScrapeTest, IntensityIsOneAtOrAboveMaxSpeed) {
  const physics::ScrapeDesc desc = MakeDesc();
  EXPECT_FLOAT_EQ(VFXScrape::ComputeIntensity(11.f, desc), 1.f);
  EXPECT_FLOAT_EQ(VFXScrape::ComputeIntensity(100.f, desc), 1.f);
}

TEST(VFXScrapeTest, IntensityIsLinearBetweenThresholds) {
  const physics::ScrapeDesc desc = MakeDesc();
  EXPECT_FLOAT_EQ(VFXScrape::ComputeIntensity(6.f, desc), 0.5f);
}

TEST(VFXScrapeTest, IntensityHandlesZeroSpanWithoutDividingByZero) {
  physics::ScrapeDesc desc = MakeDesc();
  desc.max_speed = desc.min_speed;
  EXPECT_FLOAT_EQ(VFXScrape::ComputeIntensity(0.f, desc), 0.f);
  EXPECT_FLOAT_EQ(VFXScrape::ComputeIntensity(desc.min_speed, desc), 1.f);
}

// ---- ReflectDirection ----------------------------------------------------------

TEST(VFXScrapeTest, HeadOnIncomingBouncesStraightBack) {
  const Vec3f reflected = VFXScrape::ReflectDirection({1.f, 0.f, 0.f}, {1.f, 0.f, 0.f});
  EXPECT_NEAR(reflected.x, -1.f, 1e-5f);
  EXPECT_NEAR(reflected.y, 0.f, 1e-5f);
  EXPECT_NEAR(reflected.z, 0.f, 1e-5f);
}

TEST(VFXScrapeTest, PurelyTangentialIncomingIsUnaffected) {
  const Vec3f reflected = VFXScrape::ReflectDirection({0.f, 1.f, 0.f}, {1.f, 0.f, 0.f});
  EXPECT_NEAR(reflected.x, 0.f, 1e-5f);
  EXPECT_NEAR(reflected.y, 1.f, 1e-5f);
  EXPECT_NEAR(reflected.z, 0.f, 1e-5f);
}

TEST(VFXScrapeTest, ReflectedDirectionIsNormalised) {
  const Vec3f reflected = VFXScrape::ReflectDirection({3.f, 4.f, 0.f}, {0.f, 1.f, 0.f});
  EXPECT_NEAR(reflected.Length(), 1.f, 1e-5f);
}

TEST(VFXScrapeTest, ZeroLengthIncomingFallsBackToNormal) {
  const Vec3f reflected = VFXScrape::ReflectDirection({0.f, 0.f, 0.f}, {0.f, 1.f, 0.f});
  EXPECT_NEAR(reflected.x, 0.f, 1e-5f);
  EXPECT_NEAR(reflected.y, 1.f, 1e-5f);
  EXPECT_NEAR(reflected.z, 0.f, 1e-5f);
}

// ---- Play / Stop lifecycle (no Renderer/audio instanced) -----------------------

TEST(VFXScrapeTest, FinishedInitiallyAndAfterConstruction) {
  VFXScrape scrape(MakeDesc(), nullptr, nullptr, nullptr);
  EXPECT_TRUE(scrape.IsFinished());
}

TEST(VFXScrapeTest, PlayClearsFinished) {
  VFXScrape scrape(MakeDesc(), nullptr, nullptr, nullptr);
  scrape.Play({0.f, 0.f, 0.f}, {0.f, 1.f, 0.f});
  EXPECT_FALSE(scrape.IsFinished());
}

TEST(VFXScrapeTest, StopSetsFinished) {
  VFXScrape scrape(MakeDesc(), nullptr, nullptr, nullptr);
  scrape.Play({0.f, 0.f, 0.f}, {0.f, 1.f, 0.f});
  scrape.Stop();
  EXPECT_TRUE(scrape.IsFinished());
}

TEST(VFXScrapeTest, StopIsIdempotent) {
  VFXScrape scrape(MakeDesc(), nullptr, nullptr, nullptr);
  scrape.Play({0.f, 0.f, 0.f}, {0.f, 1.f, 0.f});
  scrape.Stop();
  EXPECT_NO_THROW(scrape.Stop());
  EXPECT_TRUE(scrape.IsFinished());
}

TEST(VFXScrapeTest, UpdateContactAfterStopIsANoOp) {
  VFXScrape scrape(MakeDesc(), nullptr, nullptr, nullptr);
  scrape.Play({0.f, 0.f, 0.f}, {0.f, 1.f, 0.f});
  scrape.Stop();
  EXPECT_NO_THROW(
      scrape.UpdateContact({1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {2.f, 0.f, 0.f}));
  EXPECT_TRUE(scrape.IsFinished());
}

TEST(VFXScrapeTest, UpdateWithNoRendererDoesNotCrash) {
  VFXScrape scrape(MakeDesc(), nullptr, nullptr, nullptr);
  scrape.Play({0.f, 0.f, 0.f}, {0.f, 1.f, 0.f});
  EXPECT_NO_THROW(scrape.Update(0.016f));
}
