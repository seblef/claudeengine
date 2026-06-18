#include "editor/RenderingSettingsPanel.h"

#include <imgui.h>

namespace editor {

void RenderingSettingsPanel::Render() {
  if (ImGui::CollapsingHeader("Physics Debug Draw")) {
    int body_mode = static_cast<int>(physics_debug_body_mode_);
    ImGui::RadioButton("Selected only", &body_mode,
                       static_cast<int>(PhysicsDebugBodyMode::kSelectedOnly));
    ImGui::SameLine();
    ImGui::RadioButton("All bodies", &body_mode,
                       static_cast<int>(PhysicsDebugBodyMode::kAllBodies));
    physics_debug_body_mode_ = static_cast<PhysicsDebugBodyMode>(body_mode);

    ImGui::Checkbox("Constraints",     &physics_debug_constraints_);
    ImGui::Checkbox("Contact points",  &physics_debug_contact_points_);
    ImGui::Checkbox("Broadphase tree", &physics_debug_broadphase_);
  }
}

}  // namespace editor
