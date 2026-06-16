#version 460 core

// Fragment shader for omni-light cube shadow depth pass.
//
// Writes linear distance from the light (normalised to [0,1] by range) as
// gl_FragDepth so that the cube shadow map stores values directly comparable
// with the "length(light_dir) / range" expression in omni_light_ps.glsl.
// The default hardware depth (non-linear NDC) must NOT be used here: it is
// based on projected-z, which differs from 3-D distance and would produce
// incorrect shadow comparisons.
// When alpha_mask is set, fragments with diffuse alpha < 0.5 are discarded
// so that cutout geometry casts correct shadow silhouettes for omni lights.

#include <uniforms/material_infos.glsl>
#include <uniforms/shadow_pass_infos.glsl>

layout(binding = 0) uniform sampler2D u_diffuse;

in vec3 v_world_pos;
in vec2 v_uv;

void main() {
    if (alpha_mask != 0 && texture(u_diffuse, v_uv).a < 0.5) discard;
    // Equation: linear_depth = distance(fragment, light) / light_range
    gl_FragDepth = length(v_world_pos - light_pos_range.xyz) / light_pos_range.w;
}
