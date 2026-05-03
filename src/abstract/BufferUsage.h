#pragma once

#include <cstdint>

namespace abstract {

// Describes how a GPU buffer will be accessed after creation.
enum class BufferUsage : uint8_t {
  kDynamic,    // Updated frequently (e.g., per-frame particle data)
  kImmutable,  // Written once at creation, never updated
  kStaging,    // CPU-writable staging buffer for transfer to GPU memory
};

}  // namespace abstract
