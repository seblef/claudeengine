#pragma once

#include <string>

#include "mesh/IMeshImporter.h"
#include "mesh/MeshData.h"

namespace mesh {

// Imports FBX files (binary and ASCII) using the ufbx library.
//
// Each non-empty material part of every mesh becomes one SubmeshData
// with a single LOD. If the file provides no normals, they are recomputed
// via ComputeNormals. Tangents are always recomputed via ComputeTangents
// after vertex welding.
class FbxImporter : public IMeshImporter {
 public:
  bool Import(const std::string& path, MeshData* mesh) const override;
};

}  // namespace mesh
