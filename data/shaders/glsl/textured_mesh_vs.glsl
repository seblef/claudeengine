// Vertex shader for textured 3D geometry.
// UBO binding 0: world matrix (row-major Mat4f).
// UBO binding 1: per-frame scene infos (renderer::SceneInfos, 352 bytes, std140 row-major).
//   Only view_proj is consumed here; remaining fields are available for other shaders.

#version 460 core

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec3 in_binormal;
layout(location = 3) in vec3 in_tangent;
layout(location = 4) in vec2 in_uv;

layout(std140, row_major, binding = 0) uniform WorldBlock {
    mat4 world;
};

// Full SceneInfos layout — must match renderer::SceneInfos exactly (352 bytes, 22 float4s).
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
layout(std140, row_major, binding = 1) uniform SceneInfosBlock {
    mat4  view_proj;
    mat4  inv_view_proj;
    mat4  inv_proj;
    mat4  proj;
    mat4  view;
    vec3  eye_pos;
    float time;
    float pad0_;
    vec2  inv_screen_size;
};

out vec2 v_uv;

void main() {
    // clip = view_proj * world * position
    gl_Position = view_proj * world * vec4(in_position, 1.0);
    v_uv = in_uv;
}
