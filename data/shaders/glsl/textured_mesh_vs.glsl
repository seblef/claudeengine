// Vertex shader for textured 3D geometry.
// UBO binding 0: world matrix; binding 1: camera view-projection matrix.
// Both are uploaded as row-major C++ Mat4f — the row_major qualifier tells
// the GPU to read memory row-by-row, matching the engine's Mat4f convention.

#version 460 core

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec3 in_binormal;
layout(location = 3) in vec3 in_tangent;
layout(location = 4) in vec2 in_uv;

layout(std140, row_major, binding = 0) uniform WorldBlock  { mat4 world;     };
layout(std140, row_major, binding = 1) uniform CameraBlock { mat4 view_proj; };

out vec2 v_uv;

void main() {
    gl_Position = view_proj * world * vec4(in_position, 1.0);
    v_uv = in_uv;
}
