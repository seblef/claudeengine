#include "editor/EnvironmentEditorPanel.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>

#include <imgui.h>
#include <loguru.hpp>
#include <nfd.h>

#include "core/Color.h"
#include "core/Config.h"
#include "core/Vec3f.h"
#include "editor/EditorScene.h"
#include "environment/CloudRenderer.h"
#include "environment/CloudShadowRenderer.h"
#include "environment/EnvironmentDesc.h"
#include "environment/SkyRenderer.h"
#include "environment/WaterRenderer.h"
#include "game/GameLightDesc.h"
#include "game/GameObjectType.h"
#include "game/GameTerrain.h"
#include "renderer/Renderer.h"
#include "terrain/TerrainData.h"

namespace editor {

namespace {

// Sun/moon global light parameters driven by sky state.
constexpr float kSunMaxIntensity  = 1.2f;
constexpr float kMoonIntensity    = 0.08f;
// sin(45°) — maximum solar elevation for this engine's assumed latitude.
constexpr float kSinMaxElevation  = 0.7071f;

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

// Converts a 2D UV direction to an angle in degrees [0, 360).
// 0° = +X, 90° = +Y.
float UVDirToAngleDeg(float dx, float dy) {
  const float angle = std::atan2(dy, dx) * (180.f / static_cast<float>(M_PI));
  return angle < 0.f ? angle + 360.f : angle;
}

// Converts an angle in degrees back to a normalised 2D UV direction.
void AngleDegToUVDir(float deg, float* out_x, float* out_y) {
  const float rad = deg * (static_cast<float>(M_PI) / 180.f);
  *out_x = std::cos(rad);
  *out_y = std::sin(rad);
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

  // Cache wind and normal map direction angles for the UI.
  wind_angle_deg_    = DirectionToAngleDeg(env.wind_direction);
  normal_dir1_angle_ = UVDirToAngleDeg(env.normal_dir1_x, env.normal_dir1_y);
  normal_dir2_angle_ = UVDirToAngleDeg(env.normal_dir2_x, env.normal_dir2_y);

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
    UpdateGlobalLightFromSky();
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
  environment::SkyRenderer::Instance().SetMoonTexture(desc.moon_texture);
  environment::SkyRenderer::Instance().SetNightSkyTexture(desc.night_sky_texture);
  renderer::Renderer::Instance().SetSkyRenderer(
      &environment::SkyRenderer::Instance());
  renderer::Renderer::Instance().SetSkyWorldTime(world_time_->GetTimeOfDay());
  renderer::Renderer::Instance().SetWaterSkyWorldTime(world_time_->GetTimeOfDay());
  UpdateGlobalLightFromSky();
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
    float tw = 0.f;
    float th = 0.f;
    if (scene_) {
      const auto& objs = scene_->GetObjects();
      const auto  it   = std::find_if(objs.begin(), objs.end(),
          [](const game::GameObject* o) {
            return o->GetType() == game::GameObjectType::kTerrain;
          });
      if (it != objs.end()) {
        const auto* gt = static_cast<const game::GameTerrain*>(*it);
        tw = gt->GetData().GetWorldWidth();
        th = gt->GetData().GetWorldHeight();
      }
    }
    new environment::WaterRenderer();
    environment::WaterRenderer::Instance().Build(video_, desc.water_level, tw, th);
  } else {
    environment::WaterRenderer::Instance().SetWaterLevel(desc.water_level);
  }
  environment::WaterRenderer& wr = environment::WaterRenderer::Instance();
  wr.SetWaterColor(desc.water_color_r, desc.water_color_g, desc.water_color_b);
  wr.SetRoughness(desc.roughness);
  wr.SetSunIntensity(desc.sun_intensity);
  wr.SetRefractionStrength(desc.refraction_strength);
  wr.SetAbsorptionScale(desc.absorption_scale);
  wr.SetFoamParams(desc.foam_height_thresh, desc.foam_shoreline_depth,
                   desc.foam_steepness_thresh, desc.foam_speed);
  wr.SetNormalMapParams(desc.normal_scale1, desc.normal_scale2,
                        desc.normal_scroll_speed1, desc.normal_scroll_speed2);
  wr.SetNormalMapDirections(desc.normal_dir1_x, desc.normal_dir1_y,
                            desc.normal_dir2_x, desc.normal_dir2_y);
  wr.SetNormalMapTextures(desc.normal_map_texture1, desc.normal_map_texture2);
  renderer::Renderer::Instance().SetWaterRenderer(&wr);
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

  if (!environment::CloudShadowRenderer::IsInstanced()) {
    new environment::CloudShadowRenderer();
    environment::CloudShadowRenderer::Instance().Build(video_);
  }
  renderer::Renderer::Instance().SetCloudShadowRenderer(
      &environment::CloudShadowRenderer::Instance());
}

// cppcheck-suppress functionStatic
void EnvironmentEditorPanel::DisableCloud() {
  renderer::Renderer::Instance().SetCloudRenderer(nullptr);
  renderer::Renderer::Instance().SetCloudDensity(0.f);
  if (environment::CloudRenderer::IsInstanced()) {
    environment::CloudRenderer::Instance().Reset();
    environment::CloudRenderer::Shutdown();
  }
  renderer::Renderer::Instance().SetCloudShadowRenderer(nullptr);
  if (environment::CloudShadowRenderer::IsInstanced()) {
    environment::CloudShadowRenderer::Instance().Reset();
    environment::CloudShadowRenderer::Shutdown();
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

void EnvironmentEditorPanel::UpdateGlobalLightFromSky() {
  if (!world_time_ || !scene_) return;

  const bool daytime = world_time_->IsDaytime();

  // The sky vector points *toward* the celestial body (positive Y = above horizon).
  // GameLightDesc.direction is the *ray* direction (from source toward surface),
  // so negate to convert.
  const core::Vec3f sky_toward =
      daytime ? world_time_->GetSunDirection() : world_time_->GetMoonDirection();
  const core::Vec3f light_dir = core::Vec3f(-sky_toward.x, -sky_toward.y, -sky_toward.z);

  game::GameLightDesc desc = scene_->GetGlobalLightDesc();
  desc.direction = light_dir;

  if (daytime) {
    // Sun: warm white; intensity scales with elevation (0 at horizon, max at noon).
    const float elevation_factor =
        std::max(0.f, sky_toward.y) / kSinMaxElevation;
    desc.color     = core::Color(1.f, 0.95f, 0.8f);
    desc.intensity = kSunMaxIntensity * elevation_factor;
  } else {
    // Moon: cool blue-white, dim.
    desc.color     = core::Color(0.45f, 0.5f, 0.8f);
    desc.intensity = kMoonIntensity;
  }

  scene_->SetGlobalLightDesc(desc);
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
      UpdateGlobalLightFromSky();
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

  // Moon texture picker.
  const std::string moon_label =
      env.moon_texture.empty() ? "None" : env.moon_texture;
  ImGui::LabelText("Moon texture", "%s", moon_label.c_str());

  if (ImGui::Button("Pick moon texture")) {
    const auto tex_dir = core::Config::GetDataFolder() / "textures";
    nfdu8char_t* path  = nullptr;
    const nfdu8filteritem_t filters[] = {{"Image files", "png,jpg,jpeg,tga"}};
    const nfdresult_t res =
        NFD_OpenDialogU8(&path, filters, 1, tex_dir.c_str());
    if (res == NFD_OKAY) {
      const auto rel = std::filesystem::relative(
          std::filesystem::path(path), tex_dir);
      env.moon_texture = rel.generic_string();
      if (environment::SkyRenderer::IsInstanced())
        environment::SkyRenderer::Instance().SetMoonTexture(env.moon_texture);
      NFD_FreePathU8(path);
      changed = true;
    } else if (res == NFD_ERROR) {
      LOG_F(ERROR, "NFD error opening moon texture dialog");
    }
  }

  ImGui::SameLine();
  if (ImGui::Button("Clear moon texture") && !env.moon_texture.empty()) {
    env.moon_texture.clear();
    if (environment::SkyRenderer::IsInstanced())
      environment::SkyRenderer::Instance().SetMoonTexture("");
    changed = true;
  }

  // Night sky texture picker.
  const std::string night_sky_label =
      env.night_sky_texture.empty() ? "None (procedural)" : env.night_sky_texture;
  ImGui::LabelText("Night sky texture", "%s", night_sky_label.c_str());

  if (ImGui::Button("Pick night sky texture")) {
    const auto tex_dir = core::Config::GetDataFolder() / "textures";
    nfdu8char_t* path  = nullptr;
    const nfdu8filteritem_t filters[] = {{"Image files", "png,jpg,jpeg,tga"}};
    const nfdresult_t res =
        NFD_OpenDialogU8(&path, filters, 1, tex_dir.c_str());
    if (res == NFD_OKAY) {
      const auto rel = std::filesystem::relative(
          std::filesystem::path(path), tex_dir);
      env.night_sky_texture = rel.generic_string();
      if (environment::SkyRenderer::IsInstanced())
        environment::SkyRenderer::Instance().SetNightSkyTexture(env.night_sky_texture);
      NFD_FreePathU8(path);
      changed = true;
    } else if (res == NFD_ERROR) {
      LOG_F(ERROR, "NFD error opening night sky texture dialog");
    }
  }

  ImGui::SameLine();
  if (ImGui::Button("Clear night sky texture") && !env.night_sky_texture.empty()) {
    env.night_sky_texture.clear();
    if (environment::SkyRenderer::IsInstanced())
      environment::SkyRenderer::Instance().SetNightSkyTexture("");
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

  float water_color[3] = { env.water_color_r, env.water_color_g, env.water_color_b };
  if (ImGui::ColorEdit3("Water color", water_color)) {
    env.water_color_r = water_color[0];
    env.water_color_g = water_color[1];
    env.water_color_b = water_color[2];
    if (environment::WaterRenderer::IsInstanced())
      environment::WaterRenderer::Instance().SetWaterColor(
          env.water_color_r, env.water_color_g, env.water_color_b);
    changed = true;
  }

  if (ImGui::SliderFloat("Roughness", &env.roughness, 0.01f, 0.5f, "%.3f")) {
    if (environment::WaterRenderer::IsInstanced())
      environment::WaterRenderer::Instance().SetRoughness(env.roughness);
    changed = true;
  }

  if (ImGui::SliderFloat("Sun intensity", &env.sun_intensity, 0.f, 50.f, "%.1f")) {
    if (environment::WaterRenderer::IsInstanced())
      environment::WaterRenderer::Instance().SetSunIntensity(env.sun_intensity);
    changed = true;
  }

  if (ImGui::SliderFloat("Refraction strength", &env.refraction_strength,
                          0.f, 0.15f, "%.3f")) {
    if (environment::WaterRenderer::IsInstanced())
      environment::WaterRenderer::Instance().SetRefractionStrength(
          env.refraction_strength);
    changed = true;
  }

  if (ImGui::SliderFloat("Absorption scale", &env.absorption_scale,
                          0.f, 1.f, "%.2f")) {
    if (environment::WaterRenderer::IsInstanced())
      environment::WaterRenderer::Instance().SetAbsorptionScale(
          env.absorption_scale);
    changed = true;
  }

  if (ImGui::SliderFloat("Foam height thresh", &env.foam_height_thresh,
                          0.f, 2.f, "%.2f")) {
    if (environment::WaterRenderer::IsInstanced())
      environment::WaterRenderer::Instance().SetFoamParams(
          env.foam_height_thresh, env.foam_shoreline_depth,
          env.foam_steepness_thresh, env.foam_speed);
    changed = true;
  }

  if (ImGui::SliderFloat("Foam shoreline depth", &env.foam_shoreline_depth,
                          0.f, 10.f, "%.1f")) {
    if (environment::WaterRenderer::IsInstanced())
      environment::WaterRenderer::Instance().SetFoamParams(
          env.foam_height_thresh, env.foam_shoreline_depth,
          env.foam_steepness_thresh, env.foam_speed);
    changed = true;
  }

  if (ImGui::SliderFloat("Foam steepness thresh", &env.foam_steepness_thresh,
                          0.f, 1.f, "%.2f")) {
    if (environment::WaterRenderer::IsInstanced())
      environment::WaterRenderer::Instance().SetFoamParams(
          env.foam_height_thresh, env.foam_shoreline_depth,
          env.foam_steepness_thresh, env.foam_speed);
    changed = true;
  }

  if (ImGui::SliderFloat("Foam speed", &env.foam_speed, 0.f, 5.f, "%.2f")) {
    if (environment::WaterRenderer::IsInstanced())
      environment::WaterRenderer::Instance().SetFoamParams(
          env.foam_height_thresh, env.foam_shoreline_depth,
          env.foam_steepness_thresh, env.foam_speed);
    changed = true;
  }

  if (ImGui::SliderFloat("Normal scale 1", &env.normal_scale1,
                          0.005f, 0.2f, "%.3f")) {
    if (environment::WaterRenderer::IsInstanced())
      environment::WaterRenderer::Instance().SetNormalMapParams(
          env.normal_scale1, env.normal_scale2,
          env.normal_scroll_speed1, env.normal_scroll_speed2);
    changed = true;
  }

  if (ImGui::SliderFloat("Normal scale 2", &env.normal_scale2,
                          0.005f, 0.2f, "%.3f")) {
    if (environment::WaterRenderer::IsInstanced())
      environment::WaterRenderer::Instance().SetNormalMapParams(
          env.normal_scale1, env.normal_scale2,
          env.normal_scroll_speed1, env.normal_scroll_speed2);
    changed = true;
  }

  if (ImGui::SliderFloat("Normal scroll speed 1", &env.normal_scroll_speed1,
                          0.f, 2.f, "%.2f")) {
    if (environment::WaterRenderer::IsInstanced())
      environment::WaterRenderer::Instance().SetNormalMapParams(
          env.normal_scale1, env.normal_scale2,
          env.normal_scroll_speed1, env.normal_scroll_speed2);
    changed = true;
  }

  if (ImGui::SliderFloat("Normal scroll speed 2", &env.normal_scroll_speed2,
                          0.f, 2.f, "%.2f")) {
    if (environment::WaterRenderer::IsInstanced())
      environment::WaterRenderer::Instance().SetNormalMapParams(
          env.normal_scale1, env.normal_scale2,
          env.normal_scroll_speed1, env.normal_scroll_speed2);
    changed = true;
  }

  if (ImGui::SliderFloat("Normal dir 1 angle", &normal_dir1_angle_,
                          0.f, 360.f, "%.0f deg")) {
    AngleDegToUVDir(normal_dir1_angle_, &env.normal_dir1_x, &env.normal_dir1_y);
    if (environment::WaterRenderer::IsInstanced())
      environment::WaterRenderer::Instance().SetNormalMapDirections(
          env.normal_dir1_x, env.normal_dir1_y,
          env.normal_dir2_x, env.normal_dir2_y);
    changed = true;
  }

  if (ImGui::SliderFloat("Normal dir 2 angle", &normal_dir2_angle_,
                          0.f, 360.f, "%.0f deg")) {
    AngleDegToUVDir(normal_dir2_angle_, &env.normal_dir2_x, &env.normal_dir2_y);
    if (environment::WaterRenderer::IsInstanced())
      environment::WaterRenderer::Instance().SetNormalMapDirections(
          env.normal_dir1_x, env.normal_dir1_y,
          env.normal_dir2_x, env.normal_dir2_y);
    changed = true;
  }

  // Normal map texture pickers.
  const auto PickNormalMap = [&](const char* label, const char* btn_id,
                                 const char* clr_id, std::string& tex_path,
                                 bool is_layer2) -> bool {
    bool tex_changed = false;
    const std::string display = tex_path.empty() ? "None (flat fallback)" : tex_path;
    ImGui::LabelText(label, "%s", display.c_str());

    if (ImGui::Button(btn_id)) {
      const auto tex_dir = core::Config::GetDataFolder() / "textures";
      nfdu8char_t* path  = nullptr;
      const nfdu8filteritem_t filters[] = {{"Image files", "png,jpg,jpeg,tga"}};
      const nfdresult_t res =
          NFD_OpenDialogU8(&path, filters, 1, tex_dir.c_str());
      if (res == NFD_OKAY) {
        const auto rel = std::filesystem::relative(
            std::filesystem::path(path), tex_dir);
        tex_path = rel.generic_string();
        NFD_FreePathU8(path);
        tex_changed = true;
      } else if (res == NFD_ERROR) {
        LOG_F(ERROR, "NFD error opening normal map texture dialog");
      }
    }
    ImGui::SameLine();
    if (ImGui::Button(clr_id) && !tex_path.empty()) {
      tex_path.clear();
      tex_changed = true;
    }

    if (tex_changed && environment::WaterRenderer::IsInstanced()) {
      if (is_layer2) {
        environment::WaterRenderer::Instance().SetNormalMapTextures(
            env.normal_map_texture1, tex_path);
      } else {
        environment::WaterRenderer::Instance().SetNormalMapTextures(
            tex_path, env.normal_map_texture2);
      }
    }
    return tex_changed;
  };

  changed |= PickNormalMap("Normal map 1", "Pick##nm1", "Clear##nm1",
                            env.normal_map_texture1, false);
  changed |= PickNormalMap("Normal map 2", "Pick##nm2", "Clear##nm2",
                            env.normal_map_texture2, true);

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
