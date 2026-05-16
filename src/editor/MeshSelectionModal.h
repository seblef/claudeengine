#pragma once

#include "game/MeshTemplate.h"

namespace editor {

// Modal dialog for selecting a loaded MeshTemplate before click-to-place.
//
// Call Open() to schedule the popup; the next Render() call will open it via
// ImGui::OpenPopup(). Render() returns the confirmed template on OK, or
// nullptr when the modal is closed, cancelled, or not yet open.
class MeshSelectionModal {
 public:
  // Schedules the modal to open on the next Render() call.
  void Open();

  // Draws the modal if scheduled. Returns the confirmed MeshTemplate* or nullptr.
  game::MeshTemplate* Render();

 private:
  bool                is_open_  = false;
  game::MeshTemplate* selected_ = nullptr;
};

}  // namespace editor
