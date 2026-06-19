#pragma once

#include <filesystem>
#include <string>

#include "audio/SoundDesc.h"

namespace editor {

// Floating window for creating and editing .sound.yaml templates.
//
// Layout:
//   Toolbar row  — New / Load / Save / Save As file operations and filename.
//   Properties   — Editable fields for every SoundDesc attribute (file, loop,
//                  priority, rolloff, distances, volume, pitch).
//
// Usage:
//   Call Open() to show the window from the Audio menu.
//   Call OpenTemplate(name) to load a named template and bring up the window.
//   Call Render() every ImGui frame between NewFrame() and Render().
class SoundEditorWindow {
 public:
  SoundEditorWindow() = default;

  SoundEditorWindow(const SoundEditorWindow&)            = delete;
  SoundEditorWindow& operator=(const SoundEditorWindow&) = delete;

  // Opens (or re-focuses) the editor window with an empty template.
  void Open();

  // Loads the named template from data/sounds/<name>.sound.yaml and opens
  // the window. name is the basename without the .sound.yaml extension.
  void OpenTemplate(const std::string& name);

  // Renders the window. Must be called between ImGui::NewFrame() and
  // ImGui::Render() each frame when the window is visible.
  void Render();

  [[nodiscard]] bool IsOpen() const { return open_; }

 private:
  // Resets desc_ to defaults and clears the current path.
  void NewFile();

  // Opens a native file dialog filtered to data/sounds/ and loads the YAML.
  void LoadFromFile();

  // Saves to current_path_; falls through to SaveAs() when path is empty.
  void Save();

  // Opens a native save dialog and serialises the descriptor to the chosen path.
  void SaveAs();

  // Writes desc_ as a .sound.yaml file to path. Returns true on success.
  bool SerializeToFile(const std::filesystem::path& path);

  // Loads desc_ from path. Returns true on success.
  bool LoadFromPath(const std::filesystem::path& path);

  // Renders the toolbar row: New / Load / Save / Save As and filename hint.
  void RenderToolbar();

  // Renders editable property widgets for desc_.
  void RenderProperties();

  // cppcheck-suppress unusedStructMember
  bool open_    = false;
  // cppcheck-suppress unusedStructMember
  bool unsaved_ = false;

  // The descriptor being edited in-place by the property widgets.
  // cppcheck-suppress unusedStructMember
  audio::SoundDesc desc_;

  // Currently open file path; empty when the template has never been saved.
  // cppcheck-suppress unusedStructMember
  std::filesystem::path current_path_;
};

}  // namespace editor
