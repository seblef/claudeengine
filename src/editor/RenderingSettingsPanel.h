#pragma once

namespace editor {

// Dockable panel that groups per-frame rendering debug toggles (gizmos,
// wireframe overlays, etc.).
//
// Render() draws the ImGui content for the panel body; the caller is
// responsible for wrapping it in ImGui::Begin() / ImGui::End().
class RenderingSettingsPanel {
 public:
  /// Selects which bodies PhysicsSystem::DrawDebug() should draw each frame.
  enum class PhysicsDebugBodyMode {
    kSelectedOnly,  ///< Draw only the currently selected entities' physics shapes.
    kAllBodies,     ///< Draw all non-terrain physics shapes.
  };

  RenderingSettingsPanel() = default;

  // Renders the panel body (checkboxes for each toggle).
  void Render();

  [[nodiscard]] PhysicsDebugBodyMode GetPhysicsDebugBodyMode() const {
    return physics_debug_body_mode_;
  }
  [[nodiscard]] bool IsPhysicsDebugShapesEnabled()         const {
    return physics_debug_shapes_;
  }
  [[nodiscard]] bool IsPhysicsDebugConstraintsEnabled()   const {
    return physics_debug_constraints_;
  }
  [[nodiscard]] bool IsPhysicsDebugContactPointsEnabled() const {
    return physics_debug_contact_points_;
  }
  [[nodiscard]] bool IsPhysicsDebugBroadPhaseEnabled()    const {
    return physics_debug_broadphase_;
  }

 private:
  // Physics debug draw settings (passed to PhysicsSystem::DrawDebug each frame).
  // cppcheck-suppress unusedStructMember
  PhysicsDebugBodyMode physics_debug_body_mode_ = PhysicsDebugBodyMode::kSelectedOnly;
  // cppcheck-suppress unusedStructMember
  bool physics_debug_shapes_         = false;
  // cppcheck-suppress unusedStructMember
  bool physics_debug_constraints_    = false;
  // cppcheck-suppress unusedStructMember
  bool physics_debug_contact_points_ = false;
  // cppcheck-suppress unusedStructMember
  bool physics_debug_broadphase_     = false;
};

}  // namespace editor
