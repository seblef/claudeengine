// CSMInfosBlock — must match renderer::CSMInfos exactly (272 bytes, 17 float4s).
// std140 offsets with row_major matrices:
//   [  0] cascade_vp[0..3]   4 × mat4  light-space view-projection per cascade
//   [256] split_distances     vec4      view-space far depth per cascade (x=far0, y=far1, ...)
layout(std140, row_major, binding = 5) uniform CSMInfosBlock {
    mat4 cascade_vp[4];
    vec4 split_distances;  // x=far0, y=far1, z=far2, w=far3 (view space)
};
