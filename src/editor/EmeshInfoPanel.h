#pragma once

#include <filesystem>

#include "editor/IResourcePanel.h"
#include "mesh/MeshData.h"

namespace editor {

// Read-only IResourcePanel that displays the header fields of a .emesh file:
// vertex count, index count, AABB, and the per-submesh material paths.
// IsDirty() always returns false; Save() is a no-op.
class EmeshInfoPanel : public IResourcePanel {
 public:
  explicit EmeshInfoPanel(std::filesystem::path path);

  void Draw()          override;
  bool IsDirty() const override { return false; }
  void Save()          override {}

 private:
  std::filesystem::path path_;
  // cppcheck-suppress unusedStructMember
  bool                  loaded_ = false;
  // cppcheck-suppress unusedStructMember
  bool                  error_  = false;
  // cppcheck-suppress unusedStructMember
  mesh::MeshData        data_;
};

}  // namespace editor
