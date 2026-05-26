#include "editor/commands/LightPropertyCommand.h"

#include "game/GameLight.h"
#include "renderer/CircleSpotLight.h"
#include "renderer/GlobalLight.h"
#include "renderer/Light.h"
#include "renderer/OmniLight.h"
#include "renderer/RectangleSpotLight.h"

namespace editor {

bool LightSnapshot::operator==(const LightSnapshot& o) const {
  return color             == o.color
      && intensity         == o.intensity
      && cast_shadow       == o.cast_shadow
      && shadow_resolution == o.shadow_resolution
      && shadow_bias       == o.shadow_bias
      && radius            == o.radius
      && inner_angle       == o.inner_angle
      && outer_angle       == o.outer_angle
      && range             == o.range
      && direction         == o.direction
      && h_angle           == o.h_angle
      && v_angle           == o.v_angle
      && ambient_color     == o.ambient_color;
}

LightSnapshot CaptureSnapshot(const game::GameLight* game_light) {
  const renderer::Light* light = game_light->GetLight();
  LightSnapshot s;
  s.color             = light->GetColor();
  s.intensity         = light->GetIntensity();
  s.cast_shadow       = light->GetCastShadow();
  s.shadow_resolution = light->GetShadowResolution();
  s.shadow_bias       = light->GetShadowBias();

  switch (light->GetType()) {
    case renderer::LightType::kOmni: {
      const auto* omni = static_cast<const renderer::OmniLight*>(light);
      s.radius = omni->GetRadius();
      break;
    }
    case renderer::LightType::kCircleSpot: {
      const auto* spot = static_cast<const renderer::CircleSpotLight*>(light);
      s.inner_angle = spot->GetInnerAngle();
      s.outer_angle = spot->GetOuterAngle();
      s.range       = spot->GetRange();
      s.direction   = spot->GetDirection();
      break;
    }
    case renderer::LightType::kRectSpot: {
      const auto* rect =
          static_cast<const renderer::RectangleSpotLight*>(light);
      s.h_angle   = rect->GetHAngle();
      s.v_angle   = rect->GetVAngle();
      s.range     = rect->GetRange();
      s.direction = rect->GetDirection();
      break;
    }
    case renderer::LightType::kGlobal: {
      const auto* global = static_cast<const renderer::GlobalLight*>(light);
      s.direction     = global->GetDirection();
      s.ambient_color = global->GetAmbientColor();
      break;
    }
  }
  return s;
}

// cppcheck-suppress constParameterPointer
void ApplySnapshot(game::GameLight* game_light, const LightSnapshot& s) {
  renderer::Light* light = game_light->GetLight();
  light->SetColor(s.color);
  light->SetIntensity(s.intensity);
  light->SetCastShadow(s.cast_shadow);
  light->SetShadowResolution(s.shadow_resolution);
  light->SetShadowBias(s.shadow_bias);

  switch (light->GetType()) {
    case renderer::LightType::kOmni: {
      auto* omni = static_cast<renderer::OmniLight*>(light);
      omni->SetRadius(s.radius);
      break;
    }
    case renderer::LightType::kCircleSpot: {
      auto* spot = static_cast<renderer::CircleSpotLight*>(light);
      spot->SetInnerAngle(s.inner_angle);
      spot->SetOuterAngle(s.outer_angle);
      spot->SetRange(s.range);
      game_light->SetSpotDirection(s.direction);
      break;
    }
    case renderer::LightType::kRectSpot: {
      auto* rect = static_cast<renderer::RectangleSpotLight*>(light);
      rect->SetHAngle(s.h_angle);
      rect->SetVAngle(s.v_angle);
      rect->SetRange(s.range);
      game_light->SetSpotDirection(s.direction);
      break;
    }
    case renderer::LightType::kGlobal: {
      auto* global = static_cast<renderer::GlobalLight*>(light);
      global->SetDirection(s.direction);
      global->SetAmbientColor(s.ambient_color);
      break;
    }
  }
}

LightPropertyCommand::LightPropertyCommand(game::GameLight* light,
                                           LightSnapshot    before,
                                           LightSnapshot    after)
    : light_(light),
      before_(std::move(before)),
      after_(std::move(after)) {}

void LightPropertyCommand::Execute() {
  ApplySnapshot(light_, after_);
}

void LightPropertyCommand::Undo() {
  ApplySnapshot(light_, before_);
}

void LightPropertyCommand::Redo() {
  ApplySnapshot(light_, after_);
}

std::string_view LightPropertyCommand::GetDescription() const {
  return "Edit Light Property";
}

}  // namespace editor
