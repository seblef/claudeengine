#pragma once

namespace abstract { class VideoDevice; }

namespace game { class GameCamera; }

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
};

}  // namespace editor
