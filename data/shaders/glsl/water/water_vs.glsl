// Water vertex shader — forward pass (after deferred lighting).
//
// Displaces the flat XZ grid using Gerstner waves, which produce physically
// realistic circular particle orbits with cresting.
//
// Gerstner wave model (per wave i):
//   phi_i    = freq_i × dot(dir_i, xz) + speed_i × time
//   offset.x += Q_i × A_i × dir_i.x × cos(phi_i)
//   offset.y += A_i × sin(phi_i)
//   offset.z += Q_i × A_i × dir_i.y × cos(phi_i)
// where Q_i = 0.5 (steepness; 0 = pure sine, 1 = sharp crest).
//
// Analytical TBN from Gerstner derivatives:
//   N.x -= dir_i.x × w_i × A_i × cos(phi_i)
//   N.y -= Q_i × w_i × A_i × sin(phi_i)       (from 1.0 base)
//   N.z -= dir_i.y × w_i × A_i × cos(phi_i)
//
//   T.x -= Q_i × dir_i.x² × w_i × A_i × sin(phi_i)
//   T.y += dir_i.x × w_i × A_i × cos(phi_i)
//   T.z -= Q_i × dir_i.x × dir_i.y × w_i × A_i × sin(phi_i)
//
//   B.x -= Q_i × dir_i.x × dir_i.y × w_i × A_i × sin(phi_i)
//   B.y += dir_i.y × w_i × A_i × cos(phi_i)
//   B.z -= Q_i × dir_i.y² × w_i × A_i × sin(phi_i)
//
// Wave phase is driven by scene_infos.time (always advancing) so waves
// animate regardless of whether the wind system is active.
// Amplitudes scale with wind_strength so a calm wind produces flat water.
//
// UBO binding 2: scene_infos (view_proj, time)
// UBO binding 7: wind_infos  (wind_xz, wind_strength)
// UBO binding 9: water_infos (water_params.a = water_level)

#version 460 core
#include <layouts/vertex_3d.glsl>
#include <uniforms/scene_infos.glsl>
#include <uniforms/wind_infos.glsl>
#include <uniforms/water_infos.glsl>

out vec3 v_world_pos;
out vec3 v_world_normal;
out vec3 v_tangent;
out vec3 v_bitangent;
out vec2 v_uv;

// Rotates a 2D unit vector by angle_rad (counter-clockwise).
vec2 Rotate2D(vec2 v, float angle_rad) {
    float c = cos(angle_rad);
    float s = sin(angle_rad);
    return vec2(c * v.x - s * v.y, s * v.x + c * v.y);
}

void main() {
    // Derive a base wind direction unit vector from wind_xz.
    // Fall back to +X when wind is calm.
    vec2  wind_dir = (wind_strength > 0.001)
                   ? normalize(wind_xz)
                   : vec2(1.0, 0.0);

    // Wind-scaled base amplitude: at 10 m/s the surface swings ±0.4 m.
    float base_amp = clamp(wind_strength * 0.04, 0.0, 2.0);

    // 4 Gerstner waves with distinct directions (rotations of wind_dir),
    // frequencies, amplitudes (as fractions of base_amp), and speeds.
    const int   N_WAVES    = 4;
    const float Q_STEEP    = 0.5;  // steepness: 0 = pure sine, 1 = sharp crest

    vec2  dirs[N_WAVES];
    float freqs[N_WAVES];
    float amps[N_WAVES];
    float speeds[N_WAVES];

    dirs[0]   = wind_dir;
    freqs[0]  = 0.05;  amps[0]  = 1.00;  speeds[0]  = 1.0;

    dirs[1]   = Rotate2D(wind_dir,  0.5236);  // +30 deg
    freqs[1]  = 0.08;  amps[1]  = 0.60;  speeds[1]  = 0.8;

    dirs[2]   = Rotate2D(wind_dir, -0.3491);  // -20 deg
    freqs[2]  = 0.12;  amps[2]  = 0.35;  speeds[2]  = 1.2;

    dirs[3]   = Rotate2D(wind_dir,  1.0472);  // +60 deg
    freqs[3]  = 0.06;  amps[3]  = 0.25;  speeds[3]  = 0.5;

    vec2  xz      = in_position.xz;
    float water_y = water_params.a;  // world_level stored in water_params.a

    vec3 offset = vec3(0.0);

    // TBN accumulation — analytical Gerstner derivatives.
    // Initialized to flat-surface frame.
    vec3 N = vec3(0.0, 1.0, 0.0);
    vec3 T = vec3(1.0, 0.0, 0.0);
    vec3 B = vec3(0.0, 0.0, 1.0);

    for (int i = 0; i < N_WAVES; ++i) {
        float A   = base_amp * amps[i];
        float w   = freqs[i];
        float phi = dot(dirs[i], xz) * w + time * speeds[i];
        float s   = sin(phi);
        float c   = cos(phi);

        float wA  = w * A;
        float QwA = Q_STEEP * wA;

        // Gerstner position offset.
        offset.x += Q_STEEP * A * dirs[i].x * c;
        offset.y += A * s;
        offset.z += Q_STEEP * A * dirs[i].y * c;

        // Analytical normal derivatives.
        N.x -= dirs[i].x * wA * c;
        N.y -= Q_STEEP   * wA * s;
        N.z -= dirs[i].y * wA * c;

        // Analytical tangent derivatives.
        T.x -= QwA * dirs[i].x * dirs[i].x * s;
        T.y += dirs[i].x * wA * c;
        T.z -= QwA * dirs[i].x * dirs[i].y * s;

        // Analytical bitangent derivatives.
        B.x -= QwA * dirs[i].x * dirs[i].y * s;
        B.y += dirs[i].y * wA * c;
        B.z -= QwA * dirs[i].y * dirs[i].y * s;
    }

    vec3 world_pos = vec3(in_position.x + offset.x,
                          water_y       + offset.y,
                          in_position.z + offset.z);

    v_world_normal = normalize(N);
    v_tangent      = normalize(T);
    v_bitangent    = normalize(B);
    v_world_pos    = world_pos;
    v_uv           = in_position.xz;  // world XZ for tileable normal map sampling

    gl_Position = view_proj * vec4(world_pos, 1.0);
}
