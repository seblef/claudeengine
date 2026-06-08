// PostProcessInfos — must match renderer::PostProcessInfos exactly (16 bytes, 1 float4).
// std140 offsets:
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
