#pragma once

#include <string>

#include "mesh/IMeshImporter.h"
#include "mesh/MeshData.h"

namespace mesh {

// Imports Wavefront OBJ files using the fast_obj library.
//
// All OBJ groups are merged into a single flat LodData. Material slots are
// ignored — materials are a game-level concept. If the file provides no
// normals, they are recomputed via ComputeNormals. Tangents are always
// recomputed via ComputeTangents after vertex welding.
class ObjImporter : public IMeshImporter {
 public:
  bool Import(const std::string& path, MeshData* mesh) const override;
};

}  // namespace mesh
