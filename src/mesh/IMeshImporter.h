#pragma once

#include <string>

#include "mesh/MeshData.h"

namespace mesh {

// Abstract interface implemented by all mesh importers (OBJ, FBX, …).
// Each importer converts a source file into a MeshData suitable for
// further processing (normal/tangent recomputation, vertex welding) before
// being serialised to .emesh or uploaded to the GPU.
class IMeshImporter {
 public:
  virtual ~IMeshImporter() = default;

  // Parses the file at path and populates mesh. Returns true on success.
  virtual bool Import(const std::string& path, MeshData* mesh) const = 0;
};

}  // namespace mesh
