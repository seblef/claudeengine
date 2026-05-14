#version 460 core

// Fragment shader for omni-light cube shadow depth pass.
//
// Writes linear distance from the light (normalised to [0,1] by range) as
// gl_FragDepth so that the cube shadow map stores values directly comparable
// with the "length(light_dir) / range" expression in omni_light_ps.glsl.
// The default hardware depth (non-linear NDC) must NOT be used here: it is
// based on projected-z, which differs from 3-D distance and would produce
// incorrect shadow comparisons.

#include <uniforms/shadow_pass_infos.glsl>

in vec3 v_world_pos;

void main() {
    gl_FragDepth = length(v_world_pos - light_pos_range.xyz) / light_pos_range.w;
}
