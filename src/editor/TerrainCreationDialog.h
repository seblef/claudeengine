#pragma once

namespace editor {

// Modal dialog for configuring and creating a new terrain.
//
// Call Open() to schedule the popup; the next Render() call will open it via
// ImGui::OpenPopup(). Render() returns true on the frame the user confirms.
// Call GetParams() after a true return to retrieve the parameters.
//
// The dialog is not a singleton — it is owned and driven by the calling panel
// via a bool flag.
class TerrainCreationDialog {
 public:
  TerrainCreationDialog() = default;

  // Schedules the modal to open on the next Render() call.
  void Open();

  // Draws the modal if scheduled. Returns true once on the frame the user
  // confirms; false otherwise.
  bool Render();

  // Parameters collected by the dialog.
  struct Params {
    float size_x     = 1000.f;  // World width in metres
    float size_z     = 1000.f;  // World depth in metres
    float resolution = 2.0f;    // Metres per heightmap texel
    float min_height = 0.f;     // World Y mapped to uint16 value 0
    float max_height = 100.f;   // World Y mapped to uint16 value 65535
  };

  [[nodiscard]] const Params& GetParams() const { return params_; }

 private:
  bool   is_open_ = false;
  Params params_;
};

}  // namespace editor
