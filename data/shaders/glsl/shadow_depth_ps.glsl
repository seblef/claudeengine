#version 460 core

// Depth is written implicitly by the rasterizer from gl_Position.z.
// No color outputs — this shader is used with a depth-only FBO.
// When alpha_mask is set, fragments with diffuse alpha < 0.5 are discarded
// so that cutout geometry casts correct shadow silhouettes.

#include <uniforms/material_infos.glsl>

layout(binding = 0) uniform sampler2D u_diffuse;

in vec2 v_uv;

void main() {
    if (alpha_mask != 0 && texture(u_diffuse, v_uv).a < 0.5) discard;
}
