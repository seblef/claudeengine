#version 460 core

#include <layouts/vertex_3d.glsl>
#include <uniforms/renderable_infos.glsl>
#include <uniforms/shadow_pass_infos.glsl>

// Transform vertex to light clip space.
// gl_Position is the only output; the fragment shader writes depth implicitly.
void main() {
    gl_Position = light_vp * world * vec4(in_position, 1.0);
}
