#include "vfx/VehicleScrapeEffect.h"

#include <gtest/gtest.h>

#include "core/Vec3f.h"
#include "physics/VehicleDesc.h"
#include "vfx/VFXSystem.h"

using core::Vec3f;
using vfx::VehicleScrapeEffect;

namespace {

physics::ScrapeDesc MakeDesc() {
  physics::ScrapeDesc desc;
  desc.min_speed          = 1.f;
  desc.max_speed          = 11.f;
  desc.contact_grace_time = 0.2f;
  return desc;
}

class VehicleScrapeEffectTest : public ::testing::Test {
 protected:
  void SetUp() override { new vfx::VFXSystem(); }
  void TearDown() override { vfx::VFXSystem::Shutdown(); }
};

}  // namespace

TEST_F(VehicleScrapeEffectTest, InactiveInitially) {
  VehicleScrapeEffect scrape(MakeDesc(), nullptr, nullptr);
  EXPECT_FALSE(scrape.IsActive());
}

TEST_F(VehicleScrapeEffectTest, ContactBelowMinSpeedStaysInactive) {
  VehicleScrapeEffect scrape(MakeDesc(), nullptr, nullptr);
  scrape.RegisterContact({0.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.5f, 0.f, 0.f});
  EXPECT_FALSE(scrape.IsActive());
}

TEST_F(VehicleScrapeEffectTest, ContactAtOrAboveMinSpeedBecomesActive) {
  VehicleScrapeEffect scrape(MakeDesc(), nullptr, nullptr);
  scrape.RegisterContact({0.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {2.f, 0.f, 0.f});
  EXPECT_TRUE(scrape.IsActive());
}

TEST_F(VehicleScrapeEffectTest, GapShorterThanGraceTimeStaysActive) {
  VehicleScrapeEffect scrape(MakeDesc(), nullptr, nullptr);
  scrape.RegisterContact({0.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {2.f, 0.f, 0.f});
  scrape.Update(0.1f);
  EXPECT_TRUE(scrape.IsActive());
}

TEST_F(VehicleScrapeEffectTest, GapLongerThanGraceTimeStops) {
  VehicleScrapeEffect scrape(MakeDesc(), nullptr, nullptr);
  scrape.RegisterContact({0.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {2.f, 0.f, 0.f});
  scrape.Update(0.3f);
  EXPECT_FALSE(scrape.IsActive());
}

TEST_F(VehicleScrapeEffectTest, RefreshedContactResetsGraceTimer) {
  VehicleScrapeEffect scrape(MakeDesc(), nullptr, nullptr);
  scrape.RegisterContact({0.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {2.f, 0.f, 0.f});
  scrape.Update(0.15f);
  scrape.RegisterContact({0.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {2.f, 0.f, 0.f});
  scrape.Update(0.15f);
  EXPECT_TRUE(scrape.IsActive());
}

TEST_F(VehicleScrapeEffectTest, ContactDroppingBelowMinSpeedStopsImmediately) {
  VehicleScrapeEffect scrape(MakeDesc(), nullptr, nullptr);
  scrape.RegisterContact({0.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {2.f, 0.f, 0.f});
  ASSERT_TRUE(scrape.IsActive());
  scrape.RegisterContact({0.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.2f, 0.f, 0.f});
  EXPECT_FALSE(scrape.IsActive());
}

TEST_F(VehicleScrapeEffectTest, UpdateWithNoActiveEffectIsANoOp) {
  VehicleScrapeEffect scrape(MakeDesc(), nullptr, nullptr);
  EXPECT_NO_THROW(scrape.Update(1.f));
  EXPECT_FALSE(scrape.IsActive());
}

TEST(VehicleScrapeEffectNoSystemTest, ContactWithoutVFXSystemStaysInactive) {
  VehicleScrapeEffect scrape(MakeDesc(), nullptr, nullptr);
  scrape.RegisterContact({0.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {2.f, 0.f, 0.f});
  EXPECT_FALSE(scrape.IsActive());
}
