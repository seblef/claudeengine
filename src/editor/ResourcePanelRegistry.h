#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "editor/IResourcePanel.h"

namespace editor {

using PanelFactory =
    std::function<std::unique_ptr<IResourcePanel>(std::filesystem::path)>;

// Called when the user creates a new resource with the given stem name.
// Must write the skeleton file and return its absolute path, or return an
// empty path on failure (e.g. name conflict, I/O error).
using NewFileFactory =
    std::function<std::filesystem::path(std::string_view name)>;

// Called when the user opens a resource file in the browser for an extension
// that has an external editor (e.g. VehicleEditorWindow). The callback is
// responsible for showing the editor window.
using OpenCallback = std::function<void(const std::filesystem::path&)>;

// Maps a compound extension suffix (e.g. ".vehicle.yaml") to either a factory
// that produces an IResourcePanel tab or an external OpenCallback, and
// optionally to a factory that creates a new skeleton file on disk.
//
// Extensions registered via RegisterOpenCallback open in an external window
// instead of a panel tab; they still appear in the ResourceBrowser tree and
// support the "New" button via RegisterNew.
//
// Usage (panel tab):
//   registry.Register(".material.yaml", [](auto path) {
//     return std::make_unique<MaterialPanel>(std::move(path));
//   });
//
// Usage (external window):
//   registry.RegisterOpenCallback(".vehicle.yaml", [](auto path) {
//     vehicle_editor_.Open(path);
//   });
//   registry.RegisterNew(".vehicle.yaml", [](std::string_view name) { ... });
//   auto panel = registry.Open("data/vehicles/truck.vehicle.yaml");
class ResourcePanelRegistry {
 public:
  ResourcePanelRegistry() = default;

  // Registers a panel-tab factory for files whose filename ends with
  // extension. extension must start with '.', e.g. ".material.yaml".
  void Register(std::string_view extension, PanelFactory factory);

  // Registers an external-window callback for extension. Files matching this
  // extension appear in the browser tree but open via callback instead of a
  // panel tab. A second call overwrites the first.
  void RegisterOpenCallback(std::string_view extension, OpenCallback callback);

  // Registers a new-file factory for extension.
  // Only one new-file factory per extension is supported; a second call
  // overwrites the first.
  void RegisterNew(std::string_view extension, NewFileFactory factory);

  // Returns a new panel for path if a panel-tab factory is registered for its
  // extension suffix, or nullptr otherwise.
  [[nodiscard]] std::unique_ptr<IResourcePanel> Open(
      const std::filesystem::path& path) const;

  // Returns true if path's extension suffix has a panel-tab factory or an
  // open callback registered (i.e. the file should appear in the browser tree).
  [[nodiscard]] bool CanOpen(const std::filesystem::path& path) const;

  // Returns true if an external open callback is registered for path's
  // extension suffix.
  [[nodiscard]] bool HasOpenCallback(const std::filesystem::path& path) const;

  // Invokes the external open callback for path. No-op if none is registered.
  void InvokeOpenCallback(const std::filesystem::path& path) const;

  // Returns true if a new-file factory is registered for extension.
  [[nodiscard]] bool CanCreate(std::string_view extension) const;

  // Calls the new-file factory for extension with name.
  // Returns the created file's absolute path, or an empty path on failure.
  // Returns an empty path if no new-file factory is registered for extension.
  [[nodiscard]] std::filesystem::path Create(std::string_view extension,
                                             std::string_view name) const;

  // Returns all registered extension suffixes, in registration order.
  [[nodiscard]] const std::vector<std::string>& GetExtensions() const {
    return extensions_;
  }

 private:
  // Returns the registered extension suffix that matches path's filename,
  // or an empty string if none match.
  [[nodiscard]] std::string MatchExtension(
      const std::filesystem::path& path) const;

  // cppcheck-suppress unusedStructMember
  std::unordered_map<std::string, PanelFactory>    factories_;
  // cppcheck-suppress unusedStructMember
  std::unordered_map<std::string, NewFileFactory>  new_factories_;
  // cppcheck-suppress unusedStructMember
  std::unordered_map<std::string, OpenCallback>    open_callbacks_;
  // Kept in insertion order so the browser can display sections consistently.
  std::vector<std::string> extensions_;
};

}  // namespace editor
