#include "editor/RenderingSettingsPanel.h"

#include <imgui.h>

namespace editor {

void RenderingSettingsPanel::Render() {
  ImGui::Checkbox("Physics shapes", &physics_gizmos_enabled_);
}

}  // namespace editor
