#pragma once

namespace editor {

// Interface for a panel that edits a single resource descriptor file.
//
// Concrete implementations (VehiclePanel, VFXPanel, …) are created by the
// ResourcePanelRegistry when the user opens a file in the ResourceBrowser.
// Each open file gets its own IResourcePanel instance displayed as an ImGui tab.
class IResourcePanel {
 public:
  virtual void Draw()          = 0;
  virtual bool IsDirty() const = 0;
  virtual void Save()          = 0;
  virtual ~IResourcePanel()    = default;
};

}  // namespace editor
