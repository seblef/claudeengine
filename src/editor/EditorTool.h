#pragma once

namespace editor {

// Active tool in the editor toolbar.
enum class EditorTool {
  kSelection,  // click-to-select; no transform gizmo
  kCamera,     // reserved for camera-specific interactions; no gizmo
  kTranslate,
  kRotate,
  kScale,
};

}  // namespace editor
