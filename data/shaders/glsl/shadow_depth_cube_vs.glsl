#version 460 core

#include <layouts/vertex_3d.glsl>
#include <uniforms/renderable_infos.glsl>
#include <uniforms/shadow_pass_infos.glsl>

// World-space position forwarded to the fragment shader so it can compute
// the exact distance from the light origin (light_pos_range.xyz).
// UV forwarded for alpha-mask cutout testing.
out vec3 v_world_pos;
out vec2 v_uv;

void main() {
    vec4 world_h = world * vec4(in_position, 1.0);
    v_world_pos  = world_h.xyz;
    v_uv         = in_uv;
    gl_Position  = light_vp * world_h;
}
