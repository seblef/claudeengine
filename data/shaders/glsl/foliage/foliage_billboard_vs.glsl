// Foliage billboard vertex shader — camera-facing quad instances.
//
// Per-instance data (xyz=world position, w=uniform scale) is read from the
// SSBO at binding 1.  The quad vertices from in_position (x ∈ [−0.5, 0.5],
// y ∈ [0, 1]) are expanded in world space using the camera right and up
// vectors extracted from the row-major view matrix.
//
// Billboards are cylindrical: they rotate around world Y but never tilt.

#version 460 core
#include <layouts/vertex_3d.glsl>
#include <uniforms/scene_infos.glsl>

// Per-instance position + scale uploaded by FoliageRenderer::ClassifyAndUpload().
layout(std430, binding = 1) readonly buffer FoliageBillboards {
    vec4 pos_scale[];
};

out vec2 v_uv;

void main() {
    vec4  inst      = pos_scale[gl_InstanceID];
    vec3  world_pos = inst.xyz;
    float scale     = inst.w;

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
