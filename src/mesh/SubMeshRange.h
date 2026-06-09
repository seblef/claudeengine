#pragma once

#include <cstdint>
#include <string>

namespace mesh {

// Describes a contiguous range of indices in the shared index buffer that
// belongs to a single material slot.  When a LodData carries one or more
// SubMeshRanges the renderer must issue a separate draw call (or equivalent)
// for each range, binding the corresponding material.
struct SubMeshRange {
  uint32_t index_offset;    // cppcheck-suppress unusedStructMember
  uint32_t index_count;     // cppcheck-suppress unusedStructMember
  std::string material_name;  // cppcheck-suppress unusedStructMember
};

}  // namespace mesh
