#include "game/VehicleDamage.h"

#include <vector>

#include <gtest/gtest.h>

#include "core/Vec3f.h"
#include "game/DamageZone.h"
#include "game/IVehicleDamageListener.h"
#include "physics/VehicleDesc.h"

using core::Vec3f;
using game::DamageZone;
using game::IVehicleDamageListener;
using game::VehicleDamage;

namespace {

struct Crossing {
  DamageZone zone;
  float      threshold;
  float      fraction;
};

class RecordingListener : public IVehicleDamageListener {
 public:
  void OnDamageThresholdCrossed(DamageZone zone, float threshold,
                                float fraction) override {
    crossings.push_back({zone, threshold, fraction});
  }

  std::vector<Crossing> crossings;
};

// half_extents = (2, 1, 3); impulse_to_damage_scale = 1 so damage == impulse.
physics::VehicleDesc MakeDesc() {
  physics::VehicleDesc desc;
  desc.half_extents                     = {2.f, 1.f, 3.f};
  desc.damage.zone_max_hp                = {100.f, 100.f, 100.f, 100.f, 100.f};
  desc.damage.impulse_to_damage_scale    = 1.f;
  desc.damage.thresholds                 = {0.4f, 0.6f, 0.8f, 1.f};
  return desc;
}

}  // namespace

// ---- Zone classification ---------------------------------------------------

TEST(VehicleDamageTest, ClassifiesFront) {
  VehicleDamage damage(MakeDesc());
  damage.RegisterImpact({0.f, 0.f, 5.f}, 10.f);
  EXPECT_FLOAT_EQ(damage.GetHP(DamageZone::kFront), 90.f);
  EXPECT_FLOAT_EQ(damage.GetHP(DamageZone::kRear), 100.f);
}

TEST(VehicleDamageTest, ClassifiesRear) {
  VehicleDamage damage(MakeDesc());
  damage.RegisterImpact({0.f, 0.f, -5.f}, 10.f);
  EXPECT_FLOAT_EQ(damage.GetHP(DamageZone::kRear), 90.f);
}

TEST(VehicleDamageTest, ClassifiesLeft) {
  VehicleDamage damage(MakeDesc());
  damage.RegisterImpact({5.f, 0.f, 0.f}, 10.f);
  EXPECT_FLOAT_EQ(damage.GetHP(DamageZone::kLeft), 90.f);
}

TEST(VehicleDamageTest, ClassifiesRight) {
  VehicleDamage damage(MakeDesc());
  damage.RegisterImpact({-5.f, 0.f, 0.f}, 10.f);
  EXPECT_FLOAT_EQ(damage.GetHP(DamageZone::kRight), 90.f);
}

TEST(VehicleDamageTest, ClassifiesRoof) {
  VehicleDamage damage(MakeDesc());
  damage.RegisterImpact({0.f, 5.f, 0.f}, 10.f);
  EXPECT_FLOAT_EQ(damage.GetHP(DamageZone::kRoof), 90.f);
}

TEST(VehicleDamageTest, UndersideFallsBackToNextDominantAxis) {
  VehicleDamage damage(MakeDesc());
  // ny is dominant but negative (underside); nz is the next largest → Front.
  damage.RegisterImpact({0.f, -5.f, 1.f}, 10.f);
  EXPECT_FLOAT_EQ(damage.GetHP(DamageZone::kFront), 90.f);
  EXPECT_FLOAT_EQ(damage.GetHP(DamageZone::kRoof), 100.f);
}

// ---- Impulse → damage mapping ----------------------------------------------

TEST(VehicleDamageTest, DamageProportionalToImpulse) {
  VehicleDamage damage(MakeDesc());
  damage.RegisterImpact({0.f, 0.f, 5.f}, 25.f);
  EXPECT_FLOAT_EQ(damage.GetDamageFraction(DamageZone::kFront), 0.25f);
}

TEST(VehicleDamageTest, ZeroOrNegativeImpulseIsNoOp) {
  VehicleDamage damage(MakeDesc());
  damage.RegisterImpact({0.f, 0.f, 5.f}, 0.f);
  damage.RegisterImpact({0.f, 0.f, 5.f}, -5.f);
  EXPECT_FLOAT_EQ(damage.GetHP(DamageZone::kFront), 100.f);
}

TEST(VehicleDamageTest, DamageClampsAtZeroHP) {
  VehicleDamage damage(MakeDesc());
  damage.RegisterImpact({0.f, 0.f, 5.f}, 1000.f);
  EXPECT_FLOAT_EQ(damage.GetHP(DamageZone::kFront), 0.f);
  EXPECT_FLOAT_EQ(damage.GetDamageFraction(DamageZone::kFront), 1.f);
}

// ---- Listener threshold notifications --------------------------------------

TEST(VehicleDamageTest, NoNotificationBelowFirstThreshold) {
  VehicleDamage damage(MakeDesc());
  RecordingListener listener;
  damage.AddListener(&listener);

  damage.RegisterImpact({0.f, 0.f, 5.f}, 30.f);  // fraction 0.3 < 0.4

  EXPECT_TRUE(listener.crossings.empty());
}

TEST(VehicleDamageTest, NotifiesOnceWhenCrossingOneThreshold) {
  VehicleDamage damage(MakeDesc());
  RecordingListener listener;
  damage.AddListener(&listener);

  damage.RegisterImpact({0.f, 0.f, 5.f}, 30.f);  // fraction 0.3
  damage.RegisterImpact({0.f, 0.f, 5.f}, 15.f);  // fraction 0.45 → crosses 0.4

  ASSERT_EQ(listener.crossings.size(), 1u);
  EXPECT_EQ(listener.crossings[0].zone, DamageZone::kFront);
  EXPECT_FLOAT_EQ(listener.crossings[0].threshold, 0.4f);
  EXPECT_FLOAT_EQ(listener.crossings[0].fraction, 0.45f);
}

TEST(VehicleDamageTest, NotifiesForEveryThresholdCrossedInOneImpact) {
  VehicleDamage damage(MakeDesc());
  RecordingListener listener;
  damage.AddListener(&listener);

  // 90 damage on a pristine 100-HP zone → fraction 0.9, crosses 0.4/0.6/0.8.
  damage.RegisterImpact({0.f, 0.f, 5.f}, 90.f);

  ASSERT_EQ(listener.crossings.size(), 3u);
  EXPECT_FLOAT_EQ(listener.crossings[0].threshold, 0.4f);
  EXPECT_FLOAT_EQ(listener.crossings[1].threshold, 0.6f);
  EXPECT_FLOAT_EQ(listener.crossings[2].threshold, 0.8f);
}

TEST(VehicleDamageTest, DestructionCrossesFinalThreshold) {
  VehicleDamage damage(MakeDesc());
  RecordingListener listener;
  damage.AddListener(&listener);

  damage.RegisterImpact({0.f, 0.f, 5.f}, 1000.f);

  ASSERT_EQ(listener.crossings.size(), 4u);
  EXPECT_FLOAT_EQ(listener.crossings.back().threshold, 1.f);
  EXPECT_FLOAT_EQ(listener.crossings.back().fraction, 1.f);
}

TEST(VehicleDamageTest, RemoveListenerStopsNotifications) {
  VehicleDamage damage(MakeDesc());
  RecordingListener listener;
  damage.AddListener(&listener);
  damage.RemoveListener(&listener);

  damage.RegisterImpact({0.f, 0.f, 5.f}, 90.f);

  EXPECT_TRUE(listener.crossings.empty());
}

TEST(VehicleDamageTest, UnaffectedZonesDoNotNotify) {
  VehicleDamage damage(MakeDesc());
  RecordingListener listener;
  damage.AddListener(&listener);

  damage.RegisterImpact({0.f, 0.f, 5.f}, 90.f);  // Front only

  for (const Crossing& c : listener.crossings)
    EXPECT_EQ(c.zone, DamageZone::kFront);
}
