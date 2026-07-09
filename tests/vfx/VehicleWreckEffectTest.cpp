#include "vfx/VehicleWreckEffect.h"

#include <memory>

#include <gtest/gtest.h>

#include "core/Vec3f.h"
#include "physics/VehicleDesc.h"
#include "vfx/VFXSystem.h"

using vfx::VehicleWreckEffect;

namespace {

physics::WreckDesc MakeWreckDesc() {
  physics::WreckDesc desc;
  desc.wreck_sound = "medium-explosion";
  return desc;
}

class VehicleWreckEffectTest : public ::testing::Test {
 protected:
  void SetUp() override { new vfx::VFXSystem(); }
  void TearDown() override { vfx::VFXSystem::Shutdown(); }
};

}  // namespace

TEST_F(VehicleWreckEffectTest, NotTriggeredInitially) {
  VehicleWreckEffect wreck(MakeWreckDesc(), nullptr, nullptr);
  EXPECT_FALSE(wreck.HasTriggered());
}

TEST_F(VehicleWreckEffectTest, OnVehicleWreckedTriggersOnce) {
  VehicleWreckEffect wreck(MakeWreckDesc(), nullptr, nullptr);

  wreck.OnVehicleWrecked({1.f, 2.f, 3.f});

  EXPECT_TRUE(wreck.HasTriggered());
}

TEST_F(VehicleWreckEffectTest, SecondCallIsIgnored) {
  VehicleWreckEffect wreck(MakeWreckDesc(), nullptr, nullptr);

  wreck.OnVehicleWrecked({1.f, 2.f, 3.f});
  EXPECT_NO_THROW(wreck.OnVehicleWrecked({4.f, 5.f, 6.f}));

  EXPECT_TRUE(wreck.HasTriggered());
}

TEST(VehicleWreckEffectNoSystemTest, TriggerWithoutVFXSystemDoesNotCrash) {
  VehicleWreckEffect wreck(MakeWreckDesc(), nullptr, nullptr);
  EXPECT_NO_THROW(wreck.OnVehicleWrecked({0.f, 0.f, 0.f}));
  EXPECT_TRUE(wreck.HasTriggered());
}

TEST(VehicleWreckEffectNoSystemTest, DestroyingUntriggeredEffectDoesNotCrash) {
  auto wreck = std::make_unique<VehicleWreckEffect>(MakeWreckDesc(), nullptr, nullptr);
  EXPECT_NO_THROW(wreck.reset());
}
