#pragma once

namespace editor {

// Applies the Slate & Teal color scheme and style to the current ImGui context.
// Call once after ImGui::CreateContext(), before the first frame.
void ApplySlateAndTealTheme();

}  // namespace editor
