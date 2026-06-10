#pragma once

#include "mesh/LodData.h"

namespace mesh {

// Recomputes per-vertex normals as area-weighted averages of incident face normals.
// Overwrites the normal field of every vertex. Requires a valid index buffer.
// Degenerate triangles (zero-area faces) are silently skipped.
void ComputeNormals(LodData* lod);

// Recomputes per-vertex tangents and binormals using Lengyel's method.
// Requires normals and UV coordinates to be set. Applies Gram-Schmidt
// orthogonalisation against the normal before storing.
// Degenerate UV triangles (zero UV area) are silently skipped.
void ComputeTangents(LodData* lod);

// Merges vertices that share identical (position, normal, UV) tuples and
// rebuilds the index buffer. Uses exact float equality — no epsilon.
// Call before ComputeTangents() to avoid tangent-space fragmentation.
void WeldVertices(LodData* lod);

// Merges all submeshes that share the same material name by concatenating
// their index ranges in the index buffer.  No-op when every material is
// already unique or when there are fewer than two submeshes.  Call after
// WeldVertices() and before ComputeTangents().
void MergeSubmeshesByMaterial(LodData* lod);

}  // namespace mesh
