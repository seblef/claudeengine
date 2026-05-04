#pragma once

#include <cstdint>

namespace abstract {

// Element type stored in an index buffer.
enum class IndexType : uint8_t {
  kUInt16,  // 16-bit indices; supports up to 65535 unique vertices per draw call
  kUInt32,  // 32-bit indices; supports up to ~4 billion unique vertices
};

}  // namespace abstract
