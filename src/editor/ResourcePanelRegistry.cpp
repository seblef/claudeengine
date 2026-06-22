#include "editor/ResourcePanelRegistry.h"

#include <algorithm>
#include <string>

namespace editor {

void ResourcePanelRegistry::Register(std::string_view extension,
                                     PanelFactory factory) {
  const std::string key(extension);
  if (factories_.count(key) == 0 && open_callbacks_.count(key) == 0)
    extensions_.push_back(key);
  factories_[key] = std::move(factory);
}

void ResourcePanelRegistry::RegisterOpenCallback(std::string_view extension,
                                                 OpenCallback callback) {
  const std::string key(extension);
  if (open_callbacks_.count(key) == 0 && factories_.count(key) == 0)
    extensions_.push_back(key);
  open_callbacks_[key] = std::move(callback);
}

void ResourcePanelRegistry::RegisterNew(std::string_view extension,
                                        NewFileFactory factory) {
  new_factories_[std::string(extension)] = std::move(factory);
}

bool ResourcePanelRegistry::CanCreate(std::string_view extension) const {
  return new_factories_.count(std::string(extension)) > 0;
}

std::filesystem::path ResourcePanelRegistry::Create(
    std::string_view extension, std::string_view name) const {
  const auto it = new_factories_.find(std::string(extension));
  if (it == new_factories_.end()) return {};
  return it->second(name);
}

std::unique_ptr<IResourcePanel> ResourcePanelRegistry::Open(
    const std::filesystem::path& path) const {
  const std::string ext = MatchExtension(path);
  if (ext.empty()) return nullptr;
  const auto it = factories_.find(ext);
  if (it == factories_.end()) return nullptr;
  return it->second(path);
}

bool ResourcePanelRegistry::CanOpen(
    const std::filesystem::path& path) const {
  return !MatchExtension(path).empty();
}

bool ResourcePanelRegistry::HasOpenCallback(
    const std::filesystem::path& path) const {
  const std::string ext = MatchExtension(path);
  return !ext.empty() && open_callbacks_.count(ext) > 0;
}

void ResourcePanelRegistry::InvokeOpenCallback(
    const std::filesystem::path& path) const {
  const std::string ext = MatchExtension(path);
  const auto it = open_callbacks_.find(ext);
  if (it != open_callbacks_.end())
    it->second(path);
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
