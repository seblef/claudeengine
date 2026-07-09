#include "editor/VFXPanel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>

#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <loguru.hpp>
#include <yaml-cpp/yaml.h>

#include "abstract/BufferUsage.h"
#include "abstract/TextureFormat.h"
#include "core/Color.h"
#include "core/CoordinateSystem.h"
#include "core/Mat4f.h"
#include "core/MathUtils.h"
#include "core/ProjectionType.h"
#include "core/Vec3f.h"
#include "core/YamlSerialiser.h"
#include "particles/ParticleSystemTemplate.h"
#include "renderer/SceneInfos.h"
#include "vfx/VFXElectricity.h"
#include "vfx/VFXExplosion.h"
#include "vfx/VFXFire.h"
#include "vfx/VFXScrape.h"

namespace editor {

namespace {

constexpr float kPreviewFovY = core::kPi / 3.f;  // 60 degrees
constexpr float kPreviewNear = 0.1f;
constexpr float kPreviewFar  = 100.f;

constexpr float kOrbitSensitivity = 0.4f;
constexpr float kZoomSensitivity  = 0.5f;
constexpr float kZoomMin          = 1.f;
constexpr float kZoomMax          = 30.f;
constexpr float kDegToRad         = core::kPi / 180.f;

constexpr int kSceneInfosSlot    = 2;
constexpr int kSceneInfosFloat4s = static_cast<int>(sizeof(renderer::SceneInfos) / 16);

// Synthetic sliding-speed sweep frequency for the scrape preview (Hz-ish).
constexpr float kScrapePhaseSpeed = 1.5f;

const char* EffectTypeLabel(vfx::VFXEffectType type) {
  switch (type) {
    case vfx::VFXEffectType::kExplosion:   return "explosion";
    case vfx::VFXEffectType::kElectricity: return "electricity";
    case vfx::VFXEffectType::kFire:        return "fire";
    case vfx::VFXEffectType::kScrape:      return "scrape";
  }
  return "explosion";
}

vfx::VFXEffectType ParseEffectType(const std::string& s) {
  if (s == "electricity") return vfx::VFXEffectType::kElectricity;
  if (s == "fire")        return vfx::VFXEffectType::kFire;
  if (s == "scrape")      return vfx::VFXEffectType::kScrape;
  return vfx::VFXEffectType::kExplosion;
}

const char* PreviewTemplateName(vfx::VFXEffectType type) {
  switch (type) {
    case vfx::VFXEffectType::kExplosion:   return vfx::VFXExplosion::kExplosionTemplateName;
    case vfx::VFXEffectType::kElectricity: return vfx::VFXElectricity::kElectricityTemplateName;
    case vfx::VFXEffectType::kFire:        return vfx::VFXFire::kFireTemplateName;
    case vfx::VFXEffectType::kScrape:      return vfx::VFXScrape::kSparkTemplateName;
  }
  return vfx::VFXExplosion::kExplosionTemplateName;
}

}  // namespace

VFXPanel::VFXPanel(std::filesystem::path path, abstract::VideoDevice* video)
    : video_(video),
      path_(std::move(path)),
      preview_renderer_(std::make_unique<particles::ParticleRenderer>(video)),
      preview_camera_(core::ProjectionType::kPerspective,
                      core::CoordinateSystem::kRightHanded) {
  preview_color_rt_ = video_->CreateRenderTarget(
      kPreviewW, kPreviewH, abstract::TextureFormat::kRGBA16F);
  preview_depth_rt_ = video_->CreateRenderTarget(
      kPreviewW, kPreviewH, abstract::TextureFormat::kDepth24Stencil8);
  std::array<abstract::RenderTarget*, 1> colors = {preview_color_rt_.get()};
  preview_rtg_ = video_->CreateRenderTargetGroup(colors, preview_depth_rt_.get());

  preview_scene_infos_cb_ = video_->CreateConstantBuffer(
      kSceneInfosFloat4s, kSceneInfosSlot, abstract::BufferUsage::kDynamic);

  preview_camera_.SetMinDepth(kPreviewNear);
  preview_camera_.SetMaxDepth(kPreviewFar);
  preview_camera_.SetFOV(kPreviewFovY);
  preview_camera_.SetScreenCenter(
      {static_cast<float>(kPreviewW) * 0.5f, static_cast<float>(kPreviewH) * 0.5f});
  UpdatePreviewCamera();

  LoadFromYaml();
  EnsurePreviewTemplateLoaded();
}

VFXPanel::~VFXPanel() {
  StopPreview();
  if (preview_template_) preview_template_->Release();
}

void VFXPanel::Draw() {
  const float dt   = ImGui::GetIO().DeltaTime;
  const float time = static_cast<float>(ImGui::GetTime());

  UpdatePreview(dt);
  UpdatePreviewCamera();
  if (!preview_emitters_.empty()) RenderPreviewFrame(time);

  DrawEffectTypeSelector();
  ImGui::Separator();

  switch (desc_.effect_type) {
    case vfx::VFXEffectType::kExplosion:   DrawExplosionParams();   break;
    case vfx::VFXEffectType::kElectricity: DrawElectricityParams(); break;
    case vfx::VFXEffectType::kFire:        DrawFireParams();        break;
    case vfx::VFXEffectType::kScrape:      DrawScrapeParams();      break;
  }

  ImGui::Separator();
  DrawSoundSection();
  ImGui::Separator();
  DrawPreviewSection();
  DrawActionsBar();
}

void VFXPanel::Save() {
  SaveToYaml();
  dirty_ = false;
}

// ---- YAML I/O ---------------------------------------------------------------

void VFXPanel::LoadFromYaml() {
  YAML::Node root;
  try {
    root = core::LoadYamlFile(path_);
  } catch (const std::exception& e) {
    LOG_F(WARNING, "VFXPanel: failed to load '%s': %s",
          path_.string().c_str(), e.what());
    return;
  }

  desc_             = vfx::VFXDesc{};
  desc_.effect_type = ParseEffectType(root["effect_type"].as<std::string>("explosion"));
  desc_.sound       = root["sound"].as<std::string>("");

  if (const YAML::Node n = root["explosion"]) {
    vfx::VFXExplosionDesc& d = desc_.explosion;
    d.particle_duration    = n["particle_duration"].as<float>(d.particle_duration);
    d.light_color          = core::ParseColor(n["light_color"], d.light_color);
    d.light_intensity      = n["light_intensity"].as<float>(d.light_intensity);
    d.light_radius         = n["light_radius"].as<float>(d.light_radius);
    d.light_lifetime       = n["light_lifetime"].as<float>(d.light_lifetime);
    d.shockwave_radius     = n["shockwave_radius"].as<float>(d.shockwave_radius);
    d.shockwave_impulse    = n["shockwave_impulse"].as<float>(d.shockwave_impulse);
    d.shake_base_magnitude = n["shake_base_magnitude"].as<float>(d.shake_base_magnitude);
    d.shake_duration       = n["shake_duration"].as<float>(d.shake_duration);
  }

  if (const YAML::Node n = root["electricity"]) {
    vfx::VFXElectricityDesc& d = desc_.electricity;
    d.arc_segments   = n["arc_segments"].as<int>(d.arc_segments);
    d.spark_rate     = n["spark_rate"].as<float>(d.spark_rate);
    d.crackle_volume = n["crackle_volume"].as<float>(d.crackle_volume);
    d.color          = core::ParseColor(n["color"], d.color);
    d.arc_length     = n["arc_length"].as<float>(d.arc_length);
    d.duration       = n["duration"].as<float>(d.duration);
  }

  if (const YAML::Node n = root["fire"])
    desc_.fire.heat_distortion = n["heat_distortion"].as<bool>(desc_.fire.heat_distortion);

  if (const YAML::Node n = root["scrape"]) {
    physics::ScrapeDesc& d = desc_.scrape;
    d.min_speed          = n["min_speed"].as<float>(d.min_speed);
    d.max_speed          = n["max_speed"].as<float>(d.max_speed);
    d.base_emission_rate = n["base_emission_rate"].as<float>(d.base_emission_rate);
    d.base_gain          = n["base_gain"].as<float>(d.base_gain);
    d.contact_grace_time = n["contact_grace_time"].as<float>(d.contact_grace_time);
    d.screech_sound      = n["screech_sound"].as<std::string>(d.screech_sound);
  }
}

void VFXPanel::SaveToYaml() {
  YAML::Emitter out;
  out << YAML::BeginMap;

  out << YAML::Key << "effect_type" << YAML::Value << EffectTypeLabel(desc_.effect_type);
  out << YAML::Key << "sound"       << YAML::Value << desc_.sound;

  {
    const vfx::VFXExplosionDesc& d = desc_.explosion;
    out << YAML::Key << "explosion" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "particle_duration" << YAML::Value << d.particle_duration;
    core::yaml::WriteColor(out, "light_color", d.light_color);
    out << YAML::Key << "light_intensity"      << YAML::Value << d.light_intensity;
    out << YAML::Key << "light_radius"         << YAML::Value << d.light_radius;
    out << YAML::Key << "light_lifetime"       << YAML::Value << d.light_lifetime;
    out << YAML::Key << "shockwave_radius"     << YAML::Value << d.shockwave_radius;
    out << YAML::Key << "shockwave_impulse"    << YAML::Value << d.shockwave_impulse;
    out << YAML::Key << "shake_base_magnitude" << YAML::Value << d.shake_base_magnitude;
    out << YAML::Key << "shake_duration"       << YAML::Value << d.shake_duration;
    out << YAML::EndMap;
  }

  {
    const vfx::VFXElectricityDesc& d = desc_.electricity;
    out << YAML::Key << "electricity" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "arc_segments"   << YAML::Value << d.arc_segments;
    out << YAML::Key << "spark_rate"     << YAML::Value << d.spark_rate;
    out << YAML::Key << "crackle_volume" << YAML::Value << d.crackle_volume;
    core::yaml::WriteColor(out, "color", d.color);
    out << YAML::Key << "arc_length" << YAML::Value << d.arc_length;
    out << YAML::Key << "duration"   << YAML::Value << d.duration;
    out << YAML::EndMap;
  }

  out << YAML::Key << "fire" << YAML::Value << YAML::BeginMap;
  out << YAML::Key << "heat_distortion" << YAML::Value << desc_.fire.heat_distortion;
  out << YAML::EndMap;

  {
    const physics::ScrapeDesc& d = desc_.scrape;
    out << YAML::Key << "scrape" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "min_speed"          << YAML::Value << d.min_speed;
    out << YAML::Key << "max_speed"          << YAML::Value << d.max_speed;
    out << YAML::Key << "base_emission_rate" << YAML::Value << d.base_emission_rate;
    out << YAML::Key << "base_gain"          << YAML::Value << d.base_gain;
    out << YAML::Key << "contact_grace_time" << YAML::Value << d.contact_grace_time;
    out << YAML::Key << "screech_sound"      << YAML::Value << d.screech_sound;
    out << YAML::EndMap;
  }

  out << YAML::EndMap;

  std::ofstream file(path_);
  if (!file) {
    LOG_F(ERROR, "VFXPanel: cannot write '%s'", path_.string().c_str());
    return;
  }
  file << out.c_str();
  LOG_F(INFO, "VFXPanel: saved '%s'", path_.string().c_str());
}

// ---- Parameter sections -------------------------------------------------------

void VFXPanel::DrawEffectTypeSelector() {
  constexpr const char* kTypeItems[] = {"Explosion", "Electricity", "Fire", "Scrape"};
  int idx = static_cast<int>(desc_.effect_type);
  if (ImGui::Combo("Effect type", &idx, kTypeItems, 4)) {
    StopPreview();
    desc_.effect_type = static_cast<vfx::VFXEffectType>(idx);
    EnsurePreviewTemplateLoaded();
    dirty_ = true;
  }
}

void VFXPanel::DrawExplosionParams() {
  vfx::VFXExplosionDesc& d = desc_.explosion;
  bool changed = false;

  ImGui::SeparatorText("Particle burst");
  changed |= ImGui::DragFloat("Particle duration", &d.particle_duration, 0.05f, 0.1f, 30.f);

  ImGui::SeparatorText("Light flash");
  {
    float c[4] = {d.light_color.r, d.light_color.g, d.light_color.b, d.light_color.a};
    if (ImGui::ColorEdit4("Light color", c)) {
      d.light_color = {c[0], c[1], c[2], c[3]};
      changed        = true;
    }
  }
  changed |= ImGui::DragFloat("Light intensity", &d.light_intensity, 0.1f, 0.f, 100.f);
  changed |= ImGui::DragFloat("Light radius",    &d.light_radius,    0.1f, 0.f, 100.f);
  changed |= ImGui::DragFloat("Light lifetime",  &d.light_lifetime,  0.01f, 0.f, 5.f);

  ImGui::SeparatorText("Shockwave");
  changed |= ImGui::DragFloat("Shockwave radius",  &d.shockwave_radius,  0.1f,  0.f, 100.f);
  changed |= ImGui::DragFloat("Shockwave impulse", &d.shockwave_impulse, 10.f,  0.f, 50000.f);

  ImGui::SeparatorText("Screen shake");
  changed |= ImGui::DragFloat("Shake magnitude", &d.shake_base_magnitude, 0.05f, 0.f, 20.f);
  changed |= ImGui::DragFloat("Shake duration",  &d.shake_duration,       0.01f, 0.f, 5.f);

  if (changed) dirty_ = true;
}

void VFXPanel::DrawElectricityParams() {
  vfx::VFXElectricityDesc& d = desc_.electricity;
  bool changed = false;

  changed |= ImGui::InputInt("Arc segments", &d.arc_segments);
  d.arc_segments = std::clamp(d.arc_segments, 1, 64);

  changed |= ImGui::DragFloat("Spark rate",     &d.spark_rate,     1.f,   0.f, 500.f);
  changed |= ImGui::DragFloat("Crackle volume", &d.crackle_volume, 0.01f, 0.f, 1.f);
  {
    float c[4] = {d.color.r, d.color.g, d.color.b, d.color.a};
    if (ImGui::ColorEdit4("Color", c)) {
      d.color = {c[0], c[1], c[2], c[3]};
      changed  = true;
    }
  }
  changed |= ImGui::DragFloat("Arc length", &d.arc_length, 0.05f, 0.1f, 50.f);
  changed |= ImGui::DragFloat("Duration",   &d.duration,   0.01f, 0.05f, 10.f);

  if (changed) dirty_ = true;
}

void VFXPanel::DrawFireParams() {
  if (ImGui::Checkbox("Heat distortion (stub)", &desc_.fire.heat_distortion))
    dirty_ = true;
  ImGui::TextDisabled(
      "Flame/smoke visuals are authored in data/particles/fire.particles.yaml.");
}

void VFXPanel::DrawScrapeParams() {
  physics::ScrapeDesc& d = desc_.scrape;
  bool changed = false;

  changed |= ImGui::DragFloat("Min speed",           &d.min_speed,          0.05f, 0.f, 50.f);
  changed |= ImGui::DragFloat("Max speed",           &d.max_speed,          0.05f, 0.f, 100.f);
  changed |= ImGui::DragFloat("Base emission rate",  &d.base_emission_rate, 1.f,   0.f, 1000.f);
  changed |= ImGui::DragFloat("Base gain",           &d.base_gain,          0.01f, 0.f, 4.f);
  changed |= ImGui::DragFloat("Contact grace time",  &d.contact_grace_time, 0.01f, 0.f, 2.f);

  {
    constexpr int kBufSize = 256;
    char buf[kBufSize];
    std::snprintf(buf, sizeof(buf), "%s", d.screech_sound.c_str());
    if (ImGui::InputText("Screech sound", buf, sizeof(buf))) {
      d.screech_sound = buf;
      changed          = true;
    }
  }

  if (changed) dirty_ = true;
}

void VFXPanel::DrawSoundSection() {
  // kScrape carries its own screech_sound field, shown above; the top-level
  // sound field would otherwise be redundant with it.
  if (desc_.effect_type == vfx::VFXEffectType::kScrape) return;

  ImGui::SeparatorText("Sound");
  ImGui::TextUnformatted(desc_.sound.empty() ? "(none)" : desc_.sound.c_str());

  ImGui::SameLine();
  if (ImGui::Button("Browse##vfx_sound")) sound_modal_.Open();

  if (const std::string chosen = sound_modal_.Render(); !chosen.empty()) {
    desc_.sound = chosen;
    dirty_       = true;
  }

  if (!desc_.sound.empty()) {
    ImGui::SameLine();
    if (ImGui::Button("Clear##vfx_sound")) {
      desc_.sound.clear();
      dirty_ = true;
    }
  }
}

void VFXPanel::DrawActionsBar() {
  ImGui::Separator();
  if (ImGui::Button("Save")) Save();
  ImGui::SameLine();
  if (ImGui::Button("Revert")) {
    StopPreview();
    LoadFromYaml();
    EnsurePreviewTemplateLoaded();
    dirty_ = false;
  }
}

// ---- Preview ------------------------------------------------------------------

void VFXPanel::EnsurePreviewTemplateLoaded() {
  const std::string name = PreviewTemplateName(desc_.effect_type);
  if (preview_template_ && preview_template_name_ == name) return;

  if (preview_template_) preview_template_->Release();
  preview_template_      = particles::ParticleSystemTemplate::GetOrLoad(name, video_);
  preview_template_name_ = name;
}

void VFXPanel::PlayPreview() {
  StopPreview();
  EnsurePreviewTemplateLoaded();

  if (!preview_template_ || preview_template_->GetSubSystems().empty()) return;

  const std::vector<particles::ParticleSubSystemDesc>& sub_systems =
      preview_template_->GetSubSystems();

  switch (desc_.effect_type) {
    case vfx::VFXEffectType::kExplosion: {
      preview_descs_   = sub_systems;
      preview_looping_ = false;
      preview_flash_   = 1.f;
      break;
    }
    case vfx::VFXEffectType::kElectricity: {
      const int n = std::max(1, desc_.electricity.arc_segments);
      preview_descs_.reserve(n);
      for (int i = 0; i < n; ++i) {
        particles::ParticleSubSystemDesc node = sub_systems[0];
        node.emission_rate = desc_.electricity.spark_rate / static_cast<float>(n);
        preview_descs_.push_back(std::move(node));
      }
      preview_looping_ = false;
      break;
    }
    case vfx::VFXEffectType::kFire: {
      preview_descs_   = sub_systems;
      preview_looping_ = true;
      break;
    }
    case vfx::VFXEffectType::kScrape: {
      particles::ParticleSubSystemDesc node = sub_systems[0];
      node.emission_rate = 0.f;  // driven live by UpdatePreview(), like VFXScrape
      preview_descs_.push_back(std::move(node));
      preview_looping_ = true;
      break;
    }
  }

  preview_emitters_.reserve(preview_descs_.size());
  std::transform(preview_descs_.begin(), preview_descs_.end(),
                 std::back_inserter(preview_emitters_),
                 [this](const auto& node_desc) {
                   return std::make_unique<particles::ParticleEmitter>(node_desc, video_);
                 });

  if (desc_.effect_type == vfx::VFXEffectType::kElectricity) {
    const int n = static_cast<int>(preview_emitters_.size());
    for (int i = 0; i < n; ++i) {
      const float t = (n == 1) ? 0.5f
                                : static_cast<float>(i) / static_cast<float>(n - 1);
      const float x = (t - 0.5f) * desc_.electricity.arc_length;
      preview_emitters_[i]->SetWorldTransform(
          core::Mat4f::Translation({x, 0.f, 0.f}));
    }
  }

  preview_playing_ = true;
  preview_elapsed_ = 0.f;
}

void VFXPanel::StopPreview() {
  preview_playing_      = false;
  preview_looping_      = false;
  preview_flash_        = 0.f;
  preview_scrape_phase_ = 0.f;
  preview_emitters_.clear();
  preview_descs_.clear();
}

void VFXPanel::UpdatePreview(float dt) {
  if (!preview_playing_) return;

  preview_elapsed_ += dt;

  if (desc_.effect_type == vfx::VFXEffectType::kScrape && !preview_descs_.empty()) {
    // Synthetic sliding contact: sweep the speed sinusoidally between 0 and
    // max_speed so spark rate/direction visibly react to the authored
    // min/max speed and base emission rate, even without a real physics
    // contact driving VFXScrape::UpdateContact() in this isolated preview.
    preview_scrape_phase_ += dt;
    const float speed =
        desc_.scrape.max_speed *
        (0.5f + 0.5f * std::sin(preview_scrape_phase_ * kScrapePhaseSpeed));
    const float span = std::max(0.001f, desc_.scrape.max_speed - desc_.scrape.min_speed);
    const float intensity = std::clamp((speed - desc_.scrape.min_speed) / span, 0.f, 1.f);

    preview_descs_[0].emission_rate = desc_.scrape.base_emission_rate * intensity;
    preview_descs_[0].direction = {
        std::sin(preview_scrape_phase_), 0.3f, std::cos(preview_scrape_phase_)};
  }

  if (desc_.effect_type == vfx::VFXEffectType::kExplosion) {
    preview_flash_ = std::max(
        0.f, 1.f - preview_elapsed_ / std::max(0.001f, desc_.explosion.light_lifetime));
  }

  for (auto& emitter : preview_emitters_) {
    emitter->Update(dt);
    emitter->UploadToGPU();
  }

  if (!preview_looping_) {
    float duration = 1.f;
    switch (desc_.effect_type) {
      case vfx::VFXEffectType::kExplosion:   duration = desc_.explosion.particle_duration; break;
      case vfx::VFXEffectType::kElectricity: duration = desc_.electricity.duration;        break;
      default: break;
    }
    if (preview_elapsed_ >= duration) StopPreview();
  }
}

void VFXPanel::RenderPreviewFrame(float time) {
  renderer::SceneInfos si{};
  si.view_proj       = preview_camera_.GetViewProjectionMatrix();
  si.inv_view_proj   = preview_camera_.GetViewProjectionMatrix().Inverse();
  si.inv_proj        = preview_camera_.GetProjectionMatrix().Inverse();
  si.proj            = preview_camera_.GetProjectionMatrix();
  si.view            = preview_camera_.GetViewMatrix();
  si.eye_pos         = preview_camera_.GetPosition();
  si.time            = time;
  si.inv_screen_size = {1.f / static_cast<float>(kPreviewW), 1.f / static_cast<float>(kPreviewH)};
  si.z_near_         = kPreviewNear;
  si.z_far_          = kPreviewFar;
  preview_scene_infos_cb_->Fill(&si);
  preview_scene_infos_cb_->Bind();

  preview_renderer_->BeginFrame();
  for (auto& emitter : preview_emitters_) {
    if (!emitter->GetDesc().texture.empty())
      preview_renderer_->EnqueueEmitter(emitter.get());
  }

  video_->SetViewport(0, 0, kPreviewW, kPreviewH);
  preview_rtg_->BindForWriting();

  video_->SetDepthTestEnabled(true);
  video_->SetDepthWriteEnabled(true);
  video_->ClearRenderTargets(core::Color::kBlack);

  preview_renderer_->RenderForwardPass(preview_camera_, nullptr);

  preview_rtg_->UnbindForWriting();
}

void VFXPanel::UpdatePreviewCamera() {
  const float azi_rad = preview_orbit_azimuth_deg_   * kDegToRad;
  const float ele_rad = preview_orbit_elevation_deg_ * kDegToRad;
  const float cos_e   = std::cos(ele_rad);

  const core::Vec3f offset{
      preview_orbit_distance_ * cos_e * std::sin(azi_rad),
      preview_orbit_distance_ * std::sin(ele_rad),
      preview_orbit_distance_ * cos_e * std::cos(azi_rad),
  };

  preview_camera_.SetPosition(offset);
  preview_camera_.SetTarget(core::Vec3f::kZero);
  preview_camera_.SetUp(core::Vec3f::kAxisY);
  preview_camera_.UpdateMatrices();
}

void VFXPanel::DrawPreviewSection() {
  ImGui::SeparatorText("Preview");

  if (!preview_playing_) {
    if (ImGui::Button(ICON_FA_PLAY " Preview")) PlayPreview();
  } else if (preview_looping_) {
    if (ImGui::Button(ICON_FA_STOP " Stop")) StopPreview();
  } else {
    ImGui::BeginDisabled();
    ImGui::Button(ICON_FA_PLAY " Playing...");
    ImGui::EndDisabled();
  }

  const ImVec2 img_size(static_cast<float>(kPreviewW), static_cast<float>(kPreviewH));
  const ImVec2 cursor = ImGui::GetCursorScreenPos();

  // Reserve space and capture input (prevents parent window drag).
  ImGui::InvisibleButton("##vfx_preview_area", img_size);

  if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.MouseDown[ImGuiMouseButton_Left]) {
      preview_orbit_azimuth_deg_   -= io.MouseDelta.x * kOrbitSensitivity;
      preview_orbit_elevation_deg_ += io.MouseDelta.y * kOrbitSensitivity;
      preview_orbit_elevation_deg_  = std::clamp(preview_orbit_elevation_deg_, -89.f, 89.f);
    }
    const float scroll = io.MouseWheel;
    if (scroll != 0.f) {
      preview_orbit_distance_ -= scroll * kZoomSensitivity;
      preview_orbit_distance_  = std::clamp(preview_orbit_distance_, kZoomMin, kZoomMax);
    }
  }

  // OpenGL FBO origin is bottom-left; flip UV Y to match ImGui's top-left.
  ImGui::GetWindowDrawList()->AddImage(
      preview_color_rt_->GetNativeHandle(),
      cursor,
      ImVec2(cursor.x + img_size.x, cursor.y + img_size.y),
      ImVec2(0.f, 1.f),
      ImVec2(1.f, 0.f));

  if (preview_flash_ > 0.f) {
    const core::Color& lc = desc_.explosion.light_color;
    const ImU32 flash_col = IM_COL32(
        static_cast<int>(std::clamp(lc.r, 0.f, 1.f) * 255.f),
        static_cast<int>(std::clamp(lc.g, 0.f, 1.f) * 255.f),
        static_cast<int>(std::clamp(lc.b, 0.f, 1.f) * 255.f),
        static_cast<int>(std::clamp(preview_flash_, 0.f, 1.f) * 180.f));
    ImGui::GetWindowDrawList()->AddRectFilled(
        cursor, ImVec2(cursor.x + img_size.x, cursor.y + img_size.y), flash_col);
  }
}

}  // namespace editor
