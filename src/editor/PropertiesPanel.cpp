#include "editor/PropertiesPanel.h"

#include <cmath>

#include <imgui.h>

#include "core/Color.h"
#include "core/Vec3f.h"
#include "editor/EditorUtils.h"
#include "game/GameLight.h"
#include "game/GameMesh.h"
#include "game/GameObject.h"
#include "game/GameObjectType.h"
#include "game/MeshTemplate.h"
#include "renderer/CircleSpotLight.h"
#include "renderer/GlobalLight.h"
#include "renderer/Light.h"
#include "renderer/OmniLight.h"
#include "renderer/RectangleSpotLight.h"

namespace editor {

namespace {

constexpr float kDeg2Rad = static_cast<float>(M_PI) / 180.f;
constexpr float kRad2Deg = 180.f / static_cast<float>(M_PI);

const char* LightTypeName(renderer::LightType type) {
  switch (type) {
    case renderer::LightType::kGlobal:     return "Global Light";
    case renderer::LightType::kOmni:       return "Omni Light";
    case renderer::LightType::kCircleSpot: return "Circle Spot";
    case renderer::LightType::kRectSpot:   return "Rectangle Spot";
  }
  return "Unknown";
}

// Renders Yaw/Pitch DragFloat widgets and applies changes to SetDirection().
// Returns true when the direction was modified.
template <typename TLight>
bool RenderDirectionWidgets(TLight* light) {
  auto [yaw, pitch] = Vec3fToYawPitch(light->GetDirection());
  bool changed = false;
  changed |= ImGui::DragFloat("Yaw (°)",   &yaw,   0.1f, -180.f, 180.f);
  changed |= ImGui::DragFloat("Pitch (°)", &pitch, 0.1f,  -89.f,  89.f);
  if (changed)
    light->SetDirection(YawPitchToVec3f(yaw, pitch));
  return changed;
}

// Renders common fields shared by all light types (Color, Intensity, shadows).
void RenderCommonLightFields(renderer::Light* light) {
  const core::Color& col = light->GetColor();
  float c[3] = {col.r, col.g, col.b};
  if (ImGui::ColorEdit3("Color", c))
    light->SetColor({c[0], c[1], c[2]});

  float intensity = light->GetIntensity();
  if (ImGui::DragFloat("Intensity", &intensity, 0.01f, 0.f, 100.f))
    light->SetIntensity(intensity);

  bool cast_shadow = light->GetCastShadow();
  if (ImGui::Checkbox("Cast Shadow", &cast_shadow))
    light->SetCastShadow(cast_shadow);

  // Shadow Resolution combo — options: 256, 512, 1024, 2048.
  static const int kResolutions[]   = {256, 512, 1024, 2048};
  static const char* kResLabels[]   = {"256", "512", "1024", "2048"};
  constexpr int kResCount = 4;
  const int cur_res = light->GetShadowResolution();
  int cur_idx = 2;  // default 1024
  for (int i = 0; i < kResCount; ++i) {
    if (kResolutions[i] == cur_res) {
      cur_idx = i;
      break;
    }
  }
  if (ImGui::Combo("Shadow Resolution", &cur_idx, kResLabels, kResCount))
    light->SetShadowResolution(kResolutions[cur_idx]);

  float bias = light->GetShadowBias();
  if (ImGui::DragFloat("Shadow Bias", &bias, 0.0001f, 0.f, 0.1f, "%.4f"))
    light->SetShadowBias(bias);
}

void RenderOmniFields(renderer::OmniLight* light) {
  float radius = light->GetRadius();
  if (ImGui::DragFloat("Radius", &radius, 0.1f, 0.1f, 500.f))
    light->SetRadius(radius);
}

void RenderCircleSpotFields(renderer::CircleSpotLight* light) {
  float inner_deg = light->GetInnerAngle() * kRad2Deg;
  float outer_deg = light->GetOuterAngle() * kRad2Deg;

  if (ImGui::DragFloat("Inner Angle (°)", &inner_deg, 0.1f, 0.f, 89.f))
    light->SetInnerAngle(inner_deg * kDeg2Rad);

  // Outer angle is clamped to stay strictly greater than inner.
  const float outer_min = inner_deg + 0.1f;
  if (ImGui::DragFloat("Outer Angle (°)", &outer_deg, 0.1f, outer_min, 89.f)) {
    if (outer_deg <= inner_deg) outer_deg = inner_deg + 0.1f;
    light->SetOuterAngle(outer_deg * kDeg2Rad);
  }

  float range = light->GetRange();
  if (ImGui::DragFloat("Range", &range, 0.1f, 0.1f, 1000.f))
    light->SetRange(range);

  RenderDirectionWidgets(light);
}

void RenderRectSpotFields(renderer::RectangleSpotLight* light) {
  float h_deg = light->GetHAngle() * kRad2Deg;
  float v_deg = light->GetVAngle() * kRad2Deg;

  if (ImGui::DragFloat("H Angle (°)", &h_deg, 0.1f, 0.1f, 89.f))
    light->SetHAngle(h_deg * kDeg2Rad);

  if (ImGui::DragFloat("V Angle (°)", &v_deg, 0.1f, 0.1f, 89.f))
    light->SetVAngle(v_deg * kDeg2Rad);

  float range = light->GetRange();
  if (ImGui::DragFloat("Range", &range, 0.1f, 0.1f, 1000.f))
    light->SetRange(range);

  RenderDirectionWidgets(light);
}

void RenderGlobalLightFields(renderer::GlobalLight* light) {
  RenderDirectionWidgets(light);

  const core::Vec3f& amb = light->GetAmbientColor();
  float a[3] = {amb.x, amb.y, amb.z};
  if (ImGui::ColorEdit3("Ambient", a))
    light->SetAmbientColor({a[0], a[1], a[2]});
}

}  // namespace

void PropertiesPanel::Render(game::GameObject* obj) {
  if (!obj) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextUnformatted("No object selected");
    ImGui::PopStyleColor();
    return;
  }

  switch (obj->GetType()) {
    case game::GameObjectType::kLight:
      RenderLightProperties(static_cast<game::GameLight*>(obj));
      break;
    case game::GameObjectType::kMesh:
      RenderMeshProperties(static_cast<game::GameMesh*>(obj));
      break;
    default:
      break;
  }
}

void PropertiesPanel::RenderLightProperties(const game::GameLight* game_light) {
  renderer::Light* light = game_light->GetLight();
  const renderer::LightType type = light->GetType();

  ImGui::SeparatorText(LightTypeName(type));

  RenderCommonLightFields(light);

  ImGui::Spacing();

  // Type-specific sections — irrelevant blocks are greyed out.
  const bool is_omni     = (type == renderer::LightType::kOmni);
  const bool is_circle   = (type == renderer::LightType::kCircleSpot);
  const bool is_rect     = (type == renderer::LightType::kRectSpot);
  const bool is_global   = (type == renderer::LightType::kGlobal);

  ImGui::BeginDisabled(!is_omni);
  if (is_omni) {
    RenderOmniFields(static_cast<renderer::OmniLight*>(light));
  } else {
    float dummy = 0.f;
    ImGui::DragFloat("Radius", &dummy, 0.1f, 0.1f, 500.f);
  }
  ImGui::EndDisabled();

  ImGui::BeginDisabled(!is_circle);
  if (is_circle) {
    RenderCircleSpotFields(static_cast<renderer::CircleSpotLight*>(light));
  } else {
    float dummy = 0.f;
    ImGui::DragFloat("Inner Angle (°)", &dummy, 0.1f, 0.f,  89.f);
    ImGui::DragFloat("Outer Angle (°)", &dummy, 0.1f, 0.1f, 89.f);
    ImGui::DragFloat("Range##circle",   &dummy, 0.1f, 0.1f, 1000.f);
    ImGui::DragFloat("Yaw (°)##circle", &dummy, 0.1f, -180.f, 180.f);
    ImGui::DragFloat("Pitch (°)##circle", &dummy, 0.1f, -89.f, 89.f);
  }
  ImGui::EndDisabled();

  ImGui::BeginDisabled(!is_rect);
  if (is_rect) {
    RenderRectSpotFields(static_cast<renderer::RectangleSpotLight*>(light));
  } else {
    float dummy = 0.f;
    ImGui::DragFloat("H Angle (°)", &dummy, 0.1f, 0.1f, 89.f);
    ImGui::DragFloat("V Angle (°)", &dummy, 0.1f, 0.1f, 89.f);
    ImGui::DragFloat("Range##rect",   &dummy, 0.1f, 0.1f, 1000.f);
    ImGui::DragFloat("Yaw (°)##rect",   &dummy, 0.1f, -180.f, 180.f);
    ImGui::DragFloat("Pitch (°)##rect", &dummy, 0.1f, -89.f, 89.f);
  }
  ImGui::EndDisabled();

  ImGui::BeginDisabled(!is_global);
  if (is_global) {
    RenderGlobalLightFields(static_cast<renderer::GlobalLight*>(light));
  } else {
    float dummy = 0.f;
    float dummy3[3] = {};
    ImGui::DragFloat("Yaw (°)##global",   &dummy, 0.1f, -180.f, 180.f);
    ImGui::DragFloat("Pitch (°)##global", &dummy, 0.1f, -89.f, 89.f);
    ImGui::ColorEdit3("Ambient", dummy3);
  }
  ImGui::EndDisabled();
}

void PropertiesPanel::RenderMeshProperties(const game::GameMesh* mesh) {
  ImGui::SeparatorText("Mesh");
  ImGui::LabelText("Template", "%s", mesh->GetTemplate()->GetId().c_str());
}

}  // namespace editor
