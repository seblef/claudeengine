#pragma once

namespace audio {

// Distance-attenuation model for positional sound sources.
enum class RolloffType {
  kLinear,       // Gain decreases linearly between min and max distance
  kInverse,      // Gain follows inverse-distance law (physically correct)
  kExponential,  // Gain decreases exponentially with distance
};

}  // namespace audio
