#include "editor/PropertiesPanel.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>

#include <imgui.h>
#include <ImGuizmo.h>

#include "core/BBox3.h"
#include "core/Color.h"
#include "core/Mat4f.h"
#include "core/Vec3f.h"
#include "editor/EditorCommandHistory.h"
#include "editor/EditorUtils.h"
#include "editor/commands/LightPropertyCommand.h"
#include "editor/commands/PhysicsPropertyCommand.h"
#include "editor/commands/RenameObjectCommand.h"
#include "editor/commands/TransformCommand.h"
#include "game/GameLight.h"
#include "game/GameMesh.h"
#include "game/GameObject.h"
#include "game/GameObjectType.h"
#include "game/GameParticleSystem.h"
#include "game/GameSoundEmitter.h"
#include "game/MeshTemplate.h"
#include "particles/ParticleSystemTemplate.h"
#include "physics/CollisionLayer.h"
#include "physics/MotionType.h"
#include "physics/PhysicsBodyDesc.h"
#include "physics/PhysicsShapeType.h"
#include "renderer/CircleSpotLight.h"
#include "renderer/GlobalLight.h"
#include "renderer/Light.h"
#include "renderer/OmniLight.h"
#include "renderer/RectangleSpotLight.h"

namespace editor {

namespace {

constexpr float kDeg2Rad = static_cast<float>(M_PI) / 180.f;
constexpr float kRad2Deg = 180.f / static_cast<float>(M_PI);

// Derives a PhysicsShapeDesc for a basic shape type from a local bounding box.
// ConvexHull / Exact / Terrain carry no size parameters and are not handled here.
// Cylinder and Capsule are assumed to be upright (Y axis).
physics::PhysicsShapeDesc ShapeDescFromBBox(const core::BBox3& bbox,
                                            physics::PhysicsShapeType type) {
  const core::Vec3f half   = bbox.GetSize() * 0.5f;
  const core::Vec3f center = bbox.GetCenter();
  physics::PhysicsShapeDesc desc;
  switch (type) {
    case physics::PhysicsShapeType::Box:
      desc = physics::PhysicsShapeDesc::MakeBox(half);
      break;
    case physics::PhysicsShapeType::Sphere:
      desc = physics::PhysicsShapeDesc::MakeSphere(
          std::max({half.x, half.y, half.z}));
      break;
    case physics::PhysicsShapeType::Cylinder: {
      const float r = std::max(half.x, half.z);
      desc = physics::PhysicsShapeDesc::MakeCylinder(r, std::max(half.y, 0.001f));
      break;
    }
    case physics::PhysicsShapeType::Capsule: {
      const float r  = std::max(half.x, half.z);
      const float hh = std::max(half.y - r, 0.001f);
      desc = physics::PhysicsShapeDesc::MakeCapsule(r, hh);
      break;
    }
    default:
      break;
  }
  desc.center_offset = center;
  return desc;
}

const char* LightTypeName(renderer::LightType type) {
  switch (type) {
    case renderer::LightType::kGlobal:     return "Global Light";
    case renderer::LightType::kOmni:       return "Omni Light";
    case renderer::LightType::kCircleSpot: return "Circle Spot";
    case renderer::LightType::kRectSpot:   return "Rectangle Spot";
  }
  return "Unknown";
}

}  // namespace

void PropertiesPanel::Render(game::GameObject* obj) {
  if (!obj) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextUnformatted("No object selected");
    ImGui::PopStyleColor();
    return;
  }

  char name_buf[256];
  std::strncpy(name_buf, obj->GetName().c_str(), sizeof(name_buf) - 1);
  name_buf[sizeof(name_buf) - 1] = '\0';

  ImGui::InputText("Name##propname", name_buf, sizeof(name_buf));
  if (ImGui::IsItemActivated())
    before_name_ = obj->GetName();
  if (ImGui::IsItemDeactivatedAfterEdit()) {
    const std::string new_name(name_buf);
    if (!new_name.empty() && new_name != before_name_ && history_)
      history_->Push(std::make_unique<RenameObjectCommand>(obj, before_name_, new_name));
  }
  ImGui::Separator();

  RenderTransformSection(obj);

  switch (obj->GetType()) {
    case game::GameObjectType::kLight:
      RenderLightProperties(static_cast<game::GameLight*>(obj));
      break;
    case game::GameObjectType::kMesh:
      RenderMeshProperties(static_cast<game::GameMesh*>(obj));
      break;
    case game::GameObjectType::kParticleSystem:
      RenderParticleSystemProperties(
          static_cast<const game::GameParticleSystem*>(obj));
      break;
    case game::GameObjectType::kSoundEmitter:
      RenderSoundEmitterProperties(
          static_cast<game::GameSoundEmitter*>(obj));
      break;
    default:
      break;
  }
}

void PropertiesPanel::RenderTransformSection(game::GameObject* obj) {
  // ImGuizmo uses row-vector convention (transpose of our column-vector Mat4f).
  const core::Mat4f mat_t = obj->GetWorldTransform().Transpose();
  float matrix[16];
  std::memcpy(matrix, mat_t.Data(), sizeof(matrix));

  float translation[3], rotation[3], scale[3];
  ImGuizmo::DecomposeMatrixToComponents(matrix, translation, rotation, scale);

  ImGui::SeparatorText("Transform");

  const bool p_changed    = ImGui::DragFloat3("Position##xfm", translation, 0.1f);
  const bool p_activated  = ImGui::IsItemActivated();
  const bool p_deactivated = ImGui::IsItemDeactivatedAfterEdit();

  const bool r_changed    = ImGui::DragFloat3("Rotation##xfm", rotation, 0.5f,
                                              0.f, 0.f, "%.1f deg");
  const bool r_activated  = ImGui::IsItemActivated();
  const bool r_deactivated = ImGui::IsItemDeactivatedAfterEdit();

  const bool s_changed    = ImGui::DragFloat3("Scale##xfm", scale, 0.01f,
                                              0.001f, 1000.f);
  const bool s_activated  = ImGui::IsItemActivated();
  const bool s_deactivated = ImGui::IsItemDeactivatedAfterEdit();

  if (p_activated || r_activated || s_activated)
    before_transform_ = obj->GetWorldTransform();

  if (p_changed || r_changed || s_changed) {
    float new_matrix[16];
    ImGuizmo::RecomposeMatrixFromComponents(translation, rotation, scale, new_matrix);
    obj->SetWorldTransform(core::Mat4f(new_matrix).Transpose());
    if (on_transform_changed_)
      on_transform_changed_(obj);
  }

  if (history_ && (p_deactivated || r_deactivated || s_deactivated)) {
    const core::Mat4f after = obj->GetWorldTransform();
    if (after != before_transform_)
      history_->Push(std::make_unique<TransformCommand>(obj, before_transform_, after));
  }
}

void PropertiesPanel::RenderLightProperties(game::GameLight* game_light) {
  renderer::Light* light = game_light->GetLight();
  const renderer::LightType type = light->GetType();

  // Captures a snapshot and pushes a command if before != after.
  auto track = [&]() {
    if (!history_) return;
    if (ImGui::IsItemActivated())
      before_snapshot_ = CaptureSnapshot(game_light);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      LightSnapshot after = CaptureSnapshot(game_light);
      if (after != before_snapshot_)
        history_->Push(std::make_unique<LightPropertyCommand>(
            game_light, before_snapshot_, after));
    }
  };

  ImGui::SeparatorText(LightTypeName(type));

  // ---- Common fields -------------------------------------------------------
  const core::Color& col = light->GetColor();
  float c[3] = {col.r, col.g, col.b};
  if (ImGui::ColorEdit3("Color", c))
    light->SetColor({c[0], c[1], c[2]});
  track();

  float intensity = light->GetIntensity();
  if (ImGui::DragFloat("Intensity", &intensity, 0.01f, 0.f, 100.f))
    light->SetIntensity(intensity);
  track();

  bool cast_shadow = light->GetCastShadow();
  if (ImGui::Checkbox("Cast Shadow", &cast_shadow))
    light->SetCastShadow(cast_shadow);
  track();

  static const int   kResolutions[] = {256, 512, 1024, 2048};
  static const char* kResLabels[]   = {"256", "512", "1024", "2048"};
  constexpr int kResCount = 4;
  const int cur_res = light->GetShadowResolution();
  int cur_idx = 2;
  for (int i = 0; i < kResCount; ++i) {
    if (kResolutions[i] == cur_res) {
      cur_idx = i;
      break;
    }
  }
  if (ImGui::Combo("Shadow Resolution", &cur_idx, kResLabels, kResCount))
    light->SetShadowResolution(kResolutions[cur_idx]);
  track();

  float bias = light->GetShadowBias();
  if (ImGui::DragFloat("Shadow Bias", &bias, 0.0001f, 0.f, 0.1f, "%.4f"))
    light->SetShadowBias(bias);
  track();

  ImGui::Spacing();

  // ---- OmniLight -----------------------------------------------------------
  const bool is_omni   = (type == renderer::LightType::kOmni);
  const bool is_circle = (type == renderer::LightType::kCircleSpot);
  const bool is_rect   = (type == renderer::LightType::kRectSpot);
  const bool is_global = (type == renderer::LightType::kGlobal);

  ImGui::BeginDisabled(!is_omni);
  if (is_omni) {
    auto* omni = static_cast<renderer::OmniLight*>(light);
    float radius = omni->GetRadius();
    if (ImGui::DragFloat("Radius", &radius, 0.1f, 0.1f, 500.f))
      omni->SetRadius(radius);
    track();
  } else {
    float dummy = 0.f;
    ImGui::DragFloat("Radius", &dummy, 0.1f, 0.1f, 500.f);
  }
  ImGui::EndDisabled();

  // ---- CircleSpotLight -----------------------------------------------------
  ImGui::BeginDisabled(!is_circle);
  if (is_circle) {
    auto* spot = static_cast<renderer::CircleSpotLight*>(light);

    float inner_deg = spot->GetInnerAngle() * kRad2Deg;
    float outer_deg = spot->GetOuterAngle() * kRad2Deg;

    if (ImGui::DragFloat("Inner Angle (°)", &inner_deg, 0.1f, 0.f, 89.f))
      spot->SetInnerAngle(inner_deg * kDeg2Rad);
    track();

    const float outer_min = inner_deg + 0.1f;
    if (ImGui::DragFloat("Outer Angle (°)", &outer_deg, 0.1f, outer_min, 89.f)) {
      if (outer_deg <= inner_deg) outer_deg = inner_deg + 0.1f;
      spot->SetOuterAngle(outer_deg * kDeg2Rad);
    }
    track();

    float range = spot->GetRange();
    if (ImGui::DragFloat("Range##circle", &range, 0.1f, 0.1f, 1000.f))
      spot->SetRange(range);
    track();

    auto [yaw, pitch] = Vec3fToYawPitch(spot->GetDirection());
    bool dir_changed = false;
    dir_changed |= ImGui::DragFloat("Yaw (°)##circle",   &yaw,   0.1f, -180.f, 180.f);
    track();
    dir_changed |= ImGui::DragFloat("Pitch (°)##circle", &pitch, 0.1f,  -89.f,  89.f);
    track();
    if (dir_changed)
      spot->SetDirection(YawPitchToVec3f(yaw, pitch));
  } else {
    float dummy = 0.f;
    ImGui::DragFloat("Inner Angle (°)", &dummy, 0.1f, 0.f,  89.f);
    ImGui::DragFloat("Outer Angle (°)", &dummy, 0.1f, 0.1f, 89.f);
    ImGui::DragFloat("Range##circle",   &dummy, 0.1f, 0.1f, 1000.f);
    ImGui::DragFloat("Yaw (°)##circle",   &dummy, 0.1f, -180.f, 180.f);
    ImGui::DragFloat("Pitch (°)##circle", &dummy, 0.1f,  -89.f,  89.f);
  }
  ImGui::EndDisabled();

  // ---- RectangleSpotLight --------------------------------------------------
  ImGui::BeginDisabled(!is_rect);
  if (is_rect) {
    auto* rect = static_cast<renderer::RectangleSpotLight*>(light);

    float h_deg = rect->GetHAngle() * kRad2Deg;
    float v_deg = rect->GetVAngle() * kRad2Deg;

    if (ImGui::DragFloat("H Angle (°)", &h_deg, 0.1f, 0.1f, 89.f))
      rect->SetHAngle(h_deg * kDeg2Rad);
    track();

    if (ImGui::DragFloat("V Angle (°)", &v_deg, 0.1f, 0.1f, 89.f))
      rect->SetVAngle(v_deg * kDeg2Rad);
    track();

    float range = rect->GetRange();
    if (ImGui::DragFloat("Range##rect", &range, 0.1f, 0.1f, 1000.f))
      rect->SetRange(range);
    track();

    auto [yaw, pitch] = Vec3fToYawPitch(rect->GetDirection());
    bool dir_changed = false;
    dir_changed |= ImGui::DragFloat("Yaw (°)##rect",   &yaw,   0.1f, -180.f, 180.f);
    track();
    dir_changed |= ImGui::DragFloat("Pitch (°)##rect", &pitch, 0.1f,  -89.f,  89.f);
    track();
    if (dir_changed)
      rect->SetDirection(YawPitchToVec3f(yaw, pitch));
  } else {
    float dummy = 0.f;
    ImGui::DragFloat("H Angle (°)", &dummy, 0.1f, 0.1f, 89.f);
    ImGui::DragFloat("V Angle (°)", &dummy, 0.1f, 0.1f, 89.f);
    ImGui::DragFloat("Range##rect",     &dummy, 0.1f, 0.1f, 1000.f);
    ImGui::DragFloat("Yaw (°)##rect",   &dummy, 0.1f, -180.f, 180.f);
    ImGui::DragFloat("Pitch (°)##rect", &dummy, 0.1f,  -89.f,  89.f);
  }
  ImGui::EndDisabled();

  // ---- GlobalLight ---------------------------------------------------------
  ImGui::BeginDisabled(!is_global);
  if (is_global) {
    auto* global = static_cast<renderer::GlobalLight*>(light);

    auto [yaw, pitch] = Vec3fToYawPitch(global->GetDirection());
    bool dir_changed = false;
    dir_changed |= ImGui::DragFloat("Yaw (°)##global",   &yaw,   0.1f, -180.f, 180.f);
    track();
    dir_changed |= ImGui::DragFloat("Pitch (°)##global", &pitch, 0.1f,  -89.f,  89.f);
    track();
    if (dir_changed)
      global->SetDirection(YawPitchToVec3f(yaw, pitch));

    const core::Vec3f& amb = global->GetAmbientColor();
    float a[3] = {amb.x, amb.y, amb.z};
    if (ImGui::ColorEdit3("Ambient", a))
      global->SetAmbientColor({a[0], a[1], a[2]});
    track();
  } else {
    float dummy = 0.f;
    float dummy3[3] = {};
    ImGui::DragFloat("Yaw (°)##global",   &dummy, 0.1f, -180.f, 180.f);
    ImGui::DragFloat("Pitch (°)##global", &dummy, 0.1f,  -89.f,  89.f);
    ImGui::ColorEdit3("Ambient", dummy3);
  }
  ImGui::EndDisabled();
}

void PropertiesPanel::RenderMeshProperties(game::GameMesh* mesh) {
  ImGui::SeparatorText("Mesh");
  ImGui::LabelText("Template", "%s", mesh->GetTemplate()->GetId().c_str());

  ImGui::Spacing();
  if (!ImGui::CollapsingHeader("Physics"))
    return;

  // --- Enable checkbox ---------------------------------------------------
  bool has_physics = mesh->GetPhysicsDesc().has_value();
  if (ImGui::Checkbox("Enable physics", &has_physics)) {
    const auto before = mesh->GetPhysicsDesc();
    if (has_physics) {
      physics::PhysicsBodyDesc new_desc;
      new_desc.shape = ShapeDescFromBBox(mesh->GetTemplate()->GetLocalBBox(),
                                         physics::PhysicsShapeType::Box);
      mesh->SetPhysicsDesc(new_desc);
    } else {
      mesh->ClearPhysicsDesc();
    }
    if (history_)
      history_->Push(std::make_unique<PhysicsPropertyCommand>(
          mesh, before, mesh->GetPhysicsDesc()));
  }

  if (!has_physics) return;

  // Working copy of the current desc; written back via SetPhysicsDesc on change.
  physics::PhysicsBodyDesc desc = *mesh->GetPhysicsDesc();

  // Macro-like lambda: captures before on drag-start, pushes command on drag-end.
  auto track = [&]() {
    if (!history_) return;
    if (ImGui::IsItemActivated())
      before_physics_desc_ = mesh->GetPhysicsDesc();
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      if (mesh->GetPhysicsDesc() != before_physics_desc_)
        history_->Push(std::make_unique<PhysicsPropertyCommand>(
            mesh, before_physics_desc_, mesh->GetPhysicsDesc()));
    }
  };

  // --- Shape type -------------------------------------------------------
  static const char* kShapeNames[] = {
    "Box", "Sphere", "Cylinder", "Capsule", "ConvexHull", "Exact"
  };
  static const physics::PhysicsShapeType kShapeTypes[] = {
    physics::PhysicsShapeType::Box,
    physics::PhysicsShapeType::Sphere,
    physics::PhysicsShapeType::Cylinder,
    physics::PhysicsShapeType::Capsule,
    physics::PhysicsShapeType::ConvexHull,
    physics::PhysicsShapeType::Exact,
  };
  constexpr int kShapeCount = 6;

  int shape_idx = 0;
  for (int i = 0; i < kShapeCount; ++i) {
    if (kShapeTypes[i] == desc.shape.type) {
      shape_idx = i;
      break;
    }
  }
  if (ImGui::Combo("Shape type", &shape_idx, kShapeNames, kShapeCount)) {
    const auto before = mesh->GetPhysicsDesc();
    const physics::PhysicsShapeType new_type = kShapeTypes[shape_idx];
    switch (new_type) {
      case physics::PhysicsShapeType::Box:
      case physics::PhysicsShapeType::Sphere:
      case physics::PhysicsShapeType::Cylinder:
      case physics::PhysicsShapeType::Capsule:
        desc.shape = ShapeDescFromBBox(mesh->GetTemplate()->GetLocalBBox(), new_type);
        break;
      case physics::PhysicsShapeType::ConvexHull:
        desc.shape = physics::PhysicsShapeDesc::MakeConvexHull();
        break;
      case physics::PhysicsShapeType::Exact:
        desc.shape = physics::PhysicsShapeDesc::MakeExact();
        break;
      default:
        break;
    }
    mesh->SetPhysicsDesc(desc);
    if (history_)
      history_->Push(std::make_unique<PhysicsPropertyCommand>(
          mesh, before, mesh->GetPhysicsDesc()));
  }

  // --- Per-shape parameters ---------------------------------------------
  const physics::PhysicsShapeType shape = desc.shape.type;
  const bool is_primitive = (shape == physics::PhysicsShapeType::Box
                          || shape == physics::PhysicsShapeType::Sphere
                          || shape == physics::PhysicsShapeType::Cylinder
                          || shape == physics::PhysicsShapeType::Capsule);

  if (shape == physics::PhysicsShapeType::Box) {
    float he[3] = {
      desc.shape.box.half_extents.x,
      desc.shape.box.half_extents.y,
      desc.shape.box.half_extents.z,
    };
    if (ImGui::DragFloat3("Half extents", he, 0.01f, 0.001f, 1000.f)) {
      desc.shape.box.half_extents = {he[0], he[1], he[2]};
      mesh->SetPhysicsDesc(desc);
    }
    track();
  }

  const bool needs_radius = (shape == physics::PhysicsShapeType::Sphere
                           || shape == physics::PhysicsShapeType::Cylinder
                           || shape == physics::PhysicsShapeType::Capsule);
  if (needs_radius) {
    float radius = (shape == physics::PhysicsShapeType::Sphere)
                 ? desc.shape.sphere.radius
                 : (shape == physics::PhysicsShapeType::Cylinder)
                     ? desc.shape.cylinder.radius
                     : desc.shape.capsule.radius;
    if (ImGui::DragFloat("Radius", &radius, 0.01f, 0.001f, 1000.f)) {
      if (shape == physics::PhysicsShapeType::Sphere)
        desc.shape.sphere.radius = radius;
      else if (shape == physics::PhysicsShapeType::Cylinder)
        desc.shape.cylinder.radius = radius;
      else
        desc.shape.capsule.radius = radius;
      mesh->SetPhysicsDesc(desc);
    }
    track();
  }

  const bool needs_half_height = (shape == physics::PhysicsShapeType::Cylinder
                               || shape == physics::PhysicsShapeType::Capsule);
  if (needs_half_height) {
    float hh = (shape == physics::PhysicsShapeType::Cylinder)
             ? desc.shape.cylinder.half_height
             : desc.shape.capsule.half_height;
    if (ImGui::DragFloat("Half height", &hh, 0.01f, 0.001f, 1000.f)) {
      if (shape == physics::PhysicsShapeType::Cylinder)
        desc.shape.cylinder.half_height = hh;
      else
        desc.shape.capsule.half_height = hh;
      mesh->SetPhysicsDesc(desc);
    }
    track();
  }

  if (!is_primitive)
    ImGui::TextDisabled("Computed from mesh geometry");

  ImGui::Spacing();

  // --- Motion type ------------------------------------------------------
  static const char* kMotionNames[] = {"Static", "Kinematic", "Dynamic"};
  static const physics::MotionType kMotionTypes[] = {
    physics::MotionType::Static,
    physics::MotionType::Kinematic,
    physics::MotionType::Dynamic,
  };
  constexpr int kMotionCount = 3;

  // Exact and Terrain shapes may not use Dynamic or Kinematic.
  const bool motion_restricted = (shape == physics::PhysicsShapeType::Exact
                                || shape == physics::PhysicsShapeType::Terrain);

  int motion_idx = 0;
  for (int i = 0; i < kMotionCount; ++i) {
    if (kMotionTypes[i] == desc.motion_type) {
      motion_idx = i;
      break;
    }
  }

  ImGui::BeginDisabled(motion_restricted);
  if (ImGui::Combo("Motion type", &motion_idx, kMotionNames, kMotionCount)) {
    const auto before = mesh->GetPhysicsDesc();
    desc.motion_type = kMotionTypes[motion_idx];
    mesh->SetPhysicsDesc(desc);
    if (history_)
      history_->Push(std::make_unique<PhysicsPropertyCommand>(
          mesh, before, mesh->GetPhysicsDesc()));
  }
  ImGui::EndDisabled();

  ImGui::Spacing();

  // --- Material properties ----------------------------------------------
  if (ImGui::SliderFloat("Friction", &desc.material.friction, 0.f, 1.f)) {
    mesh->SetPhysicsDesc(desc);
  }
  track();

  if (ImGui::SliderFloat("Restitution", &desc.material.restitution, 0.f, 1.f)) {
    mesh->SetPhysicsDesc(desc);
  }
  track();

  if (ImGui::DragFloat("Mass", &desc.material.mass, 0.1f, 0.001f, 100000.f)) {
    mesh->SetPhysicsDesc(desc);
  }
  track();

  if (ImGui::SliderFloat("Lin. damping", &desc.material.linear_damping,
                         0.f, 1.f)) {
    mesh->SetPhysicsDesc(desc);
  }
  track();

  if (ImGui::SliderFloat("Ang. damping", &desc.material.angular_damping,
                         0.f, 1.f)) {
    mesh->SetPhysicsDesc(desc);
  }
  track();

  if (ImGui::SliderFloat("Gravity factor", &desc.material.gravity_factor,
                         0.f, 4.f)) {
    mesh->SetPhysicsDesc(desc);
  }
  track();

  ImGui::Spacing();

  // --- Collision layer / mask -------------------------------------------
  int layer = static_cast<int>(desc.collision_layer);
  if (ImGui::InputInt("Collision layer", &layer)) {
    if (layer < 0) layer = 0;
    if (layer >= physics::kLayerCount) layer = physics::kLayerCount - 1;
    const auto before = mesh->GetPhysicsDesc();
    desc.collision_layer = static_cast<uint16_t>(layer);
    mesh->SetPhysicsDesc(desc);
    if (history_)
      history_->Push(std::make_unique<PhysicsPropertyCommand>(
          mesh, before, mesh->GetPhysicsDesc()));
  }

  uint16_t mask = desc.collision_mask;
  if (ImGui::InputScalar("Collision mask", ImGuiDataType_U16, &mask,
                         nullptr, nullptr, "%04X",
                         ImGuiInputTextFlags_CharsHexadecimal)) {
    const auto before = mesh->GetPhysicsDesc();
    desc.collision_mask = mask;
    mesh->SetPhysicsDesc(desc);
    if (history_)
      history_->Push(std::make_unique<PhysicsPropertyCommand>(
          mesh, before, mesh->GetPhysicsDesc()));
  }
}

void PropertiesPanel::RenderParticleSystemProperties(
    const game::GameParticleSystem* ps) {
  ImGui::SeparatorText("Particle System");

  const particles::ParticleSystemTemplate* tmpl = ps->GetTemplate();
  const char* tmpl_name = tmpl ? tmpl->GetId().c_str() : "(none)";
  ImGui::LabelText("Effect", "%s", tmpl_name);

  if (tmpl && on_open_particle_editor_) {
    if (ImGui::Button("Open in Particle Editor"))
      on_open_particle_editor_(tmpl->GetId());
  }
}

void PropertiesPanel::RenderSoundEmitterProperties(
    game::GameSoundEmitter* emitter) {
  ImGui::SeparatorText("Sound Emitter");

  // ---- Sound resource picker -----------------------------------------------
  const std::string& snd = emitter->GetSoundName();
  ImGui::LabelText("Sound", "%s", snd.empty() ? "(none)" : snd.c_str());
  if (ImGui::Button("Change...##sound"))
    sound_picker_modal_.Open();

  if (const std::string picked = sound_picker_modal_.Render(); !picked.empty())
    emitter->SetSoundName(picked);

  // ---- Volume scale --------------------------------------------------------
  float vol = emitter->GetVolumeScale();
  if (ImGui::DragFloat("Volume scale##se", &vol, 0.01f, 0.f, 4.f, "%.2f"))
    emitter->SetVolumeScale(vol);

  // ---- Preview -------------------------------------------------------------
  ImGui::Spacing();
  const bool can_preview = !snd.empty();
  if (!can_preview) ImGui::BeginDisabled();
  if (ImGui::Button("Play Once") && on_play_sound_once_)
    on_play_sound_once_(snd, vol);
  if (!can_preview) ImGui::EndDisabled();
}

}  // namespace editor
