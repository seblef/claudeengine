// Cloud layer vertex shader — passthrough identical to sky_vs.
// Places the fullscreen quad at the far plane (NDC z = 1.0) and reconstructs
// a world-space view ray per vertex for the fragment shader.
//
// UBO binding 2: scene_infos (inv_view_proj, eye_pos)

#version 460 core
#include <layouts/vertex_3d.glsl>
#include <uniforms/scene_infos.glsl>

out vec3 v_view_ray;

void main() {
    gl_Position = vec4(in_position.xy, 1.0, 1.0);

    vec4 world_far = inv_view_proj * vec4(in_position.xy, 1.0, 1.0);
    world_far /= world_far.w;
    v_view_ray = normalize(world_far.xyz - eye_pos);
}
