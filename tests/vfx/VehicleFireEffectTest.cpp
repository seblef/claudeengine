#include "vfx/VehicleFireEffect.h"

#include <memory>

#include <gtest/gtest.h>

#include "core/Mat4f.h"
#include "core/Vec3f.h"
#include "game/DamageZone.h"
#include "game/VehicleDamage.h"
#include "physics/VehicleDesc.h"
#include "vfx/VFXSystem.h"

using game::VehicleDamage;
using vfx::VehicleFireEffect;

namespace {

physics::FireDesc MakeFireDesc() {
  physics::FireDesc desc;
  desc.damage_threshold = 0.8f;
  return desc;
}

// half_extents chosen so {0,0,5} classifies as Front; impulse_to_damage_scale
// = 1 so damage == impulse, matching tests/game/VehicleDamageTest.cpp.
physics::VehicleDesc MakeVehicleDesc() {
  physics::VehicleDesc desc;
  desc.half_extents                  = {2.f, 1.f, 3.f};
  desc.damage.zone_max_hp            = {100.f, 100.f, 100.f, 100.f, 100.f};
  desc.damage.impulse_to_damage_scale = 1.f;
  desc.damage.thresholds             = {0.4f, 0.6f, 0.8f, 1.f};
  return desc;
}

class VehicleFireEffectTest : public ::testing::Test {
 protected:
  void SetUp() override { new vfx::VFXSystem(); }
  void TearDown() override { vfx::VFXSystem::Shutdown(); }
};

}  // namespace

TEST_F(VehicleFireEffectTest, InactiveInitially) {
  VehicleDamage damage(MakeVehicleDesc());
  VehicleFireEffect fire(MakeFireDesc(), damage);
  EXPECT_FALSE(fire.IsActive());
}

TEST_F(VehicleFireEffectTest, DamageBelowThresholdStaysInactive) {
  VehicleDamage damage(MakeVehicleDesc());
  VehicleFireEffect fire(MakeFireDesc(), damage);
  damage.AddListener(&fire);

  damage.RegisterImpact({0.f, 0.f, 5.f}, 50.f);  // fraction 0.5, crosses 0.4/0.6 only

  EXPECT_FALSE(fire.IsActive());
}

TEST_F(VehicleFireEffectTest, CrossingDamageThresholdActivatesFire) {
  VehicleDamage damage(MakeVehicleDesc());
  VehicleFireEffect fire(MakeFireDesc(), damage);
  damage.AddListener(&fire);

  damage.RegisterImpact({0.f, 0.f, 5.f}, 85.f);  // fraction 0.85, crosses 0.8

  EXPECT_TRUE(fire.IsActive());
}

TEST_F(VehicleFireEffectTest, AnyZoneCrossingThresholdActivatesFire) {
  VehicleDamage damage(MakeVehicleDesc());
  VehicleFireEffect fire(MakeFireDesc(), damage);
  damage.AddListener(&fire);

  damage.RegisterImpact({5.f, 0.f, 0.f}, 85.f);  // Left zone, fraction 0.85

  EXPECT_TRUE(fire.IsActive());
}

TEST_F(VehicleFireEffectTest, FollowsTransformWhileActive) {
  VehicleDamage damage(MakeVehicleDesc());
  VehicleFireEffect fire(MakeFireDesc(), damage);
  damage.AddListener(&fire);
  damage.RegisterImpact({0.f, 0.f, 5.f}, 85.f);
  ASSERT_TRUE(fire.IsActive());

  EXPECT_NO_THROW(fire.OnVehicleTransformUpdated(core::Mat4f::Translation({1.f, 2.f, 3.f})));
  EXPECT_TRUE(fire.IsActive());
}

TEST_F(VehicleFireEffectTest, CrossingWreckThresholdStopsFire) {
  VehicleDamage damage(MakeVehicleDesc());
  VehicleFireEffect fire(MakeFireDesc(), damage);
  damage.AddListener(&fire);
  damage.RegisterImpact({0.f, 0.f, 5.f}, 85.f);
  ASSERT_TRUE(fire.IsActive());

  damage.RegisterImpact({0.f, 0.f, 5.f}, 1000.f);  // drives fraction to 1.0 (wreck)

  EXPECT_FALSE(fire.IsActive());
}

TEST(VehicleFireEffectNoSystemTest, CrossingThresholdWithoutVFXSystemStaysInactive) {
  VehicleDamage damage(MakeVehicleDesc());
  VehicleFireEffect fire(MakeFireDesc(), damage);
  damage.AddListener(&fire);

  damage.RegisterImpact({0.f, 0.f, 5.f}, 85.f);

  EXPECT_FALSE(fire.IsActive());
}

TEST(VehicleFireEffectNoSystemTest, DestroyingActiveEffectDoesNotCrash) {
  VehicleDamage damage(MakeVehicleDesc());
  auto fire = std::make_unique<VehicleFireEffect>(MakeFireDesc(), damage);
  EXPECT_NO_THROW(fire.reset());
}
