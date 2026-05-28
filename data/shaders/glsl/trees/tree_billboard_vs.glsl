// Tree billboard vertex shader — camera-facing quad instances.
//
// Per-instance data (xyz=world pos, w=scale) is read from the SSBO at
// binding 3. The unit quad (x ∈ [−0.5, 0.5], y ∈ [0, 1]) is expanded in
// world space using the camera right vector and the world-up axis (cylindrical
// billboard — rotates around Y, never tilts forward/backward).

#version 460 core
#include <layouts/vertex_3d.glsl>
#include <uniforms/scene_infos.glsl>

// Per-instance position + scale (binding 3).
layout(std430, binding = 3) readonly buffer TreeBillboards {
    vec4 pos_scale[];
};

out vec2 v_uv;

void main() {
    vec4  inst      = pos_scale[gl_InstanceID];
    vec3  world_pos = inst.xyz;
    float scale     = inst.w;

    // Extract camera right from the view matrix (row-major: column 0 = right).
    vec3 cam_right = normalize(vec3(view[0][0], view[1][0], view[2][0]));

    // Cylindrical billboard: world up prevents tilt toward/away from camera.
    vec3 cam_up = vec3(0.0, 1.0, 0.0);

    vec3 vertex_world = world_pos
                      + cam_right * in_position.x * scale
                      + cam_up    * in_position.y * scale;

    gl_Position = view_proj * vec4(vertex_world, 1.0);
    v_uv        = in_uv;
}
