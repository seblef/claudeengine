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
// Foam (three physically-motivated sources):
//   h_foam      = smoothstep(thresh, thresh+0.5, v_world_pos.y - water_level)  // crest
//   steep_foam  = smoothstep(st-0.1, st, 1-v_world_normal.y)                   // steepness
//   sl_foam     = (1-smoothstep(0,shore_depth,depth)) * animRing * 0.8         // shoreline
//   foam_amount = max(max(h_foam, steep_foam), sl_foam)
//   ft          = foam_tex(v_uv*0.04 + t*0.015).r * foam_tex(v_uv*0.07 - t*0.010).r
//   foam_amount = clamp(foam_amount * ft * 2.5, 0, 1)
//   final_color = mix(final_color, vec3(1), foam_amount)
//
// Alpha equation:
//   water_depth = v_world_pos.y - world_y_of_scene_bottom  (world-space metres)
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
// Refraction (screen-space):
//   refract_uv  = screen_uv + ts_n.xy * refraction_strength
//   Safety: fall back to screen_uv when distorted UV lands on sky (depth ≥ 1).
//   refract_col = scene_color_tex[refract_uv]
//
// Beer-Lambert absorption:
//   water_depth  = v_world_pos.y - (inv_view_proj * ndc).y / w   (metres below surface)
//   absorption   = exp(-water_depth * absorption_scale)           // transmission factor
//   refract_col  = mix(water_color, refract_col, absorption)      // tint towards water_color at depth
//
// Screen-space reflections (SSR):
//   R           = reflect(-V, N)
//   Two-phase hierarchical ray march (128 m max reach, ~12 depth samples):
//     Phase 1 — coarse bracketing: 8 steps × 16 m.  Early-out on clip.w ≤ 0
//               or |ndc.xy| > 1.05.  Records hit_step on first depth crossing.
//     Phase 2 — binary refinement: 4 bisection iters over [hit_step-1, hit_step]
//               coarse interval; final colour sample at refined t_hi.
//   On hit: edge_fade and dist_fade (= 1 - t_hi/128) attenuate ssr_weight;
//   reflect_color from scene_color_tex at refined UV.
//   Fresnel blend: sky_reflect = mix(sky_zenith, reflect_color, ssr_weight)
//                  surface_col = mix(absorbed, sky_reflect, fresnel)
//
// Wave-tip translucency (subsurface-scattering approximation):
//   back_scatter  = max(dot(sun_dir, -N), 0)    // sun behind the wave face
//   steepness     = 1 - v_world_normal.y         // 0 = flat, 1 = vertical crest
//   tip_amount    = back_scatter * steepness * sun_vis
//   translucency  = kTipColor * tip_amount * sun_intensity * 0.15
//   final_color  += translucency
//
// UBO binding 2: scene_infos (view_proj, eye_pos, inv_screen_size, inv_view_proj, z_near, z_far)
// UBO binding 9: water_infos
// Sampler 0:     u_normal_map     — procedural tileable water normal map
// Sampler 1:     u_foam_tex       — procedural tileable foam blob texture
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
layout(binding = 1) uniform sampler2D u_foam_tex;        // slot 1 — procedural foam texture
layout(binding = 2) uniform sampler2D scene_color_tex;  // slot 2 — scene colour snapshot
layout(binding = 3) uniform sampler2D depth_tex;        // slot 3 — scene depth snapshot

layout(location = 0) out vec4 out_color;

void main() {
    // Per-fragment XZ distance from eye, used to gate expensive features by LOD tier.
    // is_near: full quality (SSR, foam, second normal, translucency).
    // is_mid:  partial quality (foam, second normal, translucency; no SSR).
    // far:     minimal quality (only primary normal map and reflections).
    float frag_dist = length(eye_pos.xz - v_world_pos.xz);
    bool  is_near   = frag_dist < scroll_params.z;  // lod_near_dist
    bool  is_mid    = frag_dist < scroll_params.w;  // lod_far_dist

    // Sample two scrolling normal map layers and combine into a world-space normal.
    // foam_params.z/w = normal_scale1/2; scroll_params.x/y = scroll speeds.
    // Second layer skipped beyond lod_far_dist — reuse n1 instead.
    vec2 uv1 = v_uv * foam_params.z + time * scroll_params.x * vec2(1.0, 0.6);
    vec2 uv2 = v_uv * foam_params.w - time * scroll_params.y * vec2(0.7, 1.0);

    vec3 n1   = texture(u_normal_map, uv1).rgb * 2.0 - 1.0;
    vec3 n2   = is_mid ? texture(u_normal_map, uv2).rgb * 2.0 - 1.0 : n1;
    vec3 ts_n = normalize(n1 + n2);

    // Transform tangent-space normal into world space using analytical Gerstner TBN.
    mat3 TBN = mat3(normalize(v_tangent),
                    normalize(v_bitangent),
                    normalize(v_world_normal));
    vec3 N = normalize(TBN * ts_n);

    vec3 V = normalize(eye_pos - v_world_pos);

    // Screen-space UV of the current fragment.
    vec2 screen_uv = gl_FragCoord.xy * inv_screen_size;

    // Screen-space refraction: distort UV using tangent-space normal xy.
    // ts_n.xy produces screen-aligned distortion that tracks surface ripples.
    vec2 distortion = ts_n.xy * refraction_params.x;
    vec2 refract_uv = clamp(screen_uv + distortion, vec2(0.001), vec2(0.999));

    // Safety: if the distorted UV lands on sky geometry (no depth), fall back to
    // undistorted UV to avoid sampling the sky colour through the water surface.
    if (texture(depth_tex, refract_uv).r >= 0.9999)
        refract_uv = screen_uv;

    vec3 refract_color = texture(scene_color_tex, refract_uv).rgb;

    // Water column depth via world-space reconstruction from the depth buffer.
    // inv_view_proj unprojection gives the world-space position of the scene bottom;
    // the Y difference to the water surface yields depth in world-space metres.
    float scene_d = texture(depth_tex, screen_uv).r;
    float water_depth;
    if (scene_d >= 0.9999) {
        water_depth = 100.0;   // sky below water — treat as very deep
    } else {
        vec4 ndc   = vec4(screen_uv * 2.0 - 1.0, scene_d * 2.0 - 1.0, 1.0);
        vec4 world = inv_view_proj * ndc;
        water_depth = max(v_world_pos.y - (world.y / world.w), 0.0);
    }

    // Steepness used by both foam and translucency — always computed.
    float steepness = 1.0 - v_world_normal.y;

    // Foam — skipped beyond lod_far_dist (imperceptible at distance).
    float foam_amount = 0.0;
    if (is_mid) {
        // 1. Wave-height foam: crest exceeds foam_height_thresh.
        float wave_height = v_world_pos.y - water_params.a;
        float h_foam      = smoothstep(refraction_params.z,
                                       refraction_params.z + 0.5,
                                       wave_height);

        // 2. Steepness foam: Gerstner macro normal tilted far from vertical.
        float steep_foam = smoothstep(foam_params.x - 0.1, foam_params.x, steepness);

        // 3. Shoreline foam: shallow water with an animated ring following the wave.
        float shore_foam = 1.0 - smoothstep(0.0, refraction_params.w, water_depth);
        float anim       = sin(water_depth * 6.0 - time * foam_params.y) * 0.5 + 0.5;
        float sl_foam    = shore_foam * anim * 0.8;

        foam_amount = max(max(h_foam, steep_foam), sl_foam);

        // Foam texture modulation — two scrolling samples break foam into natural clumps.
        float ft = texture(u_foam_tex, v_uv * 0.04 + time * 0.015).r
                 * texture(u_foam_tex, v_uv * 0.07 - time * 0.010).r;
        foam_amount = clamp(foam_amount * ft * 2.5, 0.0, 1.0);
    }

    // Schlick Fresnel.
    float n_dot_v = max(dot(N, V), 0.0);
    float fresnel  = pow(1.0 - n_dot_v, 5.0);

    // Beer-Lambert absorption: transmission factor → 0 (opaque) at depth, 1 at surface.
    // mix(water_color, refract, absorption): deep water shows intrinsic water_color,
    // shallow water shows the refracted scene below.
    float absorption = exp(-water_depth * refraction_params.y);
    vec3  absorbed   = mix(water_params.rgb, refract_color, absorption);

    // Screen-space reflections (SSR): hierarchical two-phase ray march against the
    // scene depth buffer (8 coarse × 16 m + 4-iter binary refinement, 128 m max).
    // Falls back to sky zenith where no intersection is found.
    // Skipped beyond lod_near_dist — sky zenith used directly.
    vec3  R             = reflect(-V, N);
    vec3  reflect_color = sky_zenith_color.rgb;
    float ssr_weight    = 0.0;

    if (is_near && R.y >= -0.05 && fresnel > 0.03) {
        const int   kCoarseSteps = 8;
        const float kCoarseStep  = 16.0;
        const float kMaxReach    = kCoarseStep * float(kCoarseSteps);  // 128 m

        // Phase 1 — coarse bracketing: find first coarse interval containing a hit.
        int hit_step = -1;

        for (int i = 1; i <= kCoarseSteps; ++i) {
            vec3 sample_pos = v_world_pos + R * kCoarseStep * float(i);
            vec4 clip       = view_proj * vec4(sample_pos, 1.0);
            if (clip.w <= 0.0) break;
            vec3 ndc = clip.xyz / clip.w;
            if (any(greaterThan(abs(ndc.xy), vec2(1.05)))) break;

            vec2  suv          = ndc.xy * 0.5 + 0.5;
            float ray_depth    = ndc.z  * 0.5 + 0.5;
            float stored_depth = texture(depth_tex, suv).r;

            if (ray_depth > stored_depth) { hit_step = i; break; }
        }

        // Phase 2 — binary refinement over the bracketed coarse interval.
        if (hit_step > 0) {
            float t_lo = kCoarseStep * float(hit_step - 1);
            float t_hi = kCoarseStep * float(hit_step);

            for (int r = 0; r < 4; ++r) {
                float t_mid     = (t_lo + t_hi) * 0.5;
                vec3  spos      = v_world_pos + R * t_mid;
                vec4  clip      = view_proj * vec4(spos, 1.0);
                vec3  ndc       = clip.xyz / clip.w;
                vec2  suv       = ndc.xy * 0.5 + 0.5;
                float ray_depth = ndc.z  * 0.5 + 0.5;

                if (ray_depth > texture(depth_tex, suv).r)
                    t_hi = t_mid;
                else
                    t_lo = t_mid;
            }

            vec4  clip_hi   = view_proj * vec4(v_world_pos + R * t_hi, 1.0);
            vec3  ndc_hi    = clip_hi.xyz / clip_hi.w;
            vec2  suv_hi    = ndc_hi.xy * 0.5 + 0.5;
            vec2  edge_fade = 1.0 - smoothstep(0.8, 1.0, abs(suv_hi * 2.0 - 1.0));
            float dist_fade = 1.0 - t_hi / kMaxReach;
            ssr_weight      = min(edge_fade.x, edge_fade.y) * dist_fade;
            reflect_color   = texture(scene_color_tex, suv_hi).rgb;
        }
    }

    // Fresnel blend: SSR result (or sky fallback) mixed into the reflection term.
    vec3 sky_reflect = mix(sky_zenith_color.rgb, reflect_color, ssr_weight);
    vec3 surface_col = mix(absorbed, sky_reflect, fresnel);

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

    // Wave-tip translucency: cyan-teal glow where wave crests are steep and backlit.
    // Skipped beyond lod_far_dist — imperceptible on distant water.
    if (is_mid) {
        float back_scatter   = max(dot(sun_dir, -N), 0.0);
        float tip_amount     = back_scatter * steepness * sun_vis;
        const vec3 kTipColor = vec3(0.10, 0.80, 0.70);
        final_color += kTipColor * tip_amount * sun_params.w * 0.15;
    }

    final_color = mix(final_color, vec3(1.0), foam_amount);

    // Alpha: shoreline fade-in, foam override, Fresnel lift.
    float out_alpha = smoothstep(0.0, 1.5, water_depth);
    out_alpha = max(out_alpha, foam_amount);
    out_alpha = clamp(out_alpha + fresnel * (1.0 - out_alpha), 0.0, 1.0);

    out_color = vec4(final_color, out_alpha);
}
