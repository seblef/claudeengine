#include "vfx/VFXElectricity.h"

#include <memory>

#include <gtest/gtest.h>

#include "core/Vec3f.h"
#include "vfx/VFXElectricityDesc.h"

using core::Vec3f;
using vfx::VFXElectricity;
using vfx::VFXElectricityDesc;

TEST(VFXElectricityTest, FinishedInitially) {
  VFXElectricity arc(VFXElectricityDesc{}, nullptr);
  EXPECT_TRUE(arc.IsFinished());
}

TEST(VFXElectricityTest, PlayClearsFinished) {
  VFXElectricity arc(VFXElectricityDesc{}, nullptr);
  arc.Play({0.f, 0.f, 0.f}, {1.f, 0.f, 0.f});
  EXPECT_FALSE(arc.IsFinished());
}

TEST(VFXElectricityTest, PlayWithNoTemplateDoesNotCrash) {
  VFXElectricity arc(VFXElectricityDesc{}, nullptr);
  EXPECT_NO_THROW(arc.Play({0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}));
  EXPECT_NO_THROW(arc.Update(0.016f));
}

TEST(VFXElectricityTest, SelfExpiresAfterDuration) {
  VFXElectricityDesc desc;
  desc.duration = 1.f;
  VFXElectricity arc(desc, nullptr);
  arc.Play({0.f, 0.f, 0.f}, {1.f, 0.f, 0.f});

  arc.Update(0.5f);
  EXPECT_FALSE(arc.IsFinished());

  arc.Update(0.6f);
  EXPECT_TRUE(arc.IsFinished());
}

TEST(VFXElectricityTest, PlayWithZeroLengthDirectionDoesNotCrash) {
  VFXElectricity arc(VFXElectricityDesc{}, nullptr);
  EXPECT_NO_THROW(arc.Play({0.f, 0.f, 0.f}, Vec3f::kZero));
}

TEST(VFXElectricityTest, DestroyingActiveEffectDoesNotCrash) {
  auto arc = std::make_unique<VFXElectricity>(VFXElectricityDesc{}, nullptr);
  arc->Play({0.f, 0.f, 0.f}, {1.f, 0.f, 0.f});
  EXPECT_NO_THROW(arc.reset());
}
