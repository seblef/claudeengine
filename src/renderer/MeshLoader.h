#pragma once

#include <memory>
#include <string>

#include "abstract/VideoDevice.h"
#include "renderer/GeometryData.h"

namespace renderer {

// Loads a mesh file from disk and returns GPU-ready geometry.
//
// The format is selected automatically from the file extension:
//   .obj   — Wavefront OBJ (via ObjImporter)
//   .fbx   — Autodesk FBX (via FbxImporter)
//   .emesh — engine binary format (via EmeshReader)
//
// Returns nullptr if the file cannot be found, parsed, or yields no geometry,
// or if the vertex count exceeds the 16-bit index limit (65 535).
class MeshLoader {
 public:
  [[nodiscard]] static std::unique_ptr<GeometryData> LoadGeometry(
      const std::string& path,
      abstract::VideoDevice* video);
};

}  // namespace renderer
