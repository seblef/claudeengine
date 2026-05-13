#pragma once

#include <memory>
#include <string>

#include "abstract/VideoDevice.h"
#include "renderer/RenderableMesh.h"

namespace renderer {

// Loads a mesh file from disk and returns a GPU-ready RenderableMesh.
//
// The format is selected automatically from the file extension:
//   .obj   — Wavefront OBJ (via ObjImporter)
//   .fbx   — Autodesk FBX (via FbxImporter)
//   .emesh — engine binary format (via EmeshReader)
//
// Each submesh uses LOD 0. Submeshes with more than 65 535 vertices are
// skipped with an error log (GeometryData uses 16-bit indices).
//
// Materials are loaded from the mesh's material_slot names: each slot is
// treated as a path relative to data/materials/ without the file extension
// (e.g. "demo/checker" → data/materials/demo/checker.yaml). Submeshes with
// an empty material_slot fall back to the default material.
//
// Returns nullptr if the file cannot be found, parsed, or yields no geometry.
class MeshLoader {
 public:
  [[nodiscard]] static std::unique_ptr<RenderableMesh> Load(
      const std::string& path,
      abstract::VideoDevice* video);
};

}  // namespace renderer
