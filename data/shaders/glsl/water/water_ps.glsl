// Water fragment shader — forward pass (after deferred lighting).
//
// Runs after the emissive pass on a copy of the HDR scene colour and depth.
// Outputs a single RGBA value; the alpha channel drives SrcAlpha blending so
// shallow shoreline water fades in and deep water is opaque.
//
// Normal map blending:
//   Two tileable water normal map layers scroll in slightly different directions
//   to produce micro-surface ripple detail independent of the vertex waves.
//   uv1 = v_uv * normal_scale1 + time * scroll_speed1 * vec2(1.0, 0.6)
//   uv2 = v_uv * normal_scale2 - time * scroll_speed2 * vec2(0.7, 1.0)
//   ts_n = normalize(n1 + n2)
//   N    = normalize(TBN * ts_n)
//   where TBN comes from the Gerstner analytical tangent frame (from VS).
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
// Specular (Cook-Torrance GGX microfacet):
//   H      = normalize(sun_dir + V)
//   D      = alpha2 / (PI * (n_dot_h^2 * (alpha2-1) + 1)^2)            // GGX NDF
//   G      = G_V * G_L  where G_X = n_dot_x / (n_dot_x*(1-k)+k)        // Schlick-Smith
//   F_spec = 0.02 + 0.98 * (1 - dot(H,V))^5                             // Schlick, f0=0.02
//   spec   = D * G * F_spec / (4 * n_dot_v * n_dot_l) * n_dot_l * sun_intensity
//
// Refraction:
//   Samples scene_color_tex with a UV distortion driven by the surface normal
//   and refraction_strength. Absorption_scale darkens the refracted colour
//   towards water_color with depth.
//
// UBO binding 2: scene_infos (eye_pos, inv_screen_size, z_near_, z_far_)
// UBO binding 9: water_infos
// Sampler 0:     u_normal_map     — procedural tileable water normal map
// Sampler 2:     scene_color_tex  — HDR copy before water pass
// Sampler 3:     depth_tex        — depth copy before water pass

#version 460 core
#include <uniforms/scene_infos.glsl>
#include <uniforms/water_infos.glsl>

#define PI 3.14159265359

in  vec3 v_world_pos;
in  vec3 v_world_normal;
in  vec3 v_tangent;
in  vec3 v_bitangent;
in  vec2 v_uv;

layout(binding = 0) uniform sampler2D u_normal_map;      // slot 0 — procedural normal map
layout(binding = 2) uniform sampler2D scene_color_tex;  // slot 2 — scene colour snapshot
layout(binding = 3) uniform sampler2D depth_tex;        // slot 3 — scene depth snapshot

layout(location = 0) out vec4 out_color;

// Convert a raw depth buffer value [0,1] to linear view-space depth.
float LinearizeDepth(float d) {
    float z_ndc = 2.0 * d - 1.0;
    return (2.0 * z_near * z_far) / (z_far + z_near - z_ndc * (z_far - z_near));
}

void main() {
    // Sample two scrolling normal map layers and combine into a world-space normal.
    // foam_params.z/w = normal_scale1/2; scroll_params.x/y = scroll speeds.
    vec2 uv1 = v_uv * foam_params.z + time * scroll_params.x * vec2(1.0, 0.6);
    vec2 uv2 = v_uv * foam_params.w - time * scroll_params.y * vec2(0.7, 1.0);

    vec3 n1   = texture(u_normal_map, uv1).rgb * 2.0 - 1.0;
    vec3 n2   = texture(u_normal_map, uv2).rgb * 2.0 - 1.0;
    vec3 ts_n = normalize(n1 + n2);

    // Transform tangent-space normal into world space using analytical Gerstner TBN.
    mat3 TBN = mat3(normalize(v_tangent),
                    normalize(v_bitangent),
                    normalize(v_world_normal));
    vec3 N = normalize(TBN * ts_n);

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

    // Cook-Torrance GGX microfacet specular from sun.
    vec3  sun_dir  = normalize(sun_params.xyz);
    float sun_vis  = max(sign(sun_dir.y), 0.0);
    vec3  H        = normalize(sun_dir + V);
    float roughness = max(sky_zenith_color.a, 0.001);
    float n_dot_l  = max(dot(N, sun_dir), 0.0);
    float n_dot_h  = max(dot(N, H), 0.0);

    // GGX normal distribution function.
    float alpha  = roughness * roughness;
    float alpha2 = alpha * alpha;
    float d      = n_dot_h * n_dot_h * (alpha2 - 1.0) + 1.0;
    float D      = alpha2 / (PI * d * d);

    // Schlick-Smith geometry term (direct lighting).
    float k   = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float G_V = n_dot_v / max(n_dot_v * (1.0 - k) + k, 0.001);
    float G_L = n_dot_l / max(n_dot_l * (1.0 - k) + k, 0.001);
    float G   = G_V * G_L;

    // Schlick Fresnel (water IOR 1.33 → f0 ≈ 0.02).
    float f0     = 0.02;
    float F_spec = f0 + (1.0 - f0) * pow(1.0 - max(dot(H, V), 0.0), 5.0);

    // Cook-Torrance BRDF × cosine term × sun radiance.
    float spec_brdf = D * G * F_spec / max(4.0 * n_dot_v * n_dot_l, 0.001) * n_dot_l;
    vec3  spec      = vec3(spec_brdf) * sun_params.w * sun_vis;

    vec3 final_color = surface_col + spec;

    // Alpha: shoreline fade-in, foam override, Fresnel lift.
    float out_alpha = smoothstep(0.0, 1.5, water_depth);
    out_alpha = max(out_alpha, foam_amount);
    out_alpha = clamp(out_alpha + fresnel * (1.0 - out_alpha), 0.0, 1.0);

    out_color = vec4(final_color, out_alpha);
}
