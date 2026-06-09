#pragma once

#include "mesh/LodData.h"

namespace mesh {

// Top-level mesh asset: a single flat geometry (one LOD).
//
// `unit_meters` carries the source-file unit scale set by the importer (e.g.
// 0.01 for centimetre-authored FBX files).  Defaults to 1.0 (metres).  The
// import UI uses this to pre-fill the scale field; the conversion is never
// applied silently by the importer itself.
struct MeshData {
  LodData lod;                  // cppcheck-suppress unusedStructMember
  float   unit_meters = 1.0f;  // cppcheck-suppress unusedStructMember
};

}  // namespace mesh
