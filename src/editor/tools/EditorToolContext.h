#pragma once

namespace abstract { class VideoDevice; }

namespace game { class GameCamera; }
namespace terrain { class TerrainData; }

namespace editor {

class EditorCommandHistory;
class EditorScene;
class PickingAccelerator;

// Aggregates non-owning pointers to the subsystems that editor tools need.
// Passed by const-ref to OnActivate() and OnRender(); lifetime is managed by
// EditorViewport and guaranteed to outlive any active tool.
struct EditorToolContext {
    // cppcheck-suppress unusedStructMember
    EditorScene*           scene;
    // cppcheck-suppress unusedStructMember
    game::GameCamera*      camera;
    // cppcheck-suppress unusedStructMember
    PickingAccelerator*    picking_acc;
    // cppcheck-suppress unusedStructMember
    EditorCommandHistory*  history;
    // cppcheck-suppress unusedStructMember
    abstract::VideoDevice* video;
    // True if the ImGuizmo transform gizmo was being dragged last frame.
    // Tools that fire on LMB release must skip the pick on the drag-end frame
    // to avoid a spurious selection change when the user releases the gizmo.
    // cppcheck-suppress unusedStructMember
    bool                        gizmo_was_using = false;
    // Terrain heightmap for floor-hit raycasts; nullptr when no terrain exists.
    // cppcheck-suppress unusedStructMember
    const terrain::TerrainData* terrain_data    = nullptr;
};

}  // namespace editor
