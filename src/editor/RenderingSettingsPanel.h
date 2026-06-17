#pragma once

namespace editor {

// Dockable panel that groups per-frame rendering debug toggles (gizmos,
// wireframe overlays, etc.).
//
// Render() draws the ImGui content for the panel body; the caller is
// responsible for wrapping it in ImGui::Begin() / ImGui::End().
class RenderingSettingsPanel {
 public:
  RenderingSettingsPanel() = default;

  // Renders the panel body (checkboxes for each toggle).
  void Render();

  void SetPhysicsGizmosEnabled(bool enabled) { physics_gizmos_enabled_ = enabled; }
  [[nodiscard]] bool IsPhysicsGizmosEnabled() const { return physics_gizmos_enabled_; }

 private:
  // cppcheck-suppress unusedStructMember
  bool physics_gizmos_enabled_ = false;
};

}  // namespace editor
