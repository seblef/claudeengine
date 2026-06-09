#pragma once

#include <string>

#include "mesh/IMeshImporter.h"
#include "mesh/MeshData.h"

namespace mesh {

// Imports Wavefront OBJ files using the fast_obj library.
//
// Geometry is split into one SubMeshRange per contiguous material run using
// fast_obj `face_materials`; all faces sharing the same material name between
// two `usemtl` directives are written into a contiguous index range within a
// single interleaved vertex buffer.  If the file provides no normals they are
// recomputed via ComputeNormals; tangents are always recomputed via
// ComputeTangents after vertex welding.
//
// OBJ files with no `.mtl` reference or a single material produce a single
// SubMeshRange.
class ObjImporter : public IMeshImporter {
 public:
  bool Import(const std::string& path, MeshData* mesh) const override;
};

}  // namespace mesh
