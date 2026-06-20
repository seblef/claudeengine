#pragma once

namespace renderer {

// CPU/GPU layout for post-processing parameters (UBO slot 10).
//
// The GPU shader reads only the first 16 bytes (one float4).  The second float4
// holds CPU-side eye-adaptation tuning constants that are uploaded for editor
// convenience but are never accessed by any shader.
//
// GPU offset map (post_process_infos.glsl):
//   [  0] exposure        float  (HDR pre-scale before tone mapping; 1.0 = neutral)
//   [  4] bloom_intensity float  (additive bloom blend weight; 0 = no bloom)
//   [  8] bloom_threshold float  (luminance threshold for bloom extraction)
//   [ 12] adapt_speed     float  (eye adaptation lerp speed; unused when disabled)
// CPU-only (not read by any shader):
//   [ 16] eye_key          float  (middle-grey key; target = key / avg_lum)
//   [ 20] eye_min_exposure float  (lower exposure clamp)
//   [ 24] eye_max_exposure float  (upper exposure clamp)
//   [ 28] _pad             float
//   [ 32] end
struct alignas(16) PostProcessInfos {
  float exposure         = 1.0f;
  float bloom_intensity  = 0.8f;
  float bloom_threshold  = 1.0f;
  float adapt_speed      = 1.5f;
  // CPU-only eye-adaptation parameters.
  // eye_max_exposure = eye_key replicates the RGBA8 luminance-buffer behaviour:
  // dark scenes (avg_lum < 1) always drove target_exp ≥ key, clamped to key.
  float eye_key          = 0.18f;
  float eye_min_exposure = 0.1f;
  float eye_max_exposure = 0.18f;
  float _pad             = 0.f;
};

static_assert(sizeof(PostProcessInfos) == 32);

}  // namespace renderer
