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

// Maps a compound extension suffix (e.g. ".vehicle.yaml") to a factory that
// produces an IResourcePanel for a given file path, and optionally to a
// factory that creates a new skeleton file on disk.
//
// Usage:
//   registry.Register(".vehicle.yaml", [](auto path) {
//     return std::make_unique<VehiclePanel>(std::move(path));
//   });
//   registry.RegisterNew(".vehicle.yaml", [](std::string_view name) {
//     // write skeleton file, return its path
//   });
//   auto panel = registry.Open("data/vehicles/truck.vehicle.yaml");
class ResourcePanelRegistry {
 public:
  ResourcePanelRegistry() = default;

  // Registers a factory for files whose filename ends with extension.
  // extension must start with '.', e.g. ".vehicle.yaml".
  void Register(std::string_view extension, PanelFactory factory);

  // Registers a new-file factory for extension.
  // Only one new-file factory per extension is supported; a second call
  // overwrites the first.
  void RegisterNew(std::string_view extension, NewFileFactory factory);

  // Returns a new panel for path if a factory is registered for its extension
  // suffix, or nullptr otherwise.
  [[nodiscard]] std::unique_ptr<IResourcePanel> Open(
      const std::filesystem::path& path) const;

  // Returns true if a factory is registered for path's extension suffix.
  [[nodiscard]] bool CanOpen(const std::filesystem::path& path) const;

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
  // Kept in insertion order so the browser can display sections consistently.
  std::vector<std::string> extensions_;
};

}  // namespace editor
