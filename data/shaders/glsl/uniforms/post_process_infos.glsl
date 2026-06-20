// PostProcessInfos — GPU reads only this first float4 (16 bytes).
// The CPU-side struct is 32 bytes; bytes [16..31] hold CPU-only eye-adaptation
// constants uploaded for editor convenience and are not declared or read here.
// std140 offsets used by shaders:
//   [  0] u_exposure        float  (HDR pre-scale before tone mapping; 1.0 = neutral)
//   [  4] u_bloom_intensity float  (additive bloom blend weight; 0 = no bloom)
//   [  8] u_bloom_threshold float  (luminance threshold for bloom extraction)
//   [ 12] u_adapt_speed     float  (eye adaptation lerp speed; unused when eye adaptation off)
layout(std140, binding = 10) uniform PostProcessInfos {
    float u_exposure;        // HDR pre-scale before tone mapping (1.0 = neutral)
    float u_bloom_intensity; // additive bloom blend weight (0 = no bloom)
    float u_bloom_threshold; // luminance threshold for bloom extraction
    float u_adapt_speed;     // eye adaptation lerp speed (unused when eye adaptation off)
};
