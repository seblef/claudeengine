#include "editor/ParticleEditorWindow.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <loguru.hpp>
#include <nfd.h>
#include <yaml-cpp/yaml.h>

#include "abstract/BufferUsage.h"
#include "abstract/TextureFormat.h"
#include "core/Color.h"
#include "core/Config.h"
#include "core/CoordinateSystem.h"
#include "core/MathUtils.h"
#include "core/ProjectionType.h"
#include "core/Vec2f.h"
#include "core/YamlUtils.h"
#include "particles/ColorStop.h"
#include "particles/EmitterShape.h"
#include "particles/ParticleAnimationMode.h"
#include "particles/ParticleBlendMode.h"
#include "particles/ParticleSystemTemplate.h"
#include "renderer/LightInfos.h"
#include "renderer/Light.h"
#include "renderer/SceneInfos.h"

namespace editor {

namespace {

constexpr float kPreviewFovY = core::kPi / 3.f;   // 60 degrees
constexpr float kPreviewNear = 0.1f;
constexpr float kPreviewFar  = 100.f;

constexpr float kOrbitSensitivity = 0.4f;
constexpr float kZoomSensitivity  = 0.5f;
constexpr float kZoomMin          = 1.f;
constexpr float kZoomMax          = 30.f;
constexpr float kDegToRad         = core::kPi / 180.f;

constexpr int kSceneInfosFloat4s =
    static_cast<int>(sizeof(renderer::SceneInfos) / 16);

const char* BlendModeLabel(particles::ParticleBlendMode mode) {
  switch (mode) {
    case particles::ParticleBlendMode::kAdditive:   return "additive";
    case particles::ParticleBlendMode::kAlphaBlend: return "alpha_blend";
    case particles::ParticleBlendMode::kGBuffer:    return "gbuffer";
  }
  return "additive";
}

const char* AnimationModeLabel(particles::ParticleAnimationMode mode) {
  switch (mode) {
    case particles::ParticleAnimationMode::kSequential: return "sequential";
    case particles::ParticleAnimationMode::kRandom:     return "random";
  }
  return "sequential";
}

const char* EmitterShapeLabel(particles::EmitterShape shape) {
  switch (shape) {
    case particles::EmitterShape::kPoint:  return "point";
    case particles::EmitterShape::kSphere: return "sphere";
    case particles::EmitterShape::kBox:    return "box";
    case particles::EmitterShape::kCone:   return "cone";
  }
  return "point";
}

const char* LightTypeLabel(renderer::LightType type) {
  switch (type) {
    case renderer::LightType::kOmni:        return "omni";
    case renderer::LightType::kCircleSpot:  return "circle_spot";
    default:                                return "omni";
  }
}

void EmitVec3(YAML::Emitter& out, const core::Vec3f& v) {
  out << YAML::Flow << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
}

void EmitColorRGBA(YAML::Emitter& out, const core::Color& c) {
  out << YAML::Flow << YAML::BeginSeq
      << c.r << c.g << c.b << c.a << YAML::EndSeq;
}

void EmitColorRGB(YAML::Emitter& out, const core::Color& c) {
  out << YAML::Flow << YAML::BeginSeq << c.r << c.g << c.b << YAML::EndSeq;
}

}  // namespace

ParticleEditorWindow::ParticleEditorWindow(abstract::VideoDevice* video)
    : video_(video),
      preview_camera_(core::ProjectionType::kPerspective,
                      core::CoordinateSystem::kRightHanded),
      preview_renderer_(std::make_unique<particles::ParticleRenderer>(video)) {
  // Offscreen color + depth RTs for the live preview.
  preview_color_rt_ = video_->CreateRenderTarget(
      kPreviewW, kPreviewH, abstract::TextureFormat::kRGBA16F);
  preview_depth_rt_ = video_->CreateRenderTarget(
      kPreviewW, kPreviewH, abstract::TextureFormat::kDepth24Stencil8);
  std::array<abstract::RenderTarget*, 1> colors = {preview_color_rt_.get()};
  preview_rtg_ = video_->CreateRenderTargetGroup(colors, preview_depth_rt_.get());

  // Scene infos constant buffer (UBO slot 2) used by the particle shader.
  scene_infos_cb_ = video_->CreateConstantBuffer(
      kSceneInfosFloat4s, kSceneInfosSlot, abstract::BufferUsage::kDynamic);

  preview_camera_.SetMinDepth(kPreviewNear);
  preview_camera_.SetMaxDepth(kPreviewFar);
  preview_camera_.SetFOV(kPreviewFovY);
  preview_camera_.SetScreenCenter(
      {static_cast<float>(kPreviewW) * 0.5f,
       static_cast<float>(kPreviewH) * 0.5f});
  UpdatePreviewCamera();
}

void ParticleEditorWindow::Open() {
  open_ = true;
}

void ParticleEditorWindow::OpenTemplate(const std::string& name) {
  open_ = true;

  particles::ParticleSystemTemplate* tmpl =
      particles::ParticleSystemTemplate::GetOrLoad(name, video_);
  if (!tmpl) {
    LOG_F(ERROR, "ParticleEditorWindow: failed to load template '%s'",
          name.c_str());
    return;
  }

  sub_systems_ = tmpl->GetSubSystems();
  lights_      = tmpl->GetLights();
  tmpl->Release();

  selected_sub_system_ = sub_systems_.empty() ? -1 : 0;
  selected_light_      = -1;
  current_path_        = core::Config::GetDataFolder() / "particles" /
                         (name + ".particles.yaml");
  unsaved_             = false;
  preview_dirty_       = true;

  LOG_F(INFO, "ParticleEditorWindow: opened template '%s'", name.c_str());
}

void ParticleEditorWindow::Render(float time, float dt) {
  if (!open_) return;

  // Rebuild preview emitters whenever any property changed.
  if (preview_dirty_) {
    RebuildPreview();
    preview_dirty_ = false;
  }

  // Advance simulation and upload to GPU before the ImGui frame.
  for (auto& emitter : preview_emitters_) {
    emitter->Update(dt);
    emitter->UploadToGPU();
  }

  UpdatePreviewCamera();

  // Render to the offscreen RT so ImGui::Image() can display it.
  if (!preview_emitters_.empty())
    RenderPreviewFrame(time);

  // --- Main window ---
  ImGui::SetNextWindowSize(ImVec2(900.f, 620.f), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Particle Editor##pe_win", &open_)) {
    ImGui::End();
    return;
  }

  RenderToolbar();
  ImGui::Separator();

  // Three-column layout: sub-system list | properties | live preview.
  constexpr ImGuiTableFlags kTableFlags =
      ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;
  if (ImGui::BeginTable("##pe_cols", 3, kTableFlags)) {
    constexpr float kListColW    = 165.f;
    constexpr float kPreviewColW = static_cast<float>(kPreviewW) + 16.f;
    ImGui::TableSetupColumn("##pe_left",
                             ImGuiTableColumnFlags_WidthFixed, kListColW);
    ImGui::TableSetupColumn("##pe_mid",
                             ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("##pe_right",
                             ImGuiTableColumnFlags_WidthFixed, kPreviewColW);

    ImGui::TableNextRow();

    ImGui::TableNextColumn();
    RenderSubSystemList();
    ImGui::Spacing();
    RenderLightsSection();

    ImGui::TableNextColumn();
    RenderSubSystemProperties();

    ImGui::TableNextColumn();
    RenderPreviewColumn(time);

    ImGui::EndTable();
  }

  ImGui::End();
}

void ParticleEditorWindow::RebuildPreview() {
  preview_emitters_.clear();

  // Copy descs into stable storage so emitter references remain valid even if
  // sub_systems_ reallocates after a future add/remove.
  preview_descs_ = sub_systems_;

  // Create fresh emitters from the stable copies.
  preview_emitters_.reserve(preview_descs_.size());
  std::transform(preview_descs_.begin(), preview_descs_.end(),
                 std::back_inserter(preview_emitters_),
                 [this](const auto& desc) {
                   return std::make_unique<particles::ParticleEmitter>(desc, video_);
                 });
}

void ParticleEditorWindow::RenderPreviewFrame(float time) {
  FillSceneInfosCB(time);
  scene_infos_cb_->Bind();

  // Populate the per-frame queue: only emitters with a texture are drawable.
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

void ParticleEditorWindow::FillSceneInfosCB(float time) {
  renderer::SceneInfos si{};
  si.view_proj       = preview_camera_.GetViewProjectionMatrix();
  si.inv_view_proj   = preview_camera_.GetViewProjectionMatrix().Inverse();
  si.inv_proj        = preview_camera_.GetProjectionMatrix().Inverse();
  si.proj            = preview_camera_.GetProjectionMatrix();
  si.view            = preview_camera_.GetViewMatrix();
  si.eye_pos         = preview_camera_.GetPosition();
  si.time            = time;
  si.inv_screen_size = {1.f / static_cast<float>(kPreviewW),
                        1.f / static_cast<float>(kPreviewH)};
  si.z_near_         = kPreviewNear;
  si.z_far_          = kPreviewFar;
  scene_infos_cb_->Fill(&si);
}

void ParticleEditorWindow::UpdatePreviewCamera() {
  const float azi_rad = orbit_azimuth_deg_   * kDegToRad;
  const float ele_rad = orbit_elevation_deg_ * kDegToRad;
  const float cos_e   = std::cos(ele_rad);

  const core::Vec3f offset{
    orbit_distance_ * cos_e * std::sin(azi_rad),
    orbit_distance_ * std::sin(ele_rad),
    orbit_distance_ * cos_e * std::cos(azi_rad),
  };

  preview_camera_.SetPosition(orbit_center_ + offset);
  preview_camera_.SetTarget(orbit_center_);
  preview_camera_.SetUp(core::Vec3f::kAxisY);
  preview_camera_.UpdateMatrices();
}

void ParticleEditorWindow::NewFile() {
  sub_systems_.clear();
  lights_.clear();
  selected_sub_system_ = -1;
  selected_light_      = -1;
  current_path_.clear();
  unsaved_       = false;
  preview_dirty_ = true;
}

void ParticleEditorWindow::LoadFromFile() {
  nfdu8char_t* out_path = nullptr;
  const nfdu8filteritem_t filter = {"Particle Effect", "particles.yaml"};
  const auto default_dir = core::Config::GetDataFolder() / "particles";
  const nfdresult_t result =
      NFD_OpenDialogU8(&out_path, &filter, 1, default_dir.c_str());
  if (result != NFD_OKAY) {
    if (result == NFD_ERROR)
      LOG_F(ERROR, "ParticleEditorWindow: NFD error opening load dialog");
    return;
  }

  std::filesystem::path path(out_path);
  NFD_FreePathU8(out_path);

  // The template basename is the stem without .particles.yaml.
  // e.g. fire.particles.yaml → stem = fire.particles → stem again = fire
  const std::string stem = path.stem().stem().string();
  particles::ParticleSystemTemplate* tmpl =
      particles::ParticleSystemTemplate::GetOrLoad(stem, video_);
  if (!tmpl) {
    LOG_F(ERROR, "ParticleEditorWindow: failed to load '%s'",
          path.string().c_str());
    return;
  }

  sub_systems_ = tmpl->GetSubSystems();
  lights_      = tmpl->GetLights();
  tmpl->Release();

  selected_sub_system_ = sub_systems_.empty() ? -1 : 0;
  selected_light_      = -1;
  current_path_        = std::move(path);
  unsaved_             = false;
  preview_dirty_       = true;

  LOG_F(INFO, "ParticleEditorWindow: loaded '%s'",
        current_path_.string().c_str());
}

void ParticleEditorWindow::Save() {
  if (current_path_.empty()) {
    SaveAs();
    return;
  }
  if (SerializeToFile(current_path_)) {
    unsaved_ = false;
    LOG_F(INFO, "ParticleEditorWindow: saved '%s'",
          current_path_.string().c_str());
  }
}

void ParticleEditorWindow::SaveAs() {
  const std::string default_name = current_path_.empty()
      ? "new_effect.particles.yaml"
      : current_path_.filename().string();

  nfdu8char_t* out_path = nullptr;
  const nfdu8filteritem_t filter = {"Particle Effect", "particles.yaml"};
  const auto default_dir = core::Config::GetDataFolder() / "particles";
  const nfdresult_t result =
      NFD_SaveDialogU8(&out_path, &filter, 1,
                       default_dir.c_str(), default_name.c_str());
  if (result != NFD_OKAY) {
    if (result == NFD_ERROR)
      LOG_F(ERROR, "ParticleEditorWindow: NFD error opening save dialog");
    return;
  }

  std::filesystem::path path(out_path);
  NFD_FreePathU8(out_path);

  if (path.extension() != ".yaml" ||
      path.stem().extension() != ".particles") {
    path += ".particles.yaml";
  }

  if (SerializeToFile(path)) {
    current_path_ = std::move(path);
    unsaved_      = false;
    LOG_F(INFO, "ParticleEditorWindow: saved '%s'",
          current_path_.string().c_str());
  }
}

bool ParticleEditorWindow::SerializeToFile(
    const std::filesystem::path& path) {
  YAML::Emitter out;
  out << YAML::BeginMap;

  out << YAML::Key << "sub_systems" << YAML::Value << YAML::BeginSeq;
  for (const auto& ss : sub_systems_) {
    out << YAML::BeginMap;
    out << YAML::Key << "name"          << YAML::Value << ss.name;
    out << YAML::Key << "blend_mode"    << YAML::Value
        << BlendModeLabel(ss.blend_mode);
    out << YAML::Key << "lit"           << YAML::Value << ss.lit;
    out << YAML::Key << "texture"       << YAML::Value << ss.texture;
    out << YAML::Key << "sprite_cols"   << YAML::Value << ss.sprite_cols;
    out << YAML::Key << "sprite_rows"   << YAML::Value << ss.sprite_rows;
    out << YAML::Key << "animation_fps" << YAML::Value << ss.animation_fps;
    out << YAML::Key << "animation_mode" << YAML::Value
        << AnimationModeLabel(ss.animation_mode);
    out << YAML::Key << "emitter_shape"  << YAML::Value
        << EmitterShapeLabel(ss.emitter_shape);
    out << YAML::Key << "emitter_radius" << YAML::Value << ss.emitter_radius;
    out << YAML::Key << "emission_rate"  << YAML::Value << ss.emission_rate;
    out << YAML::Key << "max_particles"  << YAML::Value << ss.max_particles;
    out << YAML::Key << "looping"        << YAML::Value << ss.looping;
    out << YAML::Key << "duration"       << YAML::Value << ss.duration;
    out << YAML::Key << "lifetime"       << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "min"            << YAML::Value << ss.lifetime_min;
    out << YAML::Key << "max"            << YAML::Value << ss.lifetime_max;
    out << YAML::EndMap;
    out << YAML::Key << "speed"  << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "min"    << YAML::Value << ss.speed_min;
    out << YAML::Key << "max"    << YAML::Value << ss.speed_max;
    out << YAML::EndMap;
    out << YAML::Key << "direction" << YAML::Value;
    EmitVec3(out, ss.direction);
    out << YAML::Key << "spread"  << YAML::Value << ss.spread;
    out << YAML::Key << "gravity" << YAML::Value << ss.gravity;
    out << YAML::Key << "size_start" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "min"        << YAML::Value << ss.size_start_min;
    out << YAML::Key << "max"        << YAML::Value << ss.size_start_max;
    out << YAML::EndMap;
    out << YAML::Key << "size_end" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "min"      << YAML::Value << ss.size_end_min;
    out << YAML::Key << "max"      << YAML::Value << ss.size_end_max;
    out << YAML::EndMap;
    out << YAML::Key << "color_gradient" << YAML::Value << YAML::BeginSeq;
    for (int s = 0; s < ss.color_gradient_count; ++s) {
      const particles::ColorStop& stop = ss.color_gradient[s];
      out << YAML::BeginMap;
      out << YAML::Key << "key"   << YAML::Value << stop.key;
      out << YAML::Key << "color" << YAML::Value;
      EmitColorRGBA(out, stop.color);
      out << YAML::EndMap;
    }
    out << YAML::EndSeq;
    out << YAML::Key << "rotation_start" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "min" << YAML::Value << ss.rotation_start_min;
    out << YAML::Key << "max" << YAML::Value << ss.rotation_start_max;
    out << YAML::EndMap;
    out << YAML::Key << "angular_velocity" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "min" << YAML::Value << ss.angular_velocity_min;
    out << YAML::Key << "max" << YAML::Value << ss.angular_velocity_max;
    out << YAML::EndMap;
    out << YAML::Key << "drag"                 << YAML::Value << ss.drag;
    out << YAML::Key << "turbulence_strength"  << YAML::Value << ss.turbulence_strength;
    out << YAML::Key << "turbulence_frequency" << YAML::Value << ss.turbulence_frequency;
    out << YAML::EndMap;
  }
  out << YAML::EndSeq;

  out << YAML::Key << "lights" << YAML::Value << YAML::BeginSeq;
  for (const auto& light : lights_) {
    out << YAML::BeginMap;
    out << YAML::Key << "type"      << YAML::Value << LightTypeLabel(light.type);
    out << YAML::Key << "color"     << YAML::Value;
    EmitColorRGB(out, light.color);
    out << YAML::Key << "intensity_min"     << YAML::Value << light.intensity_min;
    out << YAML::Key << "intensity_max"     << YAML::Value << light.intensity_max;
    out << YAML::Key << "radius"            << YAML::Value << light.radius;
    out << YAML::Key << "local_offset"      << YAML::Value;
    EmitVec3(out, light.local_offset);
    out << YAML::Key << "flicker_frequency" << YAML::Value << light.flicker_frequency;
    out << YAML::Key << "flicker_amplitude" << YAML::Value << light.flicker_amplitude;
    out << YAML::EndMap;
  }
  out << YAML::EndSeq;

  out << YAML::EndMap;

  std::ofstream file(path);
  if (!file) {
    LOG_F(ERROR, "ParticleEditorWindow: cannot write to '%s'",
          path.string().c_str());
    return false;
  }
  file << out.c_str();
  return true;
}

void ParticleEditorWindow::RenderToolbar() {
  if (ImGui::Button(ICON_FA_FILE " New"))
    NewFile();
  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_FOLDER_OPEN " Load"))
    LoadFromFile();
  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save"))
    Save();
  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save As"))
    SaveAs();

  ImGui::SameLine();
  ImGui::Spacing();
  ImGui::SameLine();

  const std::string display =
      current_path_.empty() ? "(unsaved)" : current_path_.filename().string();
  if (unsaved_)
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.8f, 0.2f, 1.f));
  ImGui::TextUnformatted(display.c_str());
  if (unsaved_) ImGui::PopStyleColor();
}

void ParticleEditorWindow::RenderSubSystemList() {
  ImGui::SeparatorText("Sub-systems");

  const float list_h = ImGui::GetTextLineHeightWithSpacing() * 8.f;
  if (ImGui::BeginChild("##ss_list", ImVec2(0.f, list_h),
                         ImGuiChildFlags_Borders)) {
    for (int i = 0; i < static_cast<int>(sub_systems_.size()); ++i) {
      const auto& ss = sub_systems_[i];
      const std::string label =
          (ss.name.empty() ? "(unnamed)" : ss.name) +
          "##ss" + std::to_string(i);
      if (ImGui::Selectable(label.c_str(), i == selected_sub_system_)) {
        selected_sub_system_ = i;
        selected_light_      = -1;
      }
    }
  }
  ImGui::EndChild();

  const int  n         = static_cast<int>(sub_systems_.size());
  const bool has_sel   = selected_sub_system_ >= 0 && selected_sub_system_ < n;
  const bool can_up    = has_sel && selected_sub_system_ > 0;
  const bool can_down  = has_sel && selected_sub_system_ < n - 1;

  if (ImGui::Button(ICON_FA_PLUS "##ss_add")) {
    particles::ParticleSubSystemDesc desc;
    desc.name = "sub_system_" + std::to_string(n);
    sub_systems_.push_back(desc);
    selected_sub_system_ = n;
    selected_light_      = -1;
    unsaved_ = preview_dirty_ = true;
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(!has_sel);
  if (ImGui::Button(ICON_FA_MINUS "##ss_del")) {
    sub_systems_.erase(sub_systems_.begin() + selected_sub_system_);
    selected_sub_system_ =
        std::min(selected_sub_system_, static_cast<int>(sub_systems_.size()) - 1);
    unsaved_ = preview_dirty_ = true;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(!can_up);
  if (ImGui::Button(ICON_FA_ARROW_UP "##ss_up")) {
    std::swap(sub_systems_[selected_sub_system_],
              sub_systems_[selected_sub_system_ - 1]);
    --selected_sub_system_;
    unsaved_ = preview_dirty_ = true;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(!can_down);
  if (ImGui::Button(ICON_FA_ARROW_DOWN "##ss_down")) {
    std::swap(sub_systems_[selected_sub_system_],
              sub_systems_[selected_sub_system_ + 1]);
    ++selected_sub_system_;
    unsaved_ = preview_dirty_ = true;
  }
  ImGui::EndDisabled();
}

void ParticleEditorWindow::RenderSubSystemProperties() {
  const int  n       = static_cast<int>(sub_systems_.size());
  const bool has_sel = selected_sub_system_ >= 0 && selected_sub_system_ < n;

  if (!has_sel) {
    ImGui::TextDisabled("Select a sub-system to edit its properties.");
    return;
  }

  auto& ss      = sub_systems_[selected_sub_system_];
  bool  changed = false;

  // Name
  char name_buf[256];
  std::snprintf(name_buf, sizeof(name_buf), "%s", ss.name.c_str());
  if (ImGui::InputText("Name##ss_name", name_buf, sizeof(name_buf))) {
    ss.name = name_buf;
    changed  = true;
  }

  ImGui::SeparatorText("Rendering");

  // Blend mode
  constexpr const char* kBlendItems[] = {"gbuffer", "additive", "alpha_blend"};
  int blend_idx = static_cast<int>(ss.blend_mode);
  if (ImGui::Combo("Blend mode", &blend_idx, kBlendItems, 3)) {
    ss.blend_mode = static_cast<particles::ParticleBlendMode>(blend_idx);
    changed       = true;
  }

  // Lit — only meaningful for alpha_blend
  const bool lit_enabled = ss.blend_mode == particles::ParticleBlendMode::kAlphaBlend;
  ImGui::BeginDisabled(!lit_enabled);
  if (ImGui::Checkbox("Lit", &ss.lit)) changed = true;
  ImGui::EndDisabled();

  // Texture path + browse button
  {
    constexpr int kBufSize = 512;
    char tex_buf[kBufSize];
    std::snprintf(tex_buf, sizeof(tex_buf), "%s", ss.texture.c_str());
    const float browse_w   = 70.f;
    const float spacing    = ImGui::GetStyle().ItemSpacing.x;
    const float input_w    = ImGui::GetContentRegionAvail().x - browse_w - spacing;
    ImGui::PushItemWidth(input_w);
    if (ImGui::InputText("##tex_path", tex_buf, sizeof(tex_buf))) {
      ss.texture = tex_buf;
      changed    = true;
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("Browse##tex")) {
      nfdu8char_t* path = nullptr;
      const auto fx_dir = core::Config::GetDataFolder() / "textures" / "fx";
      const nfdu8filteritem_t filters[] = {{"Image", "png,jpg,jpeg,tga"}};
      const nfdresult_t res =
          NFD_OpenDialogU8(&path, filters, 1, fx_dir.c_str());
      if (res == NFD_OKAY) {
        const auto tex_root = core::Config::GetDataFolder() / "textures";
        const auto rel = std::filesystem::relative(
            std::filesystem::path(path), tex_root);
        ss.texture = rel.generic_string();
        NFD_FreePathU8(path);
        changed = true;
      } else if (res == NFD_ERROR) {
        LOG_F(ERROR, "ParticleEditorWindow: NFD error opening texture dialog");
      }
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Texture");
  }

  ImGui::SeparatorText("Sprite sheet");

  changed |= ImGui::InputInt("Sprite cols",  &ss.sprite_cols);
  changed |= ImGui::InputInt("Sprite rows",  &ss.sprite_rows);
  changed |= ImGui::DragFloat("Anim FPS",    &ss.animation_fps, 0.5f, 0.f, 120.f);

  constexpr const char* kAnimItems[] = {"sequential", "random"};
  int anim_idx = static_cast<int>(ss.animation_mode);
  if (ImGui::Combo("Anim mode", &anim_idx, kAnimItems, 2)) {
    ss.animation_mode = static_cast<particles::ParticleAnimationMode>(anim_idx);
    changed            = true;
  }

  ImGui::SeparatorText("Emitter");

  constexpr const char* kShapeItems[] = {"point", "sphere", "box", "cone"};
  int shape_idx = static_cast<int>(ss.emitter_shape);
  if (ImGui::Combo("Shape", &shape_idx, kShapeItems, 4)) {
    ss.emitter_shape = static_cast<particles::EmitterShape>(shape_idx);
    changed           = true;
  }
  if (ss.emitter_shape != particles::EmitterShape::kPoint) {
    changed |= ImGui::DragFloat("Radius##emitter",
                                 &ss.emitter_radius, 0.01f, 0.f, 100.f);
  }
  changed |= ImGui::DragFloat("Emission rate", &ss.emission_rate,
                               1.f, 0.f, 10000.f);
  changed |= ImGui::InputInt("Max particles",  &ss.max_particles);

  if (ImGui::Checkbox("Looping", &ss.looping)) changed = true;
  ImGui::BeginDisabled(ss.looping);
  changed |= ImGui::DragFloat("Duration", &ss.duration, 0.01f, 0.f, 300.f);
  ImGui::EndDisabled();

  ImGui::SeparatorText("Particles");

  // Paired min/max DragFloat widgets displayed inline.
  {
    float lt[2] = {ss.lifetime_min, ss.lifetime_max};
    if (ImGui::DragFloat2("Lifetime (min/max)", lt, 0.01f, 0.f, 100.f)) {
      ss.lifetime_min = lt[0];
      ss.lifetime_max = lt[1];
      changed          = true;
    }
  }
  {
    float sp[2] = {ss.speed_min, ss.speed_max};
    if (ImGui::DragFloat2("Speed (min/max)", sp, 0.01f, 0.f, 500.f)) {
      ss.speed_min = sp[0];
      ss.speed_max = sp[1];
      changed       = true;
    }
  }
  {
    float ss_s[2] = {ss.size_start_min, ss.size_start_max};
    if (ImGui::DragFloat2("Size start (min/max)", ss_s, 0.001f, 0.f, 50.f)) {
      ss.size_start_min = ss_s[0];
      ss.size_start_max = ss_s[1];
      changed            = true;
    }
  }
  {
    float ss_e[2] = {ss.size_end_min, ss.size_end_max};
    if (ImGui::DragFloat2("Size end (min/max)", ss_e, 0.001f, 0.f, 50.f)) {
      ss.size_end_min = ss_e[0];
      ss.size_end_max = ss_e[1];
      changed          = true;
    }
  }

  ImGui::SeparatorText("Direction");

  {
    float dir[3] = {ss.direction.x, ss.direction.y, ss.direction.z};
    if (ImGui::DragFloat3("Direction", dir, 0.01f, -1.f, 1.f)) {
      ss.direction = {dir[0], dir[1], dir[2]};
      changed       = true;
    }
  }
  changed |= ImGui::DragFloat("Spread",  &ss.spread,  0.5f,  0.f, 180.f);
  changed |= ImGui::DragFloat("Gravity", &ss.gravity, 0.01f, -50.f, 50.f);

  ImGui::SeparatorText("Color gradient");

  for (int s = 0; s < ss.color_gradient_count; ++s) {
    particles::ColorStop& stop = ss.color_gradient[s];
    ImGui::PushID(s);

    char label[32];
    std::snprintf(label, sizeof(label), "Key##cg%d", s);
    changed |= ImGui::DragFloat(label, &stop.key, 0.01f, 0.f, 1.f);

    std::snprintf(label, sizeof(label), "Color##cg%d", s);
    float c[4] = {stop.color.r, stop.color.g, stop.color.b, stop.color.a};
    if (ImGui::ColorEdit4(label, c)) {
      stop.color = {c[0], c[1], c[2], c[3]};
      changed     = true;
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_MINUS "##cg_del")) {
      for (int j = s; j < ss.color_gradient_count - 1; ++j)
        ss.color_gradient[j] = ss.color_gradient[j + 1];
      --ss.color_gradient_count;
      changed = true;
    }
    ImGui::PopID();
  }

  if (ss.color_gradient_count < particles::ParticleSubSystemDesc::kMaxColorStops) {
    if (ImGui::Button(ICON_FA_PLUS " Add stop")) {
      const float new_key = (ss.color_gradient_count > 0)
          ? ss.color_gradient[ss.color_gradient_count - 1].key
          : 0.f;
      ss.color_gradient[ss.color_gradient_count++] = {new_key, core::Color{1.f, 1.f, 1.f, 1.f}};
      changed = true;
    }
  }

  ImGui::SeparatorText("Rotation");

  {
    float rot[2] = {ss.rotation_start_min, ss.rotation_start_max};
    if (ImGui::DragFloat2("Start angle (min/max)", rot, 1.f, -360.f, 360.f)) {
      ss.rotation_start_min = rot[0];
      ss.rotation_start_max = rot[1];
      changed                = true;
    }
  }
  {
    float av[2] = {ss.angular_velocity_min, ss.angular_velocity_max};
    if (ImGui::DragFloat2("Angular vel (min/max)", av, 1.f, -720.f, 720.f)) {
      ss.angular_velocity_min = av[0];
      ss.angular_velocity_max = av[1];
      changed                  = true;
    }
  }

  ImGui::SeparatorText("Dynamics");

  changed |= ImGui::DragFloat("Drag",                 &ss.drag,
                               0.01f, 0.f, 1.f);
  changed |= ImGui::DragFloat("Turbulence strength",  &ss.turbulence_strength,
                               0.01f, 0.f, 10.f);
  changed |= ImGui::DragFloat("Turbulence frequency", &ss.turbulence_frequency,
                               0.1f, 0.f, 20.f);

  if (changed)
    unsaved_ = preview_dirty_ = true;
}

void ParticleEditorWindow::RenderLightsSection() {
  ImGui::SeparatorText("Embedded lights");

  const float list_h = ImGui::GetTextLineHeightWithSpacing() * 5.f;
  if (ImGui::BeginChild("##lights_list", ImVec2(0.f, list_h),
                         ImGuiChildFlags_Borders)) {
    for (int i = 0; i < static_cast<int>(lights_.size()); ++i) {
      const std::string label =
          "light_" + std::to_string(i) +
          " (" + LightTypeLabel(lights_[i].type) + ")##lt" +
          std::to_string(i);
      if (ImGui::Selectable(label.c_str(), i == selected_light_)) {
        selected_light_      = i;
        selected_sub_system_ = -1;
      }
    }
  }
  ImGui::EndChild();

  const int  nl      = static_cast<int>(lights_.size());
  const bool has_sel = selected_light_ >= 0 && selected_light_ < nl;

  if (ImGui::Button(ICON_FA_PLUS "##lt_add")) {
    particles::EmbeddedLightDesc desc;
    lights_.push_back(desc);
    selected_light_      = nl;
    selected_sub_system_ = -1;
    unsaved_ = preview_dirty_ = true;
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(!has_sel);
  if (ImGui::Button(ICON_FA_MINUS "##lt_del")) {
    lights_.erase(lights_.begin() + selected_light_);
    selected_light_ =
        std::min(selected_light_, static_cast<int>(lights_.size()) - 1);
    unsaved_ = preview_dirty_ = true;
  }
  ImGui::EndDisabled();

  // Light properties (shown below the list when a light is selected).
  if (!has_sel) return;

  auto& lt      = lights_[selected_light_];
  bool  changed = false;

  ImGui::Spacing();
  ImGui::SeparatorText("Light properties");

  constexpr const char* kLightTypes[] = {"omni", "circle_spot"};
  int type_idx = (lt.type == renderer::LightType::kCircleSpot) ? 1 : 0;
  if (ImGui::Combo("Type##lt_type", &type_idx, kLightTypes, 2)) {
    lt.type = (type_idx == 0) ? renderer::LightType::kOmni
                               : renderer::LightType::kCircleSpot;
    changed  = true;
  }

  {
    float c[3] = {lt.color.r, lt.color.g, lt.color.b};
    if (ImGui::ColorEdit3("Color##lt", c)) {
      lt.color = {c[0], c[1], c[2]};
      changed   = true;
    }
  }
  changed |= ImGui::DragFloat("Intensity min##lt", &lt.intensity_min,
                               0.01f, 0.f, 100.f);
  changed |= ImGui::DragFloat("Intensity max##lt", &lt.intensity_max,
                               0.01f, 0.f, 100.f);
  changed |= ImGui::DragFloat("Radius##lt",        &lt.radius,
                               0.1f, 0.f, 500.f);

  {
    float off[3] = {lt.local_offset.x, lt.local_offset.y, lt.local_offset.z};
    if (ImGui::DragFloat3("Offset##lt", off, 0.01f, -100.f, 100.f)) {
      lt.local_offset = {off[0], off[1], off[2]};
      changed          = true;
    }
  }
  changed |= ImGui::DragFloat("Flicker freq##lt", &lt.flicker_frequency,
                               0.1f, 0.f, 50.f);
  changed |= ImGui::DragFloat("Flicker amp##lt",  &lt.flicker_amplitude,
                               0.01f, 0.f, 1.f);

  if (changed) unsaved_ = true;
}

void ParticleEditorWindow::RenderPreviewColumn(float /*time*/) {
  ImGui::SeparatorText("Live preview");

  const ImVec2 img_size(static_cast<float>(kPreviewW),
                        static_cast<float>(kPreviewH));
  const ImVec2 cursor = ImGui::GetCursorScreenPos();

  // Reserve space and capture input (prevents parent window drag).
  ImGui::InvisibleButton("##preview_area", img_size);

  if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.MouseDown[ImGuiMouseButton_Left]) {
      orbit_azimuth_deg_   -= io.MouseDelta.x * kOrbitSensitivity;
      orbit_elevation_deg_ += io.MouseDelta.y * kOrbitSensitivity;
      orbit_elevation_deg_  = std::clamp(orbit_elevation_deg_, -89.f, 89.f);
    }
    const float scroll = io.MouseWheel;
    if (scroll != 0.f) {
      orbit_distance_ -= scroll * kZoomSensitivity;
      orbit_distance_  = std::clamp(orbit_distance_, kZoomMin, kZoomMax);
    }
  }

  if (!preview_emitters_.empty()) {
    // OpenGL FBO origin is bottom-left; flip UV Y to match ImGui's top-left.
    ImGui::GetWindowDrawList()->AddImage(
        preview_color_rt_->GetNativeHandle(),
        cursor,
        ImVec2(cursor.x + img_size.x, cursor.y + img_size.y),
        ImVec2(0.f, 1.f),
        ImVec2(1.f, 0.f));
  } else {
    ImGui::GetWindowDrawList()->AddRectFilled(
        cursor,
        ImVec2(cursor.x + img_size.x, cursor.y + img_size.y),
        IM_COL32(30, 30, 30, 255));
    const char* msg = "No sub-systems";
    const ImVec2 text_size = ImGui::CalcTextSize(msg);
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(cursor.x + (img_size.x - text_size.x) * 0.5f,
               cursor.y + (img_size.y - text_size.y) * 0.5f),
        IM_COL32(128, 128, 128, 255), msg);
  }

  ImGui::Spacing();
  if (ImGui::Button(ICON_FA_ROTATE " Restart")) {
    preview_dirty_ = true;
  }

  // Show per-emitter particle counts for quick feedback.
  if (!preview_emitters_.empty()) {
    ImGui::Spacing();
    ImGui::SeparatorText("Emitters");
    for (int i = 0; i < static_cast<int>(preview_emitters_.size()); ++i) {
      const auto& e    = preview_emitters_[i];
      const auto& desc = preview_descs_[i];
      ImGui::Text("%-16s %d", desc.name.c_str(), e->GetParticleCount());
    }
  }
}

}  // namespace editor
