// Tree geometry-pass vertex shader — GPU instanced mesh with wind sway.
//
// Per-instance data (xyz=world pos, w=uniform scale) is read from the SSBO
// at binding 2. The instance transform is a uniform-scale translation, so the
// normal matrix is just the identity.
//
// Wind sway (horizontal XZ only):
//   vertex_y_norm = (vertex_y - bbox_min_y) / mesh_height  ∈ [0, 1]
//   trunk = sin(world_pos.x * 0.5 + wind_time) * vertex_y_norm * u_trunk_sway
//   leaf  = sin(world_pos.z * 1.3 + wind_time * 2.1)
//             * vertex_y_norm² * u_leaf_sway
//   XZ displacement = (trunk + leaf) * wind_strength / 10.0

#version 460 core
#include <layouts/vertex_3d.glsl>
#include <uniforms/scene_infos.glsl>
#include <uniforms/wind_infos.glsl>

// Per-instance position + scale (binding 2).
layout(std430, binding = 2) readonly buffer TreeInstances {
    vec4 pos_scale[];
};

// Per-layer sway parameters set by TreeRenderer::Render().
uniform float u_trunk_sway;
uniform float u_leaf_sway;
// Mesh bounding-box height used to normalise the vertex Y to [0, 1].
uniform float u_mesh_height;

out vec2 v_uv;
out vec3 v_normal_world;

void main() {
    vec4  inst       = pos_scale[gl_InstanceID];
    vec3  origin     = inst.xyz;
    float scale      = inst.w;

    // Build local → world: uniform scale + translation.
    vec3 world_pos = origin + in_position * scale;

    // vertex_y_norm in [0, 1] within the mesh bounding box.
    float y_norm = clamp(in_position.y / max(u_mesh_height, 0.01), 0.0, 1.0);

    // Trunk sway: sinusoidal offset proportional to height.
    float trunk = sin(world_pos.x * 0.5 + wind_time) * y_norm * u_trunk_sway;

    // Leaf flutter: higher frequency, quadratic height weight.
    float leaf = sin(world_pos.z * 1.3 + wind_time * 2.1)
                 * (y_norm * y_norm) * u_leaf_sway;

    // Scale sway by wind strength (normalised to moderate values around 10 m/s).
    float sway = (trunk + leaf) * wind_strength / 10.0;

    // Apply horizontal-only displacement along wind direction.
    vec2 wind_dir = (wind_strength > 0.0)
                    ? normalize(wind_xz)
                    : vec2(1.0, 0.0);
    world_pos.x += sway * wind_dir.x;
    world_pos.z += sway * wind_dir.y;

    gl_Position    = view_proj * vec4(world_pos, 1.0);
    v_uv           = in_uv;
    v_normal_world = normalize(in_normal);
}
