#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "editor/IResourcePanel.h"

namespace editor {

class ResourcePanelRegistry;

// "Browser" tab inside the Scene panel.
//
// Scans data/ recursively on construction and on every Refresh click; lists
// only files whose extension suffix is registered in the ResourcePanelRegistry.
// Displays one collapsible section per registered extension. Double-clicking
// an entry opens (or focuses) a panel tab below the tree view.
//
// Dirty-close protection mirrors EditorWindow::RenderUnsavedChangesModal:
// closing a dirty tab triggers a Save / Discard / Cancel modal.
// Ctrl+S while a resource panel tab is focused calls panel->Save().
class ResourceBrowser {
 public:
  explicit ResourceBrowser(ResourcePanelRegistry* registry);

  // Renders the full browser UI (tree + open panel tabs).
  void Render();

 private:
  struct OpenPanel {
    std::filesystem::path          canonical_path;
    // cppcheck-suppress unusedStructMember
    std::string                    display_name;
    std::unique_ptr<IResourcePanel> panel;
    bool                            want_close = false;
  };

  // Scans data/ and rebuilds file_map_.
  void Scan();

  // Returns the index into open_panels_ for canonical_path, or -1.
  int FindOpenPanel(const std::filesystem::path& canonical_path) const;

  // Opens a panel for path or focuses its existing tab.
  void OpenOrFocus(const std::filesystem::path& path);

  // Renders the collapsible tree of discovered files.
  void RenderTree();

  // Renders the tab bar of open panels below the tree.
  void RenderPanelTabs();

  // Renders the dirty-close confirmation modal.
  void RenderDirtyCloseModal();

  // Renders the "New <type>" name-input modal and handles creation.
  void RenderNewFileModal();

  // cppcheck-suppress unusedStructMember
  ResourcePanelRegistry* registry_;

  // extension suffix → list of discovered paths
  // cppcheck-suppress unusedStructMember
  std::unordered_map<std::string, std::vector<std::filesystem::path>> file_map_;

  // cppcheck-suppress unusedStructMember
  std::vector<OpenPanel> open_panels_;
  int                    active_tab_       = -1;

  // Index into open_panels_ of the tab pending a dirty-close confirmation,
  // or -1 when no modal is open.
  int                    pending_close_idx_ = -1;

  bool                   open_dirty_modal_ = false;

  // State for the "New Resource" name-input modal.
  // cppcheck-suppress unusedStructMember
  std::string            pending_new_ext_;
  // cppcheck-suppress unusedStructMember
  char                   new_name_buf_[128] = {};
  // cppcheck-suppress unusedStructMember
  bool                   open_new_modal_    = false;
  // Non-empty when the last Create() call failed; shown as red text in modal.
  // cppcheck-suppress unusedStructMember
  std::string            new_error_msg_;
};

}  // namespace editor
