#pragma once

#include <string>
#include <vector>

namespace editor {

// Modal dialog for selecting a .vehicle.yaml template before click-to-place.
//
// Call Open() to schedule the popup; the next Render() call will open it via
// ImGui::OpenPopup(). The modal scans data/vehicles/ for *.vehicle.yaml files
// each time Open() is called. Render() returns the chosen template name
// (basename without extension, e.g. "duke") on OK, or an empty string when
// the modal is closed, cancelled, or not yet open.
//
// Pass a unique popup_id to each instance so their ImGui popup IDs do not
// collide when multiple VehicleSelectionModal instances exist in the same frame.
class VehicleSelectionModal {
 public:
  explicit VehicleSelectionModal(const char* popup_id = "Select Vehicle");

  // Scans data/vehicles/ and schedules the popup to open on the next Render().
  void Open();

  // Draws the modal if scheduled. Returns the confirmed template name or "".
  std::string Render();

 private:
  // cppcheck-suppress unusedStructMember
  const char*              popup_id_;
  bool                     is_open_  = false;
  int                      selected_ = -1;
  // cppcheck-suppress unusedStructMember
  std::vector<std::string> entries_;  // basenames without extension
};

}  // namespace editor
