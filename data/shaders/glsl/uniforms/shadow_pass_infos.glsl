// ShadowPassInfosBlock — must match renderer::ShadowPassInfos exactly (64 bytes, 4 float4s).
// std140 offsets:
//   [  0] light_vp  mat4
layout(std140, row_major, binding = 6) uniform ShadowPassInfosBlock {
    mat4 light_vp;
};
