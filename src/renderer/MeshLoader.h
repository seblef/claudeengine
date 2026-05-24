#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "abstract/VideoDevice.h"
#include "core/Vec3f.h"
#include "renderer/GeometryData.h"

namespace renderer {

// CPU-side geometry retained after GPU upload, used for ray-triangle picking.
struct CPUMeshData {
  // cppcheck-suppress unusedStructMember
  std::vector<core::Vec3f> positions;  // one per vertex
  // cppcheck-suppress unusedStructMember
  std::vector<uint32_t>    indices;    // 3 per triangle
};

// Combined result of a mesh load: GPU geometry + CPU positions/indices.
struct MeshLoadResult {
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<GeometryData> geometry;
  // cppcheck-suppress unusedStructMember
  CPUMeshData                   cpu;
};

// Loads a mesh file from disk and returns GPU-ready geometry.
//
// The format is selected automatically from the file extension:
//   .obj   — Wavefront OBJ (via ObjImporter)
//   .fbx   — Autodesk FBX (via FbxImporter)
//   .emesh — engine binary format (via EmeshReader)
//
// Returns nullptr / nullopt if the file cannot be found, parsed, or yields no
// geometry, or if the vertex count exceeds the 16-bit index limit (65 535).
class MeshLoader {
 public:
  // Loads geometry and retains CPU-side positions + indices for ray picking.
  [[nodiscard]] static std::optional<MeshLoadResult> Load(
      const std::string& path, abstract::VideoDevice* video);

  // Thin wrapper around Load() for callers that only need GPU geometry.
  [[nodiscard]] static std::unique_ptr<GeometryData> LoadGeometry(
      const std::string& path,
      abstract::VideoDevice* video);
};

}  // namespace renderer
