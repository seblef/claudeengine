#pragma once

#include <cstdint>

namespace abstract {

// Action applied to a stencil buffer value when a stencil/depth test passes or fails.
enum class StencilOp : uint8_t {
  kKeep,      // Keep the current stencil value.
  kZero,      // Set the stencil value to zero.
  kIncrWrap,  // Increment with wrap-around.
  kDecrWrap,  // Decrement with wrap-around.
};

}  // namespace abstract
