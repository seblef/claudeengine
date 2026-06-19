#pragma once

#include <string>
#include <vector>

namespace editor {

// Modal dialog for selecting a .sound.yaml template before click-to-place.
//
// Call Open() to schedule the popup; the next Render() call opens it via
// ImGui::OpenPopup(). The modal scans data/sounds/ for *.sound.yaml files
// each time Open() is called. Render() returns the chosen template name
// (basename without the .sound.yaml extension) on OK, or an empty string when
// the modal is closed, cancelled, or not yet open.
class SoundEmitterSelectionModal {
 public:
  // Scans data/sounds/ and schedules the popup to open on the next Render().
  void Open();

  // Draws the modal if scheduled. Returns the confirmed template name or "".
  std::string Render();

 private:
  bool                     is_open_  = false;
  int                      selected_ = -1;
  // cppcheck-suppress unusedStructMember
  std::vector<std::string> entries_;  // basenames without .sound.yaml extension
};

}  // namespace editor
