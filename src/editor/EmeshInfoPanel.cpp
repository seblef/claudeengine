#include "editor/EmeshInfoPanel.h"

#include <filesystem>
#include <string>

#include <imgui.h>

#include "core/BBox3.h"
#include "mesh/EmeshReader.h"
#include "mesh/SubMeshRange.h"

namespace editor {

EmeshInfoPanel::EmeshInfoPanel(std::filesystem::path path)
    : path_(std::move(path)) {
  mesh::EmeshReader reader;
  loaded_ = reader.Read(path_.string(), &data_);
  error_  = !loaded_;
}

void EmeshInfoPanel::Draw() {
  if (error_) {
    ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "Failed to read %s",
                       path_.filename().string().c_str());
    return;
  }

  const mesh::LodData& lod = data_.lod;

  ImGui::Text("Vertices : %u",
              static_cast<unsigned>(lod.vertices.size()));
  ImGui::Text("Indices  : %u",
              static_cast<unsigned>(lod.indices.size()));

  const core::Vec3f& mn = lod.aabb.GetMin();
  const core::Vec3f& mx = lod.aabb.GetMax();
  ImGui::Separator();
  ImGui::TextUnformatted("AABB");
  ImGui::Text("  Min : (%.3f, %.3f, %.3f)", mn.x, mn.y, mn.z);
  ImGui::Text("  Max : (%.3f, %.3f, %.3f)", mx.x, mx.y, mx.z);

  ImGui::Separator();
  if (lod.submeshes.empty()) {
    ImGui::TextDisabled("No submeshes (single-material)");
  } else {
    ImGui::Text("Submeshes: %u",
                static_cast<unsigned>(lod.submeshes.size()));
    for (unsigned i = 0; i < lod.submeshes.size(); ++i) {
      const mesh::SubMeshRange& sm = lod.submeshes[i];
      ImGui::Text("  [%u] %s  (offset=%u  count=%u)",
                  i,
                  sm.material_name.empty() ? "(none)" : sm.material_name.c_str(),
                  sm.index_offset,
                  sm.index_count);
    }
  }
}

}  // namespace editor
