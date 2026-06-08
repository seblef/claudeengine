#pragma once

namespace renderer {

// GPU constant buffer layout for post-processing parameters (UBO slot 10).
// Matches layout(std140, binding=10) in post_process_infos.glsl exactly.
//
// Offset map:
//   [  0] exposure        float  (HDR pre-scale before tone mapping; 1.0 = neutral)
//   [  4] bloom_intensity float  (additive bloom blend weight; 0 = no bloom)
//   [  8] bloom_threshold float  (luminance threshold for bloom extraction)
//   [ 12] adapt_speed     float  (eye adaptation lerp speed; unused when disabled)
//   [ 16] end
struct alignas(16) PostProcessInfos {
  float exposure        = 1.0f;
  float bloom_intensity = 0.8f;
  float bloom_threshold = 1.0f;
  float adapt_speed     = 1.5f;
};

static_assert(sizeof(PostProcessInfos) == 16);

}  // namespace renderer
