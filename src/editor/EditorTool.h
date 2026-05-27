#pragma once

namespace editor {

// Active tool in the editor toolbar.
enum class EditorTool {
  kSelection,  // click-to-select; no transform gizmo
  kCamera,     // reserved for camera-specific interactions; no gizmo
  kTranslate,
  kRotate,
  kScale,
  // Creation tools: no gizmo; EditorViewport selection is disabled while active.
  kCreateMesh,
  kCreateOmniLight,
  kCreateCircleSpot,
  kCreateRectSpot,
  kCreatePlayerStart,
};

// Returns true for any object-creation tool.
inline bool IsCreationTool(EditorTool tool) {
  return tool == EditorTool::kCreateMesh        ||
         tool == EditorTool::kCreateOmniLight   ||
         tool == EditorTool::kCreateCircleSpot  ||
         tool == EditorTool::kCreateRectSpot    ||
         tool == EditorTool::kCreatePlayerStart;
}

}  // namespace editor
