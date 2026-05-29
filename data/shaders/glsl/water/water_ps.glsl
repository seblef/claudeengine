// Water fragment shader — forward pass (after deferred lighting).
//
// Runs after the emissive pass on a copy of the HDR scene colour and depth.
// Outputs a single RGBA value; the alpha channel drives SrcAlpha blending so
// shallow shoreline water fades in and deep water is opaque.
//
// Alpha equation:
//   water_depth = linear depth of scene bottom - linear depth of water surface
//   out_alpha   = smoothstep(0, 1.5, water_depth)       // shoreline fade-in
//   out_alpha   = max(out_alpha, foam_amount)            // foam always opaque
//   out_alpha   = clamp(out_alpha + F*(1-out_alpha), 0, 1)  // Fresnel lift
//
// Fresnel (Schlick):
//   F = pow(1 - dot(N, V), 5)
//
// Specular (Blinn-Phong):
//   H    = normalize(sun_dir + V)
//   spec = pow(max(dot(N, H), 0), 1/roughness) * sun_intensity * sun_above_horizon
//
// Refraction:
//   Samples scene_color_tex with a UV distortion driven by the surface normal
//   and refraction_strength. Absorption_scale darkens the refracted colour
//   towards water_color with depth.
//
// UBO binding 2: scene_infos (eye_pos, inv_screen_size, z_near_, z_far_)
// UBO binding 9: water_infos
// Sampler 2:     scene_color_tex  — HDR copy before water pass
// Sampler 3:     depth_tex        — depth copy before water pass

#version 460 core
#include <uniforms/scene_infos.glsl>
#include <uniforms/water_infos.glsl>

in  vec3 v_world_pos;
in  vec3 v_world_normal;

uniform sampler2D scene_color_tex;  // slot 2 — scene colour snapshot
uniform sampler2D depth_tex;        // slot 3 — scene depth snapshot

layout(location = 0) out vec4 out_color;

// Convert a raw depth buffer value [0,1] to linear view-space depth.
float LinearizeDepth(float d) {
    float z_ndc = 2.0 * d - 1.0;
    return (2.0 * z_near * z_far) / (z_far + z_near - z_ndc * (z_far - z_near));
}

void main() {
    vec3 N = normalize(v_world_normal);
    vec3 V = normalize(eye_pos - v_world_pos);

    // Screen-space UV of the current fragment.
    vec2 screen_uv = gl_FragCoord.xy * inv_screen_size;

    // Water column depth: distance from surface to scene geometry beneath.
    float raw_scene_depth = texture(depth_tex, screen_uv).r;
    float linear_scene  = LinearizeDepth(raw_scene_depth);
    float linear_water  = LinearizeDepth(gl_FragCoord.z);
    float water_depth   = max(linear_scene - linear_water, 0.0);

    // Shoreline foam: strong near shore (water_depth < foam_shoreline_depth).
    float foam_amount = 1.0 - smoothstep(0.0, refraction_params.w, water_depth);

    // Schlick Fresnel.
    float n_dot_v = max(dot(N, V), 0.0);
    float fresnel  = pow(1.0 - n_dot_v, 5.0);

    // Refraction: distort screen UV by surface normal, scaled by depth opacity.
    float refraction_str = refraction_params.x;
    vec2 distortion = N.xz * refraction_str * max(1.0 - water_depth * 0.5, 0.0);
    vec3 refracted = texture(scene_color_tex, clamp(screen_uv + distortion,
                                                     vec2(0.0), vec2(1.0))).rgb;

    // Depth-based absorption: blend refracted colour towards water_color.
    float absorption = 1.0 - exp(-water_depth * refraction_params.y);
    vec3  absorbed   = mix(refracted, water_params.rgb, absorption);

    // Surface colour: Fresnel blend between absorbed (deep) and sky zenith.
    vec3 sky_color   = sky_zenith_color.rgb;
    vec3 surface_col = mix(absorbed, sky_color, fresnel);

    // Blinn-Phong specular from sun.
    vec3  sun_dir  = normalize(sun_params.xyz);
    float sun_vis  = max(sign(sun_dir.y), 0.0);
    vec3  H        = normalize(sun_dir + V);
    // Map roughness → Blinn-Phong shininess: shininess = 2/r² − 2.
    // roughness=0.05 → ~798, roughness=0.1 → ~198.
    float roughness = max(sky_zenith_color.a, 0.001);
    float shininess = max(2.0 / (roughness * roughness) - 2.0, 1.0);
    float spec_raw  = pow(max(dot(N, H), 0.0), shininess);
    vec3  spec      = vec3(spec_raw * sun_vis * sun_params.w);

    vec3 final_color = surface_col + spec;

    // Alpha: shoreline fade-in, foam override, Fresnel lift.
    float out_alpha = smoothstep(0.0, 1.5, water_depth);
    out_alpha = max(out_alpha, foam_amount);
    out_alpha = clamp(out_alpha + fresnel * (1.0 - out_alpha), 0.0, 1.0);

    out_color = vec4(final_color, out_alpha);
}
