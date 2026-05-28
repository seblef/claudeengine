// Water vertex shader — geometry pass.
//
// Displaces the flat XZ grid vertically with a sum of 4 sine waves whose
// directions and frequencies are derived from the current wind vector.  The
// analytical normal is computed from the wave height derivatives.
//
// Wave model (per wave i):
//   height_i = A_i * sin(dot(dir_i, xz) * freq_i + wind_time * speed_i)
//   dH/dx_i  = A_i * freq_i * cos(...) * dir_i.x
//   dH/dz_i  = A_i * freq_i * cos(...) * dir_i.y
//   N = normalize(vec3(-sum(dH/dx), 1.0, -sum(dH/dz)))
//
// Amplitudes scale with wind_strength so a calm wind produces flat water.
//
// UBO binding 2: scene_infos (view_proj)
// UBO binding 7: wind_infos  (wind_xz, wind_strength, wind_time)
// UBO binding 9: water_infos (water_params.a = water_level)

#version 460 core
#include <layouts/vertex_3d.glsl>
#include <uniforms/scene_infos.glsl>
#include <uniforms/wind_infos.glsl>
#include <uniforms/water_infos.glsl>

out vec3 v_world_pos;
out vec3 v_world_normal;

// Rotates a 2D unit vector by angle_rad (counter-clockwise).
vec2 Rotate2D(vec2 v, float angle_rad) {
    float c = cos(angle_rad);
    float s = sin(angle_rad);
    return vec2(c * v.x - s * v.y, s * v.x + c * v.y);
}

void main() {
    const float PI = 3.14159265;

    // Derive a base wind direction unit vector from wind_xz.
    // Fall back to +X when wind is calm.
    vec2  wind_dir = (wind_strength > 0.001)
                   ? normalize(wind_xz)
                   : vec2(1.0, 0.0);

    // Wind-scaled base amplitude: at 10 m/s the surface swings ±0.4 m.
    float base_amp = clamp(wind_strength * 0.04, 0.0, 2.0);

    // 4 waves with distinct directions (rotations of wind_dir), frequencies,
    // amplitudes (as fractions of base_amp), and propagation speeds.
    const int N_WAVES = 4;
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
    float water_y = water_params.a;  // undisplaced surface height

    float total_y  = 0.0;
    float d_dx     = 0.0;
    float d_dz     = 0.0;

    for (int i = 0; i < N_WAVES; ++i) {
        float amp   = base_amp * amps[i];
        float phase = dot(dirs[i], xz) * freqs[i] + wind_time * speeds[i];
        float s     = sin(phase);
        float c     = cos(phase);

        // Height contribution.
        total_y += amp * s;

        // Analytical partial derivatives for normal computation.
        // dH/dx = amp * cos(phase) * freq * dir.x
        // dH/dz = amp * cos(phase) * freq * dir.y
        d_dx += amp * freqs[i] * c * dirs[i].x;
        d_dz += amp * freqs[i] * c * dirs[i].y;
    }

    vec3 world_pos = vec3(in_position.x,
                          water_y + total_y,
                          in_position.z);

    // Surface normal from height field: N = normalize(-dH/dx, 1, -dH/dz).
    v_world_normal = normalize(vec3(-d_dx, 1.0, -d_dz));
    v_world_pos    = world_pos;

    gl_Position = view_proj * vec4(world_pos, 1.0);
}
