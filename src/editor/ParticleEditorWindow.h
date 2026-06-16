#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "abstract/ConstantBuffer.h"
#include "abstract/RenderTarget.h"
#include "abstract/RenderTargetGroup.h"
#include "abstract/VideoDevice.h"
#include "core/Camera.h"
#include "core/Vec3f.h"
#include "particles/EmbeddedLightDesc.h"
#include "particles/ParticleEmitter.h"
#include "particles/ParticleRenderer.h"
#include "particles/ParticleSubSystemDesc.h"

namespace editor {

// Floating window for creating and editing .particles.yaml composite effects.
//
// Layout:
//   Top bar       — New / Load / Save / Save As file operations and filename.
//   Left column   — Sub-system list with [+] [-] [↑] [↓] controls;
//                   embedded lights list with [+] [-] below it.
//   Middle column — Property widgets for the selected sub-system or light.
//   Right column  — Live particle preview rendered into an offscreen FBO.
//
// Usage:
//   Call Open() to show the window from the Effects menu.
//   Call Render(time, dt) every ImGui frame between NewFrame() and Render().
//   Any property edit immediately rebuilds the preview simulation.
class ParticleEditorWindow {
 public:
  explicit ParticleEditorWindow(abstract::VideoDevice* video);
  ~ParticleEditorWindow() = default;

  ParticleEditorWindow(const ParticleEditorWindow&)            = delete;
  ParticleEditorWindow& operator=(const ParticleEditorWindow&) = delete;

  // Opens (or re-focuses) the editor window.
  void Open();

  // Opens the editor with the named template loaded (basename without
  // extension, e.g. "smoke"). Creates the window if not already open.
  void OpenTemplate(const std::string& name);

  // Renders the window. time is elapsed app time in seconds; dt is the frame
  // delta used to advance the particle simulation.
  // Must be called between ImGui::NewFrame() and ImGui::Render().
  void Render(float time, float dt);

  // Registers a callback invoked after a template is successfully saved to
  // disk. The argument is the template basename without extension (e.g. "fire").
  // Use this to reload the in-memory template and refresh live scene instances.
  void SetOnTemplateSaved(std::function<void(const std::string&)> cb) {
    on_template_saved_ = std::move(cb);
  }

  [[nodiscard]] bool IsOpen() const { return open_; }

 private:
  // Rebuilds preview_emitters_ from the current sub_systems_ and re-registers
  // them with preview_renderer_. Called when preview_dirty_ is set.
  void RebuildPreview();

  // Renders the current preview emitters into preview_rtg_.
  // Must be called before the ImGui::Image() displaying the result.
  void RenderPreviewFrame(float time);

  // Uploads current camera data into scene_infos_cb_.
  void FillSceneInfosCB(float time);

  // Recomputes preview_camera_ position/target from the current orbit state.
  void UpdatePreviewCamera();

  // Clears sub_systems_ and lights_; resets path and unsaved flag.
  void NewFile();

  // Opens a native file dialog filtered to data/particles/ and loads the YAML.
  void LoadFromFile();

  // Saves to current_path_; falls through to SaveAs() when path is empty.
  void Save();

  // Opens a native save dialog and serialises the effect to the chosen path.
  void SaveAs();

  // Writes sub_systems_ and lights_ as a .particles.yaml file to path.
  // Returns true on success.
  bool SerializeToFile(const std::filesystem::path& path);

  // Renders the toolbar row: New / Load / Save / Save As and filename hint.
  void RenderToolbar();

  // Renders the sub-system list with add/remove/reorder buttons.
  void RenderSubSystemList();

  // Renders property widgets for the selected sub-system.
  // No-op when no sub-system is selected.
  void RenderSubSystemProperties();

  // Renders the embedded lights list and properties for the selected light.
  void RenderLightsSection();

  // Renders the live preview FBO image and the Restart button.
  void RenderPreviewColumn(float time);

  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice*  video_;
  // cppcheck-suppress unusedStructMember
  bool                    open_    = false;
  // cppcheck-suppress unusedStructMember
  bool                    unsaved_ = false;

  // Working descs — edited in-place by the property widgets.
  // cppcheck-suppress unusedStructMember
  std::vector<particles::ParticleSubSystemDesc> sub_systems_;
  // cppcheck-suppress unusedStructMember
  std::vector<particles::EmbeddedLightDesc>     lights_;

  // Selection state; -1 = nothing selected.
  int selected_sub_system_ = -1;
  int selected_light_      = -1;

  // Currently open file path; empty when the effect has never been saved.
  // cppcheck-suppress unusedStructMember
  std::filesystem::path current_path_;

  // Live preview state — rebuilt whenever preview_dirty_ is set.
  // preview_descs_ stores stable copies so the emitters' references are valid
  // even if sub_systems_ reallocates after an add/remove.
  // cppcheck-suppress unusedStructMember
  std::vector<particles::ParticleSubSystemDesc>            preview_descs_;
  // cppcheck-suppress unusedStructMember
  std::vector<std::unique_ptr<particles::ParticleEmitter>> preview_emitters_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<particles::ParticleRenderer>             preview_renderer_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<abstract::RenderTarget>                  preview_color_rt_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<abstract::RenderTarget>                  preview_depth_rt_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<abstract::RenderTargetGroup>             preview_rtg_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<abstract::ConstantBuffer>                scene_infos_cb_;
  core::Camera                                             preview_camera_;

  // Orbit camera state — updated by mouse input in RenderPreviewColumn().
  float       orbit_azimuth_deg_   = 30.f;
  float       orbit_elevation_deg_ = 20.f;
  // cppcheck-suppress unusedStructMember
  float       orbit_distance_      = 7.f;
  // cppcheck-suppress unusedStructMember
  core::Vec3f orbit_center_        = {0.f, 1.5f, 0.f};

  // Set by any property change; triggers RebuildPreview() at frame start.
  // cppcheck-suppress unusedStructMember
  bool                                                     preview_dirty_ = false;

  // cppcheck-suppress unusedStructMember
  std::function<void(const std::string&)>                  on_template_saved_;

  // cppcheck-suppress unusedStructMember
  static constexpr int kPreviewW        = 300;
  // cppcheck-suppress unusedStructMember
  static constexpr int kPreviewH        = 300;
  // cppcheck-suppress unusedStructMember
  static constexpr int kSceneInfosSlot  = 2;
};

}  // namespace editor
