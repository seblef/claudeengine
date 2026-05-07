// RenderableInfosBlock — must match renderer::RenderableInfos exactly (64 bytes, 4 float4s).
// std140 offsets:
//   [  0] world  mat4
layout(std140, row_major, binding = 1) uniform RenderableInfosBlock {
    mat4 world;
};
