#include "editor/ResourcePanelRegistry.h"

#include <algorithm>
#include <string>

namespace editor {

void ResourcePanelRegistry::Register(std::string_view extension,
                                     PanelFactory factory) {
  const std::string key(extension);
  if (factories_.count(key) == 0)
    extensions_.push_back(key);
  factories_[key] = std::move(factory);
}

std::unique_ptr<IResourcePanel> ResourcePanelRegistry::Open(
    const std::filesystem::path& path) const {
  const std::string ext = MatchExtension(path);
  if (ext.empty()) return nullptr;
  return factories_.at(ext)(path);
}

bool ResourcePanelRegistry::CanOpen(
    const std::filesystem::path& path) const {
  return !MatchExtension(path).empty();
}

std::string ResourcePanelRegistry::MatchExtension(
    const std::filesystem::path& path) const {
  const std::string filename = path.filename().string();
  const auto it = std::find_if(
      extensions_.begin(), extensions_.end(),
      [&filename](const std::string& ext) {
        return filename.size() >= ext.size() &&
               filename.compare(
                   filename.size() - ext.size(), ext.size(), ext) == 0;
      });
  return it != extensions_.end() ? *it : std::string{};
}

}  // namespace editor
