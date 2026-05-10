#pragma once

#include <cstdint>

namespace abstract {

// Comparison function used for depth and stencil tests.
enum class CompareFunc : uint8_t {
  kAlways,       // Test always passes.
  kNever,        // Test never passes.
  kEqual,        // Passes if ref == buffer.
  kNotEqual,     // Passes if ref != buffer.
  kLess,         // Passes if ref < buffer.
  kGreater,      // Passes if ref > buffer.
  kLessEqual,    // Passes if ref <= buffer.
  kGreaterEqual,  // Passes if ref >= buffer.
};

}  // namespace abstract
