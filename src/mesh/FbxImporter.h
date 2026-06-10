#pragma once

#include <string>

#include "mesh/IMeshImporter.h"
#include "mesh/MeshData.h"

namespace mesh {

// Imports FBX files (binary and ASCII) using the ufbx library.
//
// Iterates scene nodes (not the raw mesh array) so that each node's
// `geometry_to_world` transform is baked into vertex positions and normals,
// correctly placing every submesh in world space.  Geometry is split into one
// SubMeshRange per material using ufbx `material_parts`; faces from all mesh
// nodes that share the same material name are written into contiguous index
// ranges within a single interleaved vertex buffer.  If the file provides no
// normals they are recomputed via ComputeNormals; tangents are always
// recomputed via ComputeTangents after vertex welding.
//
// The scene unit scale (e.g. 0.01 for centimetre-authored files) is written
// to `MeshData::unit_meters` so that callers can pre-fill the UI scale field.
// The conversion is intentionally not applied here.
class FbxImporter : public IMeshImporter {
 public:
  bool Import(const std::string& path, MeshData* mesh) const override;
};

}  // namespace mesh
