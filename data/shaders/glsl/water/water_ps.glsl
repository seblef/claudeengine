// Water fragment shader — G-buffer geometry pass.
//
// Writes to 3 render targets:
//   location 0 (Albedo,   RGBA8)   — Fresnel-blended water/sky colour
//   location 1 (Normal,   RGBA16F) — world-space normal encoded as N * 0.5 + 0.5
//   location 2 (Specular, RGBA8)   — Blinn-Phong specular reflectance
//
// Fresnel approximation (Schlick):
//   f = pow(1.0 - dot(N, V), 5.0)
//   albedo = mix(water_color, sky_zenith_color, f)
//
// Blinn-Phong specular (sun):
//   H     = normalize(sun_dir + V)
//   spec  = pow(max(dot(N, H), 0), kShininess) * sun_visible
//
// UBO binding 2: scene_infos (eye_pos)
// UBO binding 9: water_infos (water_params, sky_zenith_color, sun_direction)

#version 460 core
#include <uniforms/scene_infos.glsl>
#include <uniforms/water_infos.glsl>

in  vec3 v_world_pos;
in  vec3 v_world_normal;

layout(location = 0) out vec4 out_albedo;
layout(location = 1) out vec4 out_normal;
layout(location = 2) out vec4 out_specular;

// Blinn-Phong shininess for water specular highlights.
const float kShininess = 128.0;

void main() {
    vec3  N = normalize(v_world_normal);
    vec3  V = normalize(eye_pos - v_world_pos);

    // Schlick Fresnel: f → 0 when looking straight down, → 1 at grazing angles.
    float n_dot_v = max(dot(N, V), 0.0);
    float fresnel  = pow(1.0 - n_dot_v, 5.0);

    vec3  water_color   = water_params.rgb;
    vec3  zenith_color  = sky_zenith_color.rgb;
    vec3  albedo        = mix(water_color, zenith_color, fresnel);

    // Blinn-Phong specular from sun direction.
    vec3  sun_dir = normalize(sun_direction.xyz);
    // Only add specular when the sun is above the horizon.
    float sun_vis = max(sign(sun_dir.y), 0.0);
    vec3  H       = normalize(sun_dir + V);
    float spec    = pow(max(dot(N, H), 0.0), kShininess) * sun_vis;

    // Encode normal to [0, 1] for RGBA16F G-buffer attachment.
    vec3 encoded_normal = N * 0.5 + 0.5;

    out_albedo   = vec4(albedo, 1.0);
    out_normal   = vec4(encoded_normal, 1.0);
    out_specular = vec4(spec, spec, spec, 1.0);
}
