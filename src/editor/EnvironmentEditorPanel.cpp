#include "editor/EnvironmentEditorPanel.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>

#include "editor/EditorScene.h"
#include "environment/CloudRenderer.h"
#include "environment/EnvironmentDesc.h"
#include "environment/SkyRenderer.h"
#include "environment/WaterRenderer.h"
#include "renderer/Renderer.h"

namespace editor {

namespace {

// Converts a XZ direction vector to a compass angle in degrees [0, 360).
// 0° = +X (east), increasing clockwise toward +Z (south).
float DirectionToAngleDeg(const core::Vec3f& dir) {
  const float angle = std::atan2(dir.z, dir.x) * (180.f / static_cast<float>(M_PI));
  return angle < 0.f ? angle + 360.f : angle;
}

// Converts a compass angle in degrees back to a normalised XZ direction.
core::Vec3f AngleDegToDirection(float deg) {
  const float rad = deg * (static_cast<float>(M_PI) / 180.f);
  return core::Vec3f(std::cos(rad), 0.f, std::sin(rad));
}

}  // namespace

// ---- Destructor --------------------------------------------------------------

EnvironmentEditorPanel::~EnvironmentEditorPanel() {
  // Tear down any live subsystems owned by this panel.
  DisableSky();
  DisableWater();
  DisableCloud();
  DisableWind();
}

// ---- SetContext --------------------------------------------------------------

void EnvironmentEditorPanel::SetContext(EditorScene* scene,
                                        abstract::VideoDevice* video) {
  DisableSky();
  DisableWater();
  DisableCloud();
  DisableWind();

  scene_ = scene;
  video_ = video;

  if (!scene_) return;

  const environment::EnvironmentDesc& env = scene_->GetEnvironmentDesc();

  // Cache wind compass angle for the UI.
  wind_angle_deg_ = DirectionToAngleDeg(env.wind_direction);

  if (env.sky_enabled)   EnableSky(env);
  if (env.water_enabled) EnableWater(env);
  if (env.cloud_enabled) EnableCloud(env);
  if (env.wind_enabled)  EnableWind(env);
}

// ---- Tick -------------------------------------------------------------------

void EnvironmentEditorPanel::Tick(float dt) {
  if (time_paused_) return;
  if (world_time_) {
    world_time_->Advance(dt);
    const float tod = world_time_->GetTimeOfDay();
    renderer::Renderer::Instance().SetSkyWorldTime(tod);
    renderer::Renderer::Instance().SetWaterSkyWorldTime(tod);
  }
  if (wind_system_) wind_system_->Update(dt);
}

// ---- Subsystem lifecycle ----------------------------------------------------

void EnvironmentEditorPanel::EnableSky(const environment::EnvironmentDesc& desc) {
  world_time_ = std::make_unique<environment::WorldTime>(
      desc.time_scale, desc.start_time_of_day);
  if (!environment::SkyRenderer::IsInstanced()) {
    new environment::SkyRenderer();
    environment::SkyRenderer::Instance().Build(video_);
  }
  environment::SkyRenderer::Instance().SetTurbidity(desc.turbidity);
  renderer::Renderer::Instance().SetSkyRenderer(
      &environment::SkyRenderer::Instance());
  renderer::Renderer::Instance().SetSkyWorldTime(world_time_->GetTimeOfDay());
  renderer::Renderer::Instance().SetWaterSkyWorldTime(world_time_->GetTimeOfDay());
}

void EnvironmentEditorPanel::DisableSky() {
  world_time_.reset();
  renderer::Renderer::Instance().SetSkyRenderer(nullptr);
  renderer::Renderer::Instance().SetSkyWorldTime(12.f);
  renderer::Renderer::Instance().SetWaterSkyWorldTime(12.f);
  if (environment::SkyRenderer::IsInstanced()) {
    environment::SkyRenderer::Instance().Reset();
    environment::SkyRenderer::Shutdown();
  }
}

void EnvironmentEditorPanel::EnableWater(const environment::EnvironmentDesc& desc) {
  if (!environment::WaterRenderer::IsInstanced()) {
    new environment::WaterRenderer();
    environment::WaterRenderer::Instance().Build(video_, desc.water_level);
  } else {
    environment::WaterRenderer::Instance().SetWaterLevel(desc.water_level);
  }
  renderer::Renderer::Instance().SetWaterRenderer(
      &environment::WaterRenderer::Instance());
}

// cppcheck-suppress functionStatic
void EnvironmentEditorPanel::DisableWater() {
  renderer::Renderer::Instance().SetWaterRenderer(nullptr);
  if (environment::WaterRenderer::IsInstanced()) {
    environment::WaterRenderer::Instance().Reset();
    environment::WaterRenderer::Shutdown();
  }
}

void EnvironmentEditorPanel::EnableCloud(const environment::EnvironmentDesc& desc) {
  if (!environment::CloudRenderer::IsInstanced()) {
    new environment::CloudRenderer();
    environment::CloudRenderer::Instance().Build(video_);
  }
  renderer::Renderer::Instance().SetCloudRenderer(
      &environment::CloudRenderer::Instance());
  renderer::Renderer::Instance().SetCloudDensity(desc.cloud_density);
}

// cppcheck-suppress functionStatic
void EnvironmentEditorPanel::DisableCloud() {
  renderer::Renderer::Instance().SetCloudRenderer(nullptr);
  renderer::Renderer::Instance().SetCloudDensity(0.f);
  if (environment::CloudRenderer::IsInstanced()) {
    environment::CloudRenderer::Instance().Reset();
    environment::CloudRenderer::Shutdown();
  }
}

void EnvironmentEditorPanel::EnableWind(const environment::EnvironmentDesc& desc) {
  wind_system_ = std::make_unique<environment::WindSystem>(desc);
  renderer::Renderer::Instance().SetWindSystem(wind_system_.get());
}

void EnvironmentEditorPanel::DisableWind() {
  renderer::Renderer::Instance().SetWindSystem(nullptr);
  wind_system_.reset();
}

// ---- Render -----------------------------------------------------------------

bool EnvironmentEditorPanel::Render() {
  if (!scene_) {
    ImGui::TextDisabled("No map loaded");
    return false;
  }

  environment::EnvironmentDesc env = scene_->GetEnvironmentDesc();
  bool changed = false;

  changed |= RenderTimeSection(env);
  changed |= RenderSkySection(env);
  changed |= RenderCloudSection(env);
  changed |= RenderWindSection(env);
  changed |= RenderWaterSection(env);
  changed |= RenderTreesSection(env);

  if (changed) scene_->SetEnvironmentDesc(env);
  return changed;
}

// ---- Section: Time -----------------------------------------------------------

bool EnvironmentEditorPanel::RenderTimeSection(environment::EnvironmentDesc& env) {
  bool changed = false;
  ImGui::SeparatorText("Time");

  if (world_time_) {
    float tod = world_time_->GetTimeOfDay();
    if (ImGui::SliderFloat("Time of day", &tod, 0.f, 24.f, "%.2f h")) {
      world_time_->SetTimeOfDay(tod);
      renderer::Renderer::Instance().SetSkyWorldTime(tod);
      renderer::Renderer::Instance().SetWaterSkyWorldTime(tod);
    }
    ImGui::Checkbox("Pause time", &time_paused_);
    ImGui::SameLine();
    ImGui::TextDisabled(time_paused_ ? "(paused)" : "(running)");
  } else {
    ImGui::BeginDisabled();
    float dummy = 12.f;
    ImGui::SliderFloat("Time of day", &dummy, 0.f, 24.f, "%.2f h");
    bool dummy_pause = false;
    ImGui::Checkbox("Pause time", &dummy_pause);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("(enable sky first)");
  }

  if (ImGui::SliderFloat("Start time", &env.start_time_of_day, 0.f, 24.f, "%.2f h")) {
    changed = true;
  }
  if (ImGui::DragFloat("Time scale", &env.time_scale, 0.5f, 0.1f, 3600.f,
                        "%.1fx")) {
    if (world_time_) world_time_->SetTimeScale(env.time_scale);
    changed = true;
  }
  return changed;
}

// ---- Section: Sky ------------------------------------------------------------

bool EnvironmentEditorPanel::RenderSkySection(environment::EnvironmentDesc& env) {
  bool changed = false;
  ImGui::SeparatorText("Sky");

  if (ImGui::Checkbox("Sky enabled##sky", &env.sky_enabled)) {
    if (env.sky_enabled) {
      EnableSky(env);
    } else {
      DisableSky();
    }
    changed = true;
  }

  ImGui::BeginDisabled(!env.sky_enabled);
  if (ImGui::SliderFloat("Turbidity", &env.turbidity, 1.f, 10.f, "%.1f")) {
    if (environment::SkyRenderer::IsInstanced())
      environment::SkyRenderer::Instance().SetTurbidity(env.turbidity);
    changed = true;
  }
  ImGui::EndDisabled();
  return changed;
}

// ---- Section: Clouds ---------------------------------------------------------

bool EnvironmentEditorPanel::RenderCloudSection(
    environment::EnvironmentDesc& env) {
  bool changed = false;
  ImGui::SeparatorText("Clouds");

  if (ImGui::Checkbox("Cloud enabled##cloud", &env.cloud_enabled)) {
    if (env.cloud_enabled) {
      EnableCloud(env);
    } else {
      DisableCloud();
    }
    changed = true;
  }

  ImGui::BeginDisabled(!env.cloud_enabled);
  if (ImGui::SliderFloat("Cloud density", &env.cloud_density, 0.f, 1.f,
                          "%.2f")) {
    renderer::Renderer::Instance().SetCloudDensity(env.cloud_density);
    changed = true;
  }
  ImGui::EndDisabled();
  return changed;
}

// ---- Section: Wind -----------------------------------------------------------

bool EnvironmentEditorPanel::RenderWindSection(environment::EnvironmentDesc& env) {
  bool changed = false;
  ImGui::SeparatorText("Wind");

  if (ImGui::Checkbox("Wind enabled##wind", &env.wind_enabled)) {
    if (env.wind_enabled) {
      EnableWind(env);
    } else {
      DisableWind();
    }
    changed = true;
  }

  ImGui::BeginDisabled(!env.wind_enabled);
  if (ImGui::SliderFloat("Wind angle", &wind_angle_deg_, 0.f, 360.f, "%.0f deg")) {
    env.wind_direction = AngleDegToDirection(wind_angle_deg_);
    if (wind_system_) wind_system_->SetBaseDirection(env.wind_direction);
    changed = true;
  }
  if (ImGui::DragFloat("Wind strength", &env.wind_strength, 0.1f, 0.f, 20.f,
                        "%.1f m/s")) {
    if (wind_system_) wind_system_->SetBaseStrength(env.wind_strength);
    changed = true;
  }
  ImGui::EndDisabled();
  return changed;
}

// ---- Section: Water ----------------------------------------------------------

bool EnvironmentEditorPanel::RenderWaterSection(
    environment::EnvironmentDesc& env) {
  bool changed = false;
  ImGui::SeparatorText("Water");

  if (ImGui::Checkbox("Water enabled##water", &env.water_enabled)) {
    if (env.water_enabled) {
      EnableWater(env);
    } else {
      DisableWater();
    }
    changed = true;
  }

  ImGui::BeginDisabled(!env.water_enabled);
  if (ImGui::DragFloat("Water level", &env.water_level, 0.1f, -500.f, 500.f,
                        "%.1f m")) {
    if (environment::WaterRenderer::IsInstanced())
      environment::WaterRenderer::Instance().SetWaterLevel(env.water_level);
    changed = true;
  }
  ImGui::EndDisabled();
  return changed;
}

// ---- Section: Trees ----------------------------------------------------------

// cppcheck-suppress functionStatic
bool EnvironmentEditorPanel::RenderTreesSection(
    environment::EnvironmentDesc& env) {
  bool changed = false;
  ImGui::SeparatorText("Trees");

  if (ImGui::Checkbox("Trees enabled##trees", &env.trees_enabled)) {
    renderer::Renderer::Instance().SetTreeEnabled(env.trees_enabled);
    changed = true;
  }

  ImGui::TextDisabled("Tree layers are managed via the Objects panel.");
  return changed;
}

}  // namespace editor
