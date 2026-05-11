// SceneInfosBlock — must match renderer::SceneInfos exactly (368 bytes, 23 float4s).
// std140 offsets:
//   [  0] view_proj        mat4
//   [ 64] inv_view_proj    mat4
//   [128] inv_proj         mat4
//   [192] proj             mat4
//   [256] view             mat4
//   [320] eye_pos          vec3  (16-byte slot; 4th component unused)
//   [336] time             float
//   [340] pad0_            float (alignment gap before inv_screen_size)
//   [344] inv_screen_size  vec2
//   [352] z_near           float (camera near plane, for depth linearization)
//   [356] z_far            float (camera far plane,  for depth linearization)
//   [360] pad1_            float
//   [364] pad2_            float
layout(std140, row_major, binding = 2) uniform SceneInfosBlock {
    mat4  view_proj;
    mat4  inv_view_proj;
    mat4  inv_proj;
    mat4  proj;
    mat4  view;
    vec3  eye_pos;
    float time;
    float pad0_;
    vec2  inv_screen_size;
    float z_near;
    float z_far;
    float pad1_;
    float pad2_;
};
