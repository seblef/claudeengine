#include "editor/VehicleEditorWindow.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <span>

#include <imgui.h>
#include <ImGuizmo.h>
#include <loguru.hpp>
#include <nfd.h>
#include <yaml-cpp/yaml.h>

#include "abstract/TextureFormat.h"
#include "core/BBox3.h"
#include "core/Color.h"
#include "core/Config.h"
#include "core/Mat4f.h"
#include "core/Vec3f.h"
#include "core/Vec4f.h"
#include "core/YamlSerialiser.h"
#include "editor/tools/PickingUtils.h"
#include "game/MeshTemplate.h"
#include "renderer/GlobalLight.h"
#include "renderer/MeshInstance.h"
#include "renderer/PreviewRenderer.h"

namespace editor {

namespace {

constexpr int   kCombinedPreviewW    = 640;
constexpr int   kCombinedPreviewH    = 480;
constexpr float kOrbitSensitivity    = 0.4f;
constexpr float kZoomStep            = 0.3f;
constexpr float kDegToRad            = 3.14159265f / 180.f;
constexpr float kPi                  = 3.14159265f;
// Max squared drag distance (px²) that still counts as a click (4 px radius).
constexpr float kClickThreshSqr      = 16.f;
// Max squared screen distance (px²) for click-to-select wheel (20 px radius).
constexpr float kWheelPickThreshSqr  = 400.f;

core::Mat4f PositionMat(const core::Vec3f& p) {
  return core::Mat4f::Translation(p);
}

// Wheel transform: 180° Y-rotation mirrors the mesh for right-side wheels
// without flipping triangle winding (det = +1), avoiding backface-culling artefacts.
core::Mat4f WheelMat(const core::Vec3f& p, bool mirrored) {
  return mirrored ? core::Mat4f::Translation(p) * core::Mat4f::RotationY(kPi)
                  : core::Mat4f::Translation(p);
}

}  // namespace

VehicleEditorWindow::VehicleEditorWindow(abstract::VideoDevice* video)
    : video_(video),
      combined_camera_(core::ProjectionType::kPerspective,
                       core::CoordinateSystem::kRightHanded) {
  combined_camera_.SetFOV(0.785398f);
  combined_camera_.SetMinDepth(0.1f);
  combined_camera_.SetMaxDepth(1000.f);
  combined_camera_.SetScreenCenter(
      {static_cast<float>(kCombinedPreviewW) * 0.5f,
       static_cast<float>(kCombinedPreviewH) * 0.5f});

  combined_rt_ = video_->CreateRenderTarget(
      kCombinedPreviewW, kCombinedPreviewH, abstract::TextureFormat::kRGBA8);
  abstract::RenderTarget* color_ptr = combined_rt_.get();
  combined_rtg_ = video_->CreateRenderTargetGroup(
      std::span<abstract::RenderTarget*>(&color_ptr, 1), nullptr);

  combined_renderer_ = std::make_unique<renderer::PreviewRenderer>(
      video_, kCombinedPreviewW, kCombinedPreviewH);

  combined_light_ = std::make_unique<renderer::GlobalLight>(
      core::Color(0.9f, 0.85f, 0.7f), 1.0f,
      core::Vec3f(0.05f, 0.05f, 0.05f),
      core::Vec3f(-1.f, -2.f, -1.f).Normalized());

  // Flat light: full-white ambient, zero directional — used when lighting is off.
  combined_flat_light_ = std::make_unique<renderer::GlobalLight>(
      core::Color(0.f, 0.f, 0.f), 0.f,
      core::Vec3f(1.f, 1.f, 1.f),
      core::Vec3f(0.f, -1.f, 0.f));
}

VehicleEditorWindow::~VehicleEditorWindow() {
  if (body_tmpl_)        body_tmpl_->Release();
  if (front_wheel_tmpl_) front_wheel_tmpl_->Release();
  if (rear_wheel_tmpl_)  rear_wheel_tmpl_->Release();
}

void VehicleEditorWindow::Open(const std::filesystem::path& path) {
  path_  = path;
  show_  = true;
  dirty_ = false;
  focused_wheel_ = 0;
  vehicle_desc_  = {};

  // Release any previously loaded templates.
  if (body_tmpl_) {
    body_tmpl_->Release();
    body_tmpl_ = nullptr;
  }
  if (front_wheel_tmpl_) {
    front_wheel_tmpl_->Release();
    front_wheel_tmpl_ = nullptr;
  }
  if (rear_wheel_tmpl_) {
    rear_wheel_tmpl_->Release();
    rear_wheel_tmpl_ = nullptr;
  }

  use_convex_hull_body_ = false;
  body_mesh_path_.clear();
  front_wheel_mesh_path_.clear();
  rear_wheel_mesh_path_.clear();
  for (auto& inst : combined_instances_) inst.reset();

  LoadFromYaml();
}

void VehicleEditorWindow::LoadFromYaml() {
  YAML::Node root;
  try {
    root = core::LoadYamlFile(path_);
  } catch (const std::exception& e) {
    LOG_F(WARNING, "VehicleEditorWindow: failed to load '%s': %s",
          path_.string().c_str(), e.what());
    return;
  }

  body_mesh_path_        = root["body_mesh"].as<std::string>("");
  front_wheel_mesh_path_ = root["front_wheel_mesh"].as<std::string>("");
  rear_wheel_mesh_path_  = root["rear_wheel_mesh"].as<std::string>("");

  if (const YAML::Node ph = root["physics"]) {
    vehicle_desc_.mass              = ph["mass"].as<float>(vehicle_desc_.mass);
    vehicle_desc_.max_engine_torque =
        ph["max_engine_torque"].as<float>(vehicle_desc_.max_engine_torque);
    vehicle_desc_.max_steer_angle   =
        ph["max_steer_angle"].as<float>(vehicle_desc_.max_steer_angle);
    vehicle_desc_.brake_torque      =
        ph["brake_torque"].as<float>(vehicle_desc_.brake_torque);
    vehicle_desc_.handbrake_torque  =
        ph["handbrake_torque"].as<float>(vehicle_desc_.handbrake_torque);
    vehicle_desc_.engine_inertia   = ph["engine_inertia"].as<float>(vehicle_desc_.engine_inertia);
    vehicle_desc_.gear_switch_time = ph["gear_switch_time"].as<float>(vehicle_desc_.gear_switch_time);
    vehicle_desc_.clutch_strength  = ph["clutch_strength"].as<float>(vehicle_desc_.clutch_strength);
    if (ph["com_offset"])
      vehicle_desc_.com_offset =
          core::ParseVec3(ph["com_offset"], vehicle_desc_.com_offset);
    use_convex_hull_body_ =
        (ph["body_shape"].as<std::string>("box") == "convex_hull");
    if (const YAML::Node susp = ph["suspension"]) {
      auto read_axle = [](physics::WheelDesc& w, const YAML::Node& n) {
        w.suspension_min_length = n["min_length"].as<float>(w.suspension_min_length);
        w.suspension_max_length = n["max_length"].as<float>(w.suspension_max_length);
        w.suspension_stiffness  = n["stiffness"].as<float>(w.suspension_stiffness);
        w.suspension_damping    = n["damping"].as<float>(w.suspension_damping);
      };
      if (susp["front"]) {
        read_axle(vehicle_desc_.front_left,  susp["front"]);
        read_axle(vehicle_desc_.front_right, susp["front"]);
      }
      if (susp["rear"]) {
        read_axle(vehicle_desc_.rear_left,  susp["rear"]);
        read_axle(vehicle_desc_.rear_right, susp["rear"]);
      }
    }
  }

  if (const YAML::Node wh = root["wheels"]) {
    auto read_wheel = [](physics::WheelDesc& w, const YAML::Node& n) {
      if (!n) return;
      if (n["position"])
        w.position = core::ParseVec3(n["position"], w.position);
      if (n["is_driven"])
        w.is_driven = n["is_driven"].as<bool>(w.is_driven);
      if (n["is_steered"])
        w.is_steered = n["is_steered"].as<bool>(w.is_steered);
    };
    read_wheel(vehicle_desc_.front_left,  wh["front_left"]);
    read_wheel(vehicle_desc_.front_right, wh["front_right"]);
    read_wheel(vehicle_desc_.rear_left,   wh["rear_left"]);
    read_wheel(vehicle_desc_.rear_right,  wh["rear_right"]);
  }

  if (!body_mesh_path_.empty())        UpdateBodyMesh(body_mesh_path_);
  if (!front_wheel_mesh_path_.empty()) UpdateFrontWheelMesh(front_wheel_mesh_path_);
  if (!rear_wheel_mesh_path_.empty())  UpdateRearWheelMesh(rear_wheel_mesh_path_);

  dirty_ = false;
}

void VehicleEditorWindow::SaveToYaml() {
  YAML::Emitter out;
  out << YAML::BeginMap;

  out << YAML::Key << "body_mesh"        << YAML::Value << body_mesh_path_;
  out << YAML::Key << "front_wheel_mesh" << YAML::Value << front_wheel_mesh_path_;
  out << YAML::Key << "rear_wheel_mesh"  << YAML::Value << rear_wheel_mesh_path_;

  out << YAML::Key << "physics" << YAML::Value << YAML::BeginMap;
  out << YAML::Key << "mass"               << YAML::Value << vehicle_desc_.mass;
  out << YAML::Key << "max_engine_torque"  << YAML::Value << vehicle_desc_.max_engine_torque;
  out << YAML::Key << "max_steer_angle"    << YAML::Value << vehicle_desc_.max_steer_angle;
  out << YAML::Key << "brake_torque"       << YAML::Value << vehicle_desc_.brake_torque;
  out << YAML::Key << "handbrake_torque"   << YAML::Value << vehicle_desc_.handbrake_torque;
  out << YAML::Key << "engine_inertia"    << YAML::Value << vehicle_desc_.engine_inertia;
  out << YAML::Key << "gear_switch_time"  << YAML::Value << vehicle_desc_.gear_switch_time;
  out << YAML::Key << "clutch_strength"   << YAML::Value << vehicle_desc_.clutch_strength;
  core::yaml::WriteVec3f(out, "com_offset", vehicle_desc_.com_offset);
  out << YAML::Key << "body_shape"
      << YAML::Value << (use_convex_hull_body_ ? "convex_hull" : "box");

  out << YAML::Key << "suspension" << YAML::Value << YAML::BeginMap;
  auto write_axle = [&out](const char* key, const physics::WheelDesc& w) {
    out << YAML::Key << key << YAML::Value << YAML::Flow << YAML::BeginMap;
    out << YAML::Key << "min_length" << YAML::Value << w.suspension_min_length;
    out << YAML::Key << "max_length" << YAML::Value << w.suspension_max_length;
    out << YAML::Key << "stiffness"  << YAML::Value << w.suspension_stiffness;
    out << YAML::Key << "damping"    << YAML::Value << w.suspension_damping;
    out << YAML::EndMap;
  };
  write_axle("front", vehicle_desc_.front_left);
  write_axle("rear",  vehicle_desc_.rear_left);
  out << YAML::EndMap;  // suspension

  out << YAML::EndMap;  // physics

  out << YAML::Key << "wheels" << YAML::Value << YAML::BeginMap;
  auto write_wheel = [&out](const char* key, const physics::WheelDesc& w) {
    out << YAML::Key << key << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "position" << YAML::Value << YAML::Flow
        << YAML::BeginSeq << w.position.x << w.position.y << w.position.z
        << YAML::EndSeq;
    out << YAML::Key << "is_driven"  << YAML::Value << w.is_driven;
    out << YAML::Key << "is_steered" << YAML::Value << w.is_steered;
    out << YAML::EndMap;
  };
  write_wheel("front_left",  vehicle_desc_.front_left);
  write_wheel("front_right", vehicle_desc_.front_right);
  write_wheel("rear_left",   vehicle_desc_.rear_left);
  write_wheel("rear_right",  vehicle_desc_.rear_right);
  out << YAML::EndMap;  // wheels

  out << YAML::EndMap;  // root

  std::ofstream file(path_);
  if (!file) {
    LOG_F(ERROR, "VehicleEditorWindow: cannot write '%s'", path_.string().c_str());
    return;
  }
  file << out.c_str();
  LOG_F(INFO, "VehicleEditorWindow: saved '%s'", path_.string().c_str());
}

void VehicleEditorWindow::Save() {
  SaveToYaml();
  dirty_ = false;
}

// ---- Mesh loading -----------------------------------------------------------

void VehicleEditorWindow::UpdateBodyMesh(const std::string& rel_path) {
  if (body_tmpl_) {
    body_tmpl_->Release();
    body_tmpl_ = nullptr;
  }
  if (rel_path.empty()) {
    combined_instances_[0].reset();
    return;
  }
  const std::string abs_path =
      (core::Config::GetDataFolder() / rel_path).string();
  body_tmpl_ = game::MeshTemplate::GetOrLoad(abs_path, video_);
  RebuildCombinedInstances();
}

void VehicleEditorWindow::UpdateFrontWheelMesh(const std::string& rel_path) {
  if (front_wheel_tmpl_) {
    front_wheel_tmpl_->Release();
    front_wheel_tmpl_ = nullptr;
  }
  if (rel_path.empty()) {
    combined_instances_[1].reset();
    combined_instances_[2].reset();
    return;
  }
  const std::string abs_path =
      (core::Config::GetDataFolder() / rel_path).string();
  front_wheel_tmpl_ = game::MeshTemplate::GetOrLoad(abs_path, video_);
  RebuildCombinedInstances();
}

void VehicleEditorWindow::UpdateRearWheelMesh(const std::string& rel_path) {
  if (rear_wheel_tmpl_) {
    rear_wheel_tmpl_->Release();
    rear_wheel_tmpl_ = nullptr;
  }
  if (rel_path.empty()) {
    combined_instances_[3].reset();
    combined_instances_[4].reset();
    return;
  }
  const std::string abs_path =
      (core::Config::GetDataFolder() / rel_path).string();
  rear_wheel_tmpl_ = game::MeshTemplate::GetOrLoad(abs_path, video_);
  RebuildCombinedInstances();
}

void VehicleEditorWindow::RebuildCombinedInstances() {
  if (body_tmpl_ && body_tmpl_->GetMesh()) {
    combined_instances_[0] = std::make_unique<renderer::MeshInstance>(
        body_tmpl_->GetMesh(), core::Mat4f::kIdentity, /*always_visible=*/true);
  } else {
    combined_instances_[0].reset();
  }

  const std::array<const physics::WheelDesc*, 4> wheels = {
      &vehicle_desc_.front_left, &vehicle_desc_.front_right,
      &vehicle_desc_.rear_left,  &vehicle_desc_.rear_right,
  };
  game::MeshTemplate* wheel_tmpls[4] = {
      front_wheel_tmpl_, front_wheel_tmpl_,
      rear_wheel_tmpl_,  rear_wheel_tmpl_,
  };
  const bool mirrored[4] = {false, true, false, true};
  for (int i = 0; i < 4; ++i) {
    if (wheel_tmpls[i] && wheel_tmpls[i]->GetMesh()) {
      combined_instances_[i + 1] = std::make_unique<renderer::MeshInstance>(
          wheel_tmpls[i]->GetMesh(),
          WheelMat(wheels[i]->position, mirrored[i]),
          /*always_visible=*/true);
    } else {
      combined_instances_[i + 1].reset();
    }
  }

  // Fit the combined camera to the union bounding box.
  core::BBox3 bbox;
  bool has_mesh = false;
  for (const auto& inst : combined_instances_) {
    if (!inst) continue;
    if (!has_mesh) {
      bbox     = inst->GetWorldBBox();
      has_mesh = true;
    } else {
      bbox << inst->GetWorldBBox();
    }
  }
  if (has_mesh) {
    combined_center_   = bbox.GetCenter();
    combined_distance_ = 1.5f * std::max(bbox.GetSize().Length(), 1.f);
  }
}

// ---- Mesh file pickers ------------------------------------------------------

void VehicleEditorWindow::PickBodyMesh() {
  nfdu8char_t* out_path = nullptr;
  const nfdu8filteritem_t filter = {"Mesh", "obj,fbx,emesh"};
  if (NFD_OpenDialogU8(&out_path, &filter, 1, nullptr) != NFD_OKAY) return;

  const std::filesystem::path abs(out_path);
  NFD_FreePathU8(out_path);
  const std::filesystem::path data_dir = core::Config::GetDataFolder();
  body_mesh_path_ = std::filesystem::relative(abs, data_dir).string();
  UpdateBodyMesh(body_mesh_path_);
  dirty_ = true;
}

void VehicleEditorWindow::PickFrontWheelMesh() {
  nfdu8char_t* out_path = nullptr;
  const nfdu8filteritem_t filter = {"Mesh", "obj,fbx,emesh"};
  if (NFD_OpenDialogU8(&out_path, &filter, 1, nullptr) != NFD_OKAY) return;

  const std::filesystem::path abs(out_path);
  NFD_FreePathU8(out_path);
  const std::filesystem::path data_dir = core::Config::GetDataFolder();
  front_wheel_mesh_path_ = std::filesystem::relative(abs, data_dir).string();
  UpdateFrontWheelMesh(front_wheel_mesh_path_);
  dirty_ = true;
}

void VehicleEditorWindow::PickRearWheelMesh() {
  nfdu8char_t* out_path = nullptr;
  const nfdu8filteritem_t filter = {"Mesh", "obj,fbx,emesh"};
  if (NFD_OpenDialogU8(&out_path, &filter, 1, nullptr) != NFD_OKAY) return;

  const std::filesystem::path abs(out_path);
  NFD_FreePathU8(out_path);
  const std::filesystem::path data_dir = core::Config::GetDataFolder();
  rear_wheel_mesh_path_ = std::filesystem::relative(abs, data_dir).string();
  UpdateRearWheelMesh(rear_wheel_mesh_path_);
  dirty_ = true;
}

// ---- Combined preview rendering ---------------------------------------------

void VehicleEditorWindow::UpdateCombinedCamera() {
  const float azi = combined_azimuth_deg_   * kDegToRad;
  const float ele = combined_elevation_deg_ * kDegToRad;
  const float ce  = std::cos(ele);
  const core::Vec3f offset{
      combined_distance_ * ce * std::sin(azi),
      combined_distance_ * std::sin(ele),
      combined_distance_ * ce * std::cos(azi),
  };
  combined_camera_.SetPosition(combined_center_ + offset);
  combined_camera_.SetTarget(combined_center_);
  combined_camera_.SetUp(core::Vec3f::kAxisY);
  combined_camera_.UpdateMatrices();
}

void VehicleEditorWindow::RenderCombined(float time) {
  UpdateCombinedCamera();

  const std::array<const physics::WheelDesc*, 4> wheels = {
      &vehicle_desc_.front_left, &vehicle_desc_.front_right,
      &vehicle_desc_.rear_left,  &vehicle_desc_.rear_right,
  };
  const bool mirrored[4] = {false, true, false, true};
  for (int i = 0; i < 4; ++i) {
    if (combined_instances_[i + 1])
      combined_instances_[i + 1]->SetWorldMatrix(
          WheelMat(wheels[i]->position, mirrored[i]));
  }

  renderer::MeshInstance* ptrs[5];
  int count = 0;
  for (const auto& inst : combined_instances_) {
    if (inst) ptrs[count++] = inst.get();
  }

  if (count > 0) {
    renderer::GlobalLight* light =
        preview_lit_ ? combined_light_.get() : combined_flat_light_.get();
    combined_renderer_->Render(time, combined_camera_,
                               std::span<renderer::MeshInstance* const>(ptrs, count),
                               light, combined_rtg_.get());
  }
}

void VehicleEditorWindow::DrawCombinedPreview(float time) {
  RenderCombined(time);

  const ImVec2 size(static_cast<float>(kCombinedPreviewW),
                    static_cast<float>(kCombinedPreviewH));
  const ImVec2 pos = ImGui::GetCursorScreenPos();

  // Dummy instead of InvisibleButton: reserves space without registering an
  // ImGui item as hovered/active, which would make ImGuizmo::CanActivate()
  // always return false and permanently disable gizmo dragging.
  ImGui::Dummy(size);

  ImGui::GetWindowDrawList()->AddImage(
      combined_rt_->GetNativeHandle(),
      pos, ImVec2(pos.x + size.x, pos.y + size.y),
      ImVec2(0.f, 1.f), ImVec2(1.f, 0.f));

  if (combined_instances_[0]) {
    // Accumulate world-space bboxes so wheel positions are included.
    core::BBox3 vehicle_bbox = combined_instances_[0]->GetWorldBBox();
    for (int i = 1; i < 5; ++i) {
      if (combined_instances_[i])
        vehicle_bbox << combined_instances_[i]->GetWorldBBox();
    }
    const core::Mat4f vp = combined_camera_.GetProjectionMatrix()
                         * combined_camera_.GetViewMatrix();
    DrawBBoxDimensionOverlay(ImGui::GetWindowDrawList(),
                              vehicle_bbox, vp, pos, size);
  }

  const ImVec2 preview_max(pos.x + size.x, pos.y + size.y);
  const bool   in_preview = ImGui::IsMouseHoveringRect(pos, preview_max);

  // Gizmo for the selected wheel.
  {
    const std::array<physics::WheelDesc*, 4> wheels = {
        &vehicle_desc_.front_left, &vehicle_desc_.front_right,
        &vehicle_desc_.rear_left,  &vehicle_desc_.rear_right,
    };
    physics::WheelDesc& wd = *wheels[focused_wheel_];

    ImGuizmo::SetRect(pos.x, pos.y, size.x, size.y);
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetOrthographic(false);

    // ImGuizmo uses row-major (row-vector) convention; our Mat4f is column-major.
    const core::Mat4f view_t = combined_camera_.GetViewMatrix().Transpose();
    const core::Mat4f proj_t = combined_camera_.GetProjectionMatrix().Transpose();
    float view_im[16], proj_im[16];
    std::memcpy(view_im, view_t.Data(), sizeof(view_im));
    std::memcpy(proj_im, proj_t.Data(), sizeof(proj_im));

    core::Mat4f model = PositionMat(wd.position);
    model = model.Transpose();
    float model_im[16];
    std::memcpy(model_im, model.Data(), sizeof(model_im));

    if (ImGuizmo::Manipulate(view_im, proj_im, ImGuizmo::TRANSLATE,
                              ImGuizmo::WORLD, model_im)) {
      core::Mat4f new_model(model_im);
      new_model = new_model.Transpose();
      const core::Vec3f new_pos = {new_model(0, 3), new_model(1, 3), new_model(2, 3)};
      wd.position = new_pos;
      const int opposite[4] = {1, 0, 3, 2};
      physics::WheelDesc* all_wheels[4] = {
          &vehicle_desc_.front_left, &vehicle_desc_.front_right,
          &vehicle_desc_.rear_left,  &vehicle_desc_.rear_right,
      };
      all_wheels[opposite[focused_wheel_]]->position = {-new_pos.x, new_pos.y, new_pos.z};
      dirty_ = true;
    }
  }

  // Orbit drag — starts when clicking inside the preview on a non-gizmo area,
  // continues until the mouse button is released (even outside the preview).
  {
    const ImGuiIO& io = ImGui::GetIO();
    if (in_preview && io.MouseClicked[ImGuiMouseButton_Left] &&
        !ImGuizmo::IsUsing() && !ImGuizmo::IsOver()) {
      orbit_drag_active_ = true;
    }
    if (!io.MouseDown[ImGuiMouseButton_Left]) orbit_drag_active_ = false;

    if (orbit_drag_active_ && !ImGuizmo::IsUsing()) {
      combined_azimuth_deg_   -= io.MouseDelta.x * kOrbitSensitivity;
      combined_elevation_deg_ += io.MouseDelta.y * kOrbitSensitivity;
      combined_elevation_deg_  = std::clamp(combined_elevation_deg_, -89.f, 89.f);
    }
  }

  // Zoom — when hovering the preview, undo the window scroll that
  // UpdateMouseWheel() already applied in NewFrame() and use the wheel for
  // zoom instead.  preview_scroll_y_ stores the scroll captured at end of
  // last frame (i.e. before UpdateMouseWheel touched it this frame).
  if (in_preview) {
    const float wheel = ImGui::GetIO().MouseWheel;
    if (wheel != 0.f) {
      ImGui::SetScrollY(preview_scroll_y_);
      combined_distance_ -= wheel * kZoomStep * combined_distance_;
      combined_distance_  = std::clamp(combined_distance_, 0.5f, 200.f);
    }
  }
  preview_scroll_y_ = ImGui::GetScrollY();

  // Click-to-select: short release inside preview without gizmo → nearest wheel.
  {
    const ImGuiIO& io = ImGui::GetIO();
    if (in_preview && io.MouseReleased[ImGuiMouseButton_Left] &&
        !ImGuizmo::IsUsing() &&
        io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] < kClickThreshSqr) {
      const ImVec2 click_pos = io.MouseClickedPos[ImGuiMouseButton_Left];
      const core::Mat4f& vp = combined_camera_.GetViewProjectionMatrix();
      const physics::WheelDesc* const wheels[4] = {
          &vehicle_desc_.front_left, &vehicle_desc_.front_right,
          &vehicle_desc_.rear_left,  &vehicle_desc_.rear_right,
      };
      float best_dist_sq = kWheelPickThreshSqr;
      int   best         = -1;
      for (int i = 0; i < 4; ++i) {
        const core::Vec3f& wp = wheels[i]->position;
        const core::Vec4f  clip =
            core::Vec4f(wp.x, wp.y, wp.z, 1.f) * vp;
        if (clip.w <= 0.f) continue;
        const float inv_w = 1.f / clip.w;
        const float sx = pos.x + (clip.x * inv_w * 0.5f + 0.5f) * size.x;
        const float sy = pos.y + (1.f - (clip.y * inv_w * 0.5f + 0.5f)) * size.y;
        const float dx  = click_pos.x - sx;
        const float dy  = click_pos.y - sy;
        const float d2  = dx * dx + dy * dy;
        if (d2 < best_dist_sq) {
          best_dist_sq = d2;
          best         = i;
        }
      }
      if (best >= 0) focused_wheel_ = best;
    }
  }
}

// ---- Draw sections ----------------------------------------------------------

void VehicleEditorWindow::DrawBodySection() {
  ImGui::SeparatorText("Body");

  ImGui::Text("Body mesh: %s",
              body_mesh_path_.empty() ? "(none)" : body_mesh_path_.c_str());
  ImGui::SameLine();
  if (ImGui::Button("Browse##body")) PickBodyMesh();
}

void VehicleEditorWindow::DrawWheelsSection() {
  ImGui::SeparatorText("Wheels");

  ImGui::Text("Front wheel: %s",
              front_wheel_mesh_path_.empty() ? "(none)"
                                             : front_wheel_mesh_path_.c_str());
  ImGui::SameLine();
  if (ImGui::Button("Browse##front_wheel")) PickFrontWheelMesh();

  ImGui::Text("Rear wheel: %s",
              rear_wheel_mesh_path_.empty() ? "(none)"
                                            : rear_wheel_mesh_path_.c_str());
  ImGui::SameLine();
  if (ImGui::Button("Browse##rear_wheel")) PickRearWheelMesh();

  ImGui::Spacing();

  // Wheel selector: radio buttons to switch the active wheel.
  ImGui::TextUnformatted("Active wheel:");
  ImGui::SameLine();
  const char* const kWheelLabels[4] = {"FL", "FR", "RL", "RR"};
  for (int i = 0; i < 4; ++i) {
    ImGui::PushID(i);
    if (ImGui::RadioButton(kWheelLabels[i], focused_wheel_ == i))
      focused_wheel_ = i;
    ImGui::PopID();
    if (i < 3) ImGui::SameLine();
  }

  ImGui::SameLine(0.f, 20.f);
  ImGui::Checkbox("Lit", &preview_lit_);

  ImGui::Spacing();

  DrawCombinedPreview(static_cast<float>(ImGui::GetTime()));

  ImGui::Spacing();

  physics::WheelDesc* wheels[4] = {
      &vehicle_desc_.front_left, &vehicle_desc_.front_right,
      &vehicle_desc_.rear_left,  &vehicle_desc_.rear_right,
  };

  for (int i = 0; i < 4; ++i) {
    ImGui::PushID(i);
    const bool active = (focused_wheel_ == i);
    if (active) ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.3f, 0.3f, 0.6f, 1.f));

    ImGui::Text("%s", kWheelLabels[i]);
    ImGui::SameLine();

    float pos[3] = {wheels[i]->position.x, wheels[i]->position.y, wheels[i]->position.z};
    if (ImGui::DragFloat3("##pos", pos, 0.01f)) {
      wheels[i]->position = {pos[0], pos[1], pos[2]};
      const int opposite[4] = {1, 0, 3, 2};
      wheels[opposite[i]]->position = {-pos[0], pos[1], pos[2]};
      dirty_ = true;
    }
    if (ImGui::IsItemFocused() || ImGui::IsItemActive()) focused_wheel_ = i;

    ImGui::SameLine();
    if (ImGui::Checkbox("Driven##drv", &wheels[i]->is_driven))  dirty_ = true;
    ImGui::SameLine();
    if (ImGui::Checkbox("Steered##str", &wheels[i]->is_steered)) dirty_ = true;

    if (active) ImGui::PopStyleColor();
    ImGui::PopID();
  }
}

void VehicleEditorWindow::DrawPhysicsSection() {
  ImGui::SeparatorText("Physics");

  dirty_ |= ImGui::DragFloat("Mass (kg)",              &vehicle_desc_.mass,            10.f, 100.f,  5000.f,  "%.0f");
  dirty_ |= ImGui::DragFloat("Max engine torque (Nm)", &vehicle_desc_.max_engine_torque, 5.f, 50.f, 2000.f, "%.0f");

  float steer_deg = vehicle_desc_.max_steer_angle / kDegToRad;
  if (ImGui::SliderFloat("Max steer angle (deg)", &steer_deg, 5.f, 60.f, "%.1f")) {
    vehicle_desc_.max_steer_angle = steer_deg * kDegToRad;
    dirty_ = true;
  }

  dirty_ |= ImGui::DragFloat("Brake torque (Nm)",     &vehicle_desc_.brake_torque,    10.f, 100.f,  5000.f,  "%.0f");
  dirty_ |= ImGui::DragFloat("Handbrake torque (Nm)", &vehicle_desc_.handbrake_torque, 10.f, 100.f, 10000.f, "%.0f");

  float com[3] = {vehicle_desc_.com_offset.x,
                  vehicle_desc_.com_offset.y,
                  vehicle_desc_.com_offset.z};
  if (ImGui::DragFloat3("COM offset", com, 0.01f)) {
    vehicle_desc_.com_offset = {com[0], com[1], com[2]};
    dirty_ = true;
  }

  if (ImGui::Checkbox("Convex hull body", &use_convex_hull_body_))
    dirty_ = true;

  ImGui::Spacing();
  ImGui::TextUnformatted("Front suspension");
  auto draw_susp = [&](physics::WheelDesc& l, physics::WheelDesc& r,
                        const char* id) {
    ImGui::PushID(id);
    bool changed = false;
    changed |= ImGui::DragFloat("Min length (m)",  &l.suspension_min_length, 0.01f, 0.01f, 1.f, "%.3f");
    changed |= ImGui::DragFloat("Max length (m)",  &l.suspension_max_length, 0.01f, 0.01f, 2.f, "%.3f");
    changed |= ImGui::DragFloat("Stiffness (N/m)", &l.suspension_stiffness,  10.f, 100.f, 10000.f, "%.0f");
    changed |= ImGui::DragFloat("Damping (N·s/m)", &l.suspension_damping,    5.f,  10.f,  2000.f, "%.0f");
    if (changed) {
      r.suspension_min_length = l.suspension_min_length;
      r.suspension_max_length = l.suspension_max_length;
      r.suspension_stiffness  = l.suspension_stiffness;
      r.suspension_damping    = l.suspension_damping;
      dirty_ = true;
    }
    ImGui::PopID();
  };
  draw_susp(vehicle_desc_.front_left, vehicle_desc_.front_right, "front_susp");

  ImGui::TextUnformatted("Rear suspension");
  draw_susp(vehicle_desc_.rear_left, vehicle_desc_.rear_right, "rear_susp");

  ImGui::SeparatorText("Transmission");
  dirty_ |= ImGui::DragFloat(
      "Engine inertia (kg\xc2\xb7m\xc2\xb2)", &vehicle_desc_.engine_inertia, 0.01f, 0.01f, 5.f, "%.3f");
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Lower = engine revs faster. Jolt default: 0.5");
  dirty_ |= ImGui::DragFloat(
      "Gear switch time (s)", &vehicle_desc_.gear_switch_time, 0.01f, 0.f, 2.f, "%.2f");
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip(
        "Seconds with clutch disengaged per gear change. 0 = instant. Jolt default: 0.5");
  dirty_ |= ImGui::DragFloat(
      "Clutch strength", &vehicle_desc_.clutch_strength, 1.f, 1.f, 200.f, "%.0f");
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Power delivery snap on clutch re-engagement. Jolt default: 10");
}

void VehicleEditorWindow::DrawActionsBar() {
  ImGui::Separator();

  if (ImGui::Button("Save")) Save();

  ImGui::SameLine();
  if (ImGui::Button("Revert")) {
    LoadFromYaml();
    dirty_ = false;
  }

  ImGui::SameLine();
  const bool can_place = static_cast<bool>(on_place_in_scene_);
  if (!can_place) ImGui::BeginDisabled();
  if (ImGui::Button("Place in Scene") && on_place_in_scene_)
    on_place_in_scene_(path_);
  if (!can_place) ImGui::EndDisabled();
}

// ---- Window rendering -------------------------------------------------------

void VehicleEditorWindow::RenderDirtyCloseModal() {
  if (!ImGui::BeginPopupModal("Unsaved Changes##vehicle_editor", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }

  ImGui::TextUnformatted("You have unsaved changes. What would you like to do?");
  ImGui::Spacing();

  if (ImGui::Button("Save")) {
    Save();
    show_ = false;
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (ImGui::Button("Discard")) {
    dirty_ = false;
    show_  = false;
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (ImGui::Button("Cancel")) {
    ImGui::CloseCurrentPopup();
  }

  ImGui::EndPopup();
}

void VehicleEditorWindow::Render() {
  if (!show_) return;

  if (open_dirty_modal_) {
    ImGui::OpenPopup("Unsaved Changes##vehicle_editor");
    open_dirty_modal_ = false;
  }
  RenderDirtyCloseModal();

  const std::string title =
      "Vehicle Editor \xe2\x80\x94 " + path_.filename().string() +
      (dirty_ ? " *" : "") + "##vehicle_editor";

  bool window_open = true;
  if (ImGui::Begin(title.c_str(), &window_open,
                   ImGuiWindowFlags_HorizontalScrollbar)) {
    DrawBodySection();
    ImGui::Spacing();
    DrawWheelsSection();
    ImGui::Spacing();
    DrawPhysicsSection();
    DrawActionsBar();
  }
  ImGui::End();

  if (!window_open) {
    if (dirty_) {
      open_dirty_modal_ = true;
    } else {
      show_ = false;
    }
  }
}

}  // namespace editor
