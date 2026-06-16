// Foliage billboard vertex shader — camera-facing quad instances.
//
// Per-instance data (xyz=world position, w=uniform scale) is read from the
// SSBO at binding 1.  The quad vertices from in_position (x ∈ [−0.5, 0.5],
// y ∈ [0, 1]) are expanded in world space using the camera right and up
// vectors extracted from the row-major view matrix.
//
// Billboards are cylindrical: they rotate around world Y but never tilt.
//
// Wind sway: a sinusoidal XZ displacement is applied to the instance root
// position, scaled by in_position.y so the base stays planted:
//   sway = sin(world_pos.x * 1.5 + wind_time * 1.8) * in_position.y
//          * u_sway_strength * wind_strength / 10.0
//   root.xz += sway * normalize(wind_xz)

#version 460 core
#include <layouts/vertex_3d.glsl>
#include <uniforms/scene_infos.glsl>
#include <uniforms/wind_infos.glsl>

// Per-instance position + scale uploaded by FoliageRenderer::ClassifyAndUpload().
layout(std430, binding = 1) readonly buffer FoliageBillboards {
    vec4 pos_scale[];
};

// Per-layer sway amplitude set by FoliageRenderer::RenderBillboards().
uniform float u_sway_strength;

out vec2 v_uv;

void main() {
    vec4  inst      = pos_scale[gl_InstanceID];
    vec3  world_pos = inst.xyz;
    float scale     = inst.w;

    // Sway root in XZ, height-weighted so the base stays planted.
    float sway    = sin(world_pos.x * 1.5 + wind_time * 1.8)
                    * in_position.y * u_sway_strength * wind_strength / 10.0;
    vec2 wind_dir = (wind_strength > 0.0)
                    ? normalize(wind_xz)
                    : vec2(1.0, 0.0);
    world_pos.x += sway * wind_dir.x;
    world_pos.z += sway * wind_dir.y;

    // Extract camera right from the view matrix (row_major: view[col][row]).
    // Row 0 of the view = right axis → elements (view[0][0], view[1][0], view[2][0]).
    vec3 cam_right = normalize(vec3(view[0][0], view[1][0], view[2][0]));

    // Cylindrical billboard: use world-up instead of camera-up to prevent tilt.
    vec3 cam_up = vec3(0.0, 1.0, 0.0);

    // Expand the unit quad into world space around world_pos.
    vec3 vertex_pos = world_pos
                    + cam_right * in_position.x * scale
                    + cam_up    * in_position.y * scale;

    gl_Position = view_proj * vec4(vertex_pos, 1.0);
    v_uv        = in_uv;
}
