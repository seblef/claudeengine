// ShadowPassInfosBlock — must match renderer::ShadowPassInfos exactly (80 bytes, 5 float4s).
// std140 offsets:
//   [  0] light_vp        mat4
//   [ 64] light_pos_range vec4  (xyz = world position, w = range; cube shadow pass only)
layout(std140, row_major, binding = 6) uniform ShadowPassInfosBlock {
    mat4 light_vp;
    vec4 light_pos_range;
};
