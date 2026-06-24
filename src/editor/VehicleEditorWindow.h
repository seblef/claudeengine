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
#include "physics/VehicleDesc.h"

namespace game  { class MeshTemplate; }
namespace renderer {
class GlobalLight;
class MeshInstance;
class PreviewRenderer;
}  // namespace renderer

namespace editor {

// Floating window for inspecting and editing a .vehicle.yaml descriptor file.
//
// Layout: Body section (mesh picker), Wheels section (per-axle mesh pickers,
// wheel selector radio buttons, 320×240 combined preview with always-active
// ImGuizmo gizmo, per-wheel position DragFloat3 inputs), Physics section
// (sliders for all VehicleDesc parameters), and an actions bar
// (Save / Revert / Place in Scene).
//
// Usage:
//   Call Open(path) when the user double-clicks a .vehicle.yaml in the
//   ResourceBrowser or creates a new one. Call Render() every ImGui frame.
//   Closing the window with unsaved changes triggers a Save/Discard/Cancel
//   modal before the window is hidden.
class VehicleEditorWindow {
 public:
  explicit VehicleEditorWindow(abstract::VideoDevice* video);
  ~VehicleEditorWindow();

  VehicleEditorWindow(const VehicleEditorWindow&)            = delete;
  VehicleEditorWindow& operator=(const VehicleEditorWindow&) = delete;

  // Opens (or re-focuses) the editor for the given .vehicle.yaml path.
  void Open(const std::filesystem::path& path);

  // Renders the ImGui window. No-op when the window is not shown.
  void Render();

  // Returns true when the window has unsaved changes.
  bool IsDirty() const { return dirty_; }

  // Registers the callback invoked when the user clicks "Place in Scene".
  void SetOnPlaceInScene(std::function<void(const std::filesystem::path&)> cb) {
    on_place_in_scene_ = std::move(cb);
  }

 private:
  void LoadFromYaml();
  void SaveToYaml();
  void Save();

  void DrawBodySection();
  void DrawWheelsSection();
  void DrawPhysicsSection();
  void DrawActionsBar();

  // Opens an NFD mesh file dialog and updates the body mesh.
  void PickBodyMesh();
  // Opens an NFD mesh file dialog and updates the front wheel mesh.
  void PickFrontWheelMesh();
  // Opens an NFD mesh file dialog and updates the rear wheel mesh.
  void PickRearWheelMesh();

  void UpdateBodyMesh(const std::string& rel_path);
  void UpdateFrontWheelMesh(const std::string& rel_path);
  void UpdateRearWheelMesh(const std::string& rel_path);

  // Recreates combined-preview MeshInstances for all loaded meshes.
  void RebuildCombinedInstances();

  // Renders body + 4 wheels into combined_rt_.
  void RenderCombined(float time);

  // Draws the combined preview image and optional ImGuizmo gizmo for
  // focused_wheel_.
  void DrawCombinedPreview(float time);

  // Updates combined_camera_ from the current orbit state.
  void UpdateCombinedCamera();

  // Renders the dirty-close confirmation modal.
  void RenderDirtyCloseModal();

  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice* video_;
  // cppcheck-suppress unusedStructMember
  std::filesystem::path  path_;
  // cppcheck-suppress unusedStructMember
  bool                   show_  = false;
  // cppcheck-suppress unusedStructMember
  bool                   dirty_ = false;
  // Set to true on the frame the window's close button is clicked while dirty.
  // cppcheck-suppress unusedStructMember
  bool                   open_dirty_modal_ = false;

  // Paths relative to core::Config::GetDataFolder().
  // cppcheck-suppress unusedStructMember
  std::string body_mesh_path_;
  // cppcheck-suppress unusedStructMember
  std::string front_wheel_mesh_path_;
  // cppcheck-suppress unusedStructMember
  std::string rear_wheel_mesh_path_;

  // cppcheck-suppress unusedStructMember
  physics::VehicleDesc vehicle_desc_;
  // cppcheck-suppress unusedStructMember
  bool use_convex_hull_body_ = false;

  // Index of the active wheel in the selector. 0=FL, 1=FR, 2=RL, 3=RR.
  // Always in [0, 3]: set by the radio-button bar, click-to-select, or
  // interacting with a DragFloat3 row.
  // cppcheck-suppress unusedStructMember
  int focused_wheel_ = 0;

  // cppcheck-suppress unusedStructMember
  std::function<void(const std::filesystem::path&)> on_place_in_scene_;

  // Non-owning: ref-counted via core::Resource; released in destructor.
  // cppcheck-suppress unusedStructMember
  game::MeshTemplate* body_tmpl_        = nullptr;
  // cppcheck-suppress unusedStructMember
  game::MeshTemplate* front_wheel_tmpl_ = nullptr;
  // cppcheck-suppress unusedStructMember
  game::MeshTemplate* rear_wheel_tmpl_  = nullptr;

  // ---- Combined vehicle preview (body + 4 wheels) --------------------------

  std::unique_ptr<abstract::RenderTarget>      combined_rt_;
  std::unique_ptr<abstract::RenderTargetGroup> combined_rtg_;
  std::unique_ptr<renderer::PreviewRenderer>   combined_renderer_;
  std::unique_ptr<renderer::GlobalLight>       combined_light_;
  // Full-ambient, zero-directional light used when preview_lit_ is false.
  std::unique_ptr<renderer::GlobalLight>       combined_flat_light_;
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
  // True while the user is dragging to orbit (drag started inside the preview).
  // cppcheck-suppress unusedStructMember
  bool orbit_drag_active_       = false;
  // When false the preview uses a flat full-ambient light so wheel positions
  // are easier to read without directional shading.
  // cppcheck-suppress unusedStructMember
  bool preview_lit_             = true;
  // Vehicle editor window scroll-Y captured at end of each frame so we can
  // undo the scroll when the user wheel-zooms inside the preview.
  // cppcheck-suppress unusedStructMember
  float preview_scroll_y_       = 0.f;

  // One MeshInstance per visible mesh: [0]=body, [1]=FL, [2]=FR, [3]=RL, [4]=RR.
  // cppcheck-suppress unusedStructMember
  std::array<std::unique_ptr<renderer::MeshInstance>, 5> combined_instances_;
};

}  // namespace editor
