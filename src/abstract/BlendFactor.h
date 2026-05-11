#pragma once

namespace abstract {

// Blend factor for SetBlendEnabled(). Controls how src/dst contributions are
// weighted in the blending equation: result = src * src_factor + dst * dst_factor.
enum class BlendFactor {
  kZero,                // 0
  kOne,                 // 1
  kSrcAlpha,            // source alpha
  kOneMinusSrcAlpha,    // 1 - source alpha
};

}  // namespace abstract
