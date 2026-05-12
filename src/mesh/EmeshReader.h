#pragma once

#include <string>

#include "mesh/MeshData.h"

namespace mesh {

// Deserialises a .emesh binary file into a MeshData.
// See EmeshWriter for the file layout specification.
class EmeshReader {
 public:
  // Reads the .emesh file at path and populates mesh. Returns true on success.
  bool Read(const std::string& path, MeshData* mesh) const;
};

}  // namespace mesh
