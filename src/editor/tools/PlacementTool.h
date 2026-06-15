#pragma once

#include <functional>
#include <memory>

#include "editor/tools/EditorToolBase.h"

#include <imgui.h>

namespace game { class GameObject; }

namespace editor {

class EditorScene;
class PickingAccelerator;

// Tool that manages object-creation preview and placement.
//
// The preview object follows the cursor each frame via ComputeTerrainHit().
// LMB click (without Alt) confirms placement and pushes a PlaceObjectCommand
// onto the history. on_done fires after placement so EditorWindow can switch
// back to SelectionTool.
//
// EditorWindow constructs a fresh PlacementTool each time the user picks a
// creation tool from the toolbar, passing the appropriate preview object and
// height. No subclassing per object type is needed.
class PlacementTool : public EditorToolBase {
 public:
  // preview: object that follows the cursor (transferred to scene on first hit).
  // height:  Y offset above the terrain hit point.
  // cursor:  ImGui cursor shown while hovering the viewport.
  // on_done: called after placement so EditorWindow can switch back to
  //          SelectionTool.
  PlacementTool(std::unique_ptr<game::GameObject> preview,
                float height,
                ImGuiMouseCursor cursor,
                std::function<void()> on_done);

  void OnActivate(const EditorToolContext& ctx) override;
  void OnDeactivate() override;
  void OnRender(const EditorToolContext& ctx,
                ImVec2 image_pos, ImVec2 image_size) override;

 private:
  void UpdatePreviewPosition(const EditorToolContext& ctx,
                              ImVec2 image_pos, ImVec2 image_size);
  void PlaceObject(const EditorToolContext& ctx);

  // Held locally until the first valid terrain hit, then transferred to scene.
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<game::GameObject> pending_preview_;
  // Non-owning pointer to the preview once it lives in the scene.
  // cppcheck-suppress unusedStructMember
  game::GameObject*                 preview_object_ = nullptr;
  // cppcheck-suppress unusedStructMember
  float                             preview_height_ = 0.f;
  // cppcheck-suppress unusedStructMember
  ImGuiMouseCursor                  preview_cursor_ = ImGuiMouseCursor_None;
  // cppcheck-suppress unusedStructMember
  std::function<void()>             on_done_;

  // Cached from OnActivate(); cleared on OnDeactivate().
  // cppcheck-suppress unusedStructMember
  EditorScene*        scene_       = nullptr;
  // cppcheck-suppress unusedStructMember
  PickingAccelerator* picking_acc_ = nullptr;
};

}  // namespace editor
