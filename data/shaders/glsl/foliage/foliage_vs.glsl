// Foliage geometry-pass vertex shader — GPU instanced mesh rendering.
//
// Per-instance world matrix is fetched from the SSBO at binding 0 using
// gl_InstanceID.  The matrix layout is row_major and matches core::Mat4f.
//
// Outputs:
//   v_uv           — UV coordinates passed to the pixel shader.
//   v_normal_world — bent world-space normal blended toward world-up so that
//                    upright foliage sprites receive natural top-down lighting.
//
// Bent normal:
//   N_bent = normalize(lerp(N_geo, up, BENT_FACTOR))
//
// BENT_FACTOR = 0.6 was chosen so that sprites facing sideways still receive
// ~40 % of their geometric contribution while the dominant sky light lands
// correctly on horizontal foliage surfaces.
//
// Wind sway (horizontal XZ only):
//   y_norm = clamp(in_position.y / mesh_height, 0, 1)
//   sway   = sin(world_pos.x * 1.5 + wind_time * 1.8) * y_norm² * u_sway_strength
//            * wind_strength / 10.0
//   XZ displacement = sway * normalize(wind_xz)

#version 460 core
#include <layouts/vertex_3d.glsl>
#include <uniforms/scene_infos.glsl>
#include <uniforms/wind_infos.glsl>

// Per-instance world matrices uploaded by FoliageRenderer::ClassifyAndUpload().
layout(std430, row_major, binding = 0) readonly buffer FoliageInstances {
    mat4 world[];
};

// Per-layer sway parameters set by FoliageRenderer::Render().
uniform float u_mesh_height;    // mesh bounding-box Y range (object space)
uniform float u_sway_strength;  // peak sway amplitude at tip (metres)

out vec2 v_uv;
out vec3 v_normal_world;

const float BENT_FACTOR = 0.6;

void main() {
    mat4  w         = world[gl_InstanceID];
    vec4  world_pos = w * vec4(in_position, 1.0);

    // Height factor: quadratic ramp so the base stays planted.
    float y_norm = clamp(in_position.y / max(u_mesh_height, 0.01), 0.0, 1.0);
    float sway   = sin(world_pos.x * 1.5 + wind_time * 1.8)
                   * y_norm * y_norm * u_sway_strength * wind_strength / 10.0;

    vec2 wind_dir = (wind_strength > 0.0)
                    ? normalize(wind_xz)
                    : vec2(1.0, 0.0);
    world_pos.x += sway * wind_dir.x;
    world_pos.z += sway * wind_dir.y;

    gl_Position = view_proj * world_pos;
    v_uv        = in_uv;

    // Uniform scale only: normal transform = upper-left 3×3 of world matrix.
    vec3 geo_normal = normalize(mat3(w) * in_normal);
    // Blend toward world-up so upright foliage sprites catch top-down light.
    v_normal_world = normalize(mix(geo_normal, vec3(0.0, 1.0, 0.0), BENT_FACTOR));
}
