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

// Maps a compound extension suffix (e.g. ".vehicle.yaml") to a factory that
// produces an IResourcePanel for a given file path.
//
// Usage:
//   registry.Register(".vehicle.yaml", [](auto path) {
//     return std::make_unique<VehiclePanel>(std::move(path));
//   });
//   auto panel = registry.Open("data/vehicles/truck.vehicle.yaml");
class ResourcePanelRegistry {
 public:
  ResourcePanelRegistry() = default;

  // Registers a factory for files whose filename ends with extension.
  // extension must start with '.', e.g. ".vehicle.yaml".
  void Register(std::string_view extension, PanelFactory factory);

  // Returns a new panel for path if a factory is registered for its extension
  // suffix, or nullptr otherwise.
  [[nodiscard]] std::unique_ptr<IResourcePanel> Open(
      const std::filesystem::path& path) const;

  // Returns true if a factory is registered for path's extension suffix.
  [[nodiscard]] bool CanOpen(const std::filesystem::path& path) const;

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
  std::unordered_map<std::string, PanelFactory> factories_;
  // Kept in insertion order so the browser can display sections consistently.
  std::vector<std::string> extensions_;
};

}  // namespace editor
