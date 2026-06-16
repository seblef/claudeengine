#version 460 core

#include <layouts/vertex_3d.glsl>
#include <uniforms/renderable_infos.glsl>
#include <uniforms/shadow_pass_infos.glsl>

// UV forwarded to the fragment shader for alpha-mask cutout testing.
out vec2 v_uv;

// Transform vertex to light clip space.
// Depth is written implicitly by the rasterizer; UV is forwarded for alpha masking.
void main() {
    v_uv        = in_uv;
    gl_Position = light_vp * world * vec4(in_position, 1.0);
}
