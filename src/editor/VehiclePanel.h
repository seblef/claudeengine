#pragma once

#include <array>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

#include "abstract/RenderTarget.h"
#include "abstract/RenderTargetGroup.h"
#include "abstract/VideoDevice.h"
#include "core/Camera.h"
#include "core/CoordinateSystem.h"
#include "core/ProjectionType.h"
#include "core/Vec3f.h"
#include "editor/IResourcePanel.h"
#include "physics/VehicleDesc.h"

namespace game  { class MeshTemplate; }
namespace renderer {
class GlobalLight;
class MeshInstance;
class PreviewRenderer;
}  // namespace renderer

namespace editor {

class MeshPreview;

// Resource editor panel for .vehicle.yaml descriptor files.
//
// Exposes three UI sections:
//   Body     — mesh file picker + 320×240 interactive orbit preview.
//   Wheels   — per-axle mesh pickers, wheel-position inputs, and a 320×240
//              combined body+wheels preview with an ImGuizmo translate gizmo
//              for the currently focused wheel.
//   Physics  — sliders/drag-floats for all VehicleDesc physics parameters.
//
// An actions bar at the bottom provides Save, Revert, and Place in Scene.
// "Place in Scene" fires the on_place_in_scene_ callback (set by the owner)
// and is disabled when no callback is registered.
class VehiclePanel : public IResourcePanel {
 public:
  VehiclePanel(std::filesystem::path path, abstract::VideoDevice* video);
  ~VehiclePanel() override;

  VehiclePanel(const VehiclePanel&)            = delete;
  VehiclePanel& operator=(const VehiclePanel&) = delete;

  void Draw()          override;
  bool IsDirty() const override { return dirty_; }
  void Save()          override;

  // Registers the callback invoked when the user clicks "Place in Scene".
  // The callback receives the absolute path to the .vehicle.yaml file.
  void SetOnPlaceInScene(std::function<void(const std::filesystem::path&)> cb) {
    on_place_in_scene_ = std::move(cb);
  }

 private:
  void LoadFromYaml();
  void SaveToYaml();

  void DrawBodySection();
  void DrawWheelsSection();
  void DrawPhysicsSection();
  void DrawActionsBar();

  // Opens an NFD mesh file dialog; on selection updates rel_path, releases the
  // old template, loads a new one, and refreshes preview.
  void PickBodyMesh();
  void PickFrontWheelMesh();
  void PickRearWheelMesh();

  // Loads the template at the given data-relative path into out_tmpl and
  // refreshes the associated preview.  Releases the previous template first.
  void UpdateBodyMesh(const std::string& rel_path);
  void UpdateFrontWheelMesh(const std::string& rel_path);
  void UpdateRearWheelMesh(const std::string& rel_path);

  // Recreates the combined-preview MeshInstances for all meshes that are loaded.
  void RebuildCombinedInstances();

  // Renders all loaded meshes (body + 4 wheels) into combined_rt_.
  void RenderCombined(float time);

  // Draws combined preview image and optional ImGuizmo gizmo for focused_wheel_.
  void DrawCombinedPreview(float time);

  // Updates combined_camera_ from the current orbit state.
  void UpdateCombinedCamera();

  // cppcheck-suppress unusedStructMember
  std::filesystem::path  path_;
  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice* video_;
  // cppcheck-suppress unusedStructMember
  bool                   dirty_ = false;

  // Paths relative to core::Config::GetDataFolder().
  // cppcheck-suppress unusedStructMember
  std::string body_mesh_path_;
  // cppcheck-suppress unusedStructMember
  std::string front_wheel_mesh_path_;
  // cppcheck-suppress unusedStructMember
  std::string rear_wheel_mesh_path_;

  // cppcheck-suppress unusedStructMember
  physics::VehicleDesc vehicle_desc_;

  // Index of the wheel row that has keyboard focus (-1 = none).
  // 0=FL, 1=FR, 2=RL, 3=RR
  // cppcheck-suppress unusedStructMember
  int focused_wheel_ = -1;

  // cppcheck-suppress unusedStructMember
  std::function<void(const std::filesystem::path&)> on_place_in_scene_;

  // Non-owning: ref-counted via core::Resource; released in destructor.
  // cppcheck-suppress unusedStructMember
  game::MeshTemplate* body_tmpl_        = nullptr;
  // cppcheck-suppress unusedStructMember
  game::MeshTemplate* front_wheel_tmpl_ = nullptr;
  // cppcheck-suppress unusedStructMember
  game::MeshTemplate* rear_wheel_tmpl_  = nullptr;

  // 320×240 interactive body mesh preview.
  std::unique_ptr<MeshPreview> body_preview_;
  // 64×64 static wheel thumbnails.
  std::unique_ptr<MeshPreview> front_wheel_thumb_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<MeshPreview> rear_wheel_thumb_;

  // ---- Combined vehicle preview (body + 4 wheels) --------------------------

  std::unique_ptr<abstract::RenderTarget>      combined_rt_;
  std::unique_ptr<abstract::RenderTargetGroup> combined_rtg_;
  std::unique_ptr<renderer::PreviewRenderer>   combined_renderer_;
  std::unique_ptr<renderer::GlobalLight>       combined_light_;
  // cppcheck-suppress unusedStructMember
  core::Camera                                 combined_camera_;

  // Orbit state for the combined preview.
  // cppcheck-suppress unusedStructMember
  float combined_azimuth_deg_   = 45.f;
  // cppcheck-suppress unusedStructMember
  float combined_elevation_deg_ = 20.f;
  // cppcheck-suppress unusedStructMember
  float combined_distance_      = 5.f;
  // cppcheck-suppress unusedStructMember
  core::Vec3f combined_center_  = {0.f, 0.f, 0.f};

  // One MeshInstance per visible mesh: [0]=body, [1]=FL, [2]=FR, [3]=RL, [4]=RR.
  // cppcheck-suppress unusedStructMember
  std::array<std::unique_ptr<renderer::MeshInstance>, 5> combined_instances_;
};

}  // namespace editor
