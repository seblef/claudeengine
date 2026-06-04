// Half-resolution SSR pre-pass fragment shader.
//
// Runs only the hierarchical ray march to compute screen-space reflections for
// near-surface water fragments.  Output is premultiplied alpha RGBA16F so that
// the full-res pass can reconstruct via a simple weighted sum:
//   reflect_color = ssr_up.rgb / ssr_up.a   (when ssr_up.a > threshold)
//
// Using only the Gerstner macro normal (from vertex shader) for the reflection
// direction keeps this pass cheap; the small error is imperceptible after the
// bilateral upsample.
//
// Ray march (two-phase hierarchical, identical to the one in water_ps.glsl):
//   Phase 1 — coarse bracketing: 8 steps × 16 m.  Early-out on clip.w ≤ 0
//             or |ndc.xy| > 1.05.  Records hit_step on first depth crossing.
//   Phase 2 — binary refinement: 4 bisection iterations over the coarse
//             interval; final colour sample at refined t_hi.
//   Edge fade (suppress border artefacts) and distance fade attenuate ssr_weight.
//
// UBO binding 2: scene_infos (view_proj, eye_pos, inv_screen_size, inv_view_proj)
// UBO binding 9: water_infos (scroll_params.z = lod_near_dist)
// Sampler 2:     scene_color_tex — HDR scene colour snapshot
// Sampler 3:     depth_tex       — scene depth snapshot

#version 460 core
#include <uniforms/scene_infos.glsl>
#include <uniforms/water_infos.glsl>

in  vec3 v_world_pos;
in  vec3 v_world_normal;
in  vec3 v_tangent;
in  vec3 v_bitangent;
in  vec2 v_uv;

layout(binding = 2) uniform sampler2D scene_color_tex;
layout(binding = 3) uniform sampler2D depth_tex;

layout(location = 0) out vec4 out_color;

void main() {
    // Discard fragments beyond the SSR LOD boundary (lod_near_dist).
    float frag_dist = length(eye_pos.xz - v_world_pos.xz);
    if (frag_dist >= scroll_params.z) {
        out_color = vec4(0.0);
        return;
    }

    // Use the Gerstner analytical normal from the vertex shader.
    // Normal map perturbation is omitted in this pass to keep cost low.
    vec3 N = normalize(v_world_normal);
    vec3 V = normalize(eye_pos - v_world_pos);

    // Schlick Fresnel gate — no SSR where surface is nearly perpendicular.
    float n_dot_v = max(dot(N, V), 0.0);
    float fresnel  = pow(1.0 - n_dot_v, 5.0);
    vec3  R        = reflect(-V, N);

    if (R.y < -0.05 || fresnel <= 0.03) {
        out_color = vec4(0.0);
        return;
    }

    // ── Two-phase hierarchical SSR ray march ─────────────────────────────────
    // Equation: sample_pos(t) = v_world_pos + R * t
    // NDC depth of the ray sample is compared against the stored depth to detect
    // intersections.  Phase 1 finds a coarse interval; Phase 2 bisects it.
    const int   kCoarseSteps = 8;
    const float kCoarseStep  = 16.0;
    const float kMaxReach    = kCoarseStep * float(kCoarseSteps);  // 128 m

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

    if (hit_step < 0) {
        out_color = vec4(0.0);
        return;
    }

    // Phase 2 — binary refinement over the coarse interval [hit_step-1, hit_step].
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

    // Edge and distance fade.
    vec2  edge_fade = 1.0 - smoothstep(0.8, 1.0, abs(suv_hi * 2.0 - 1.0));
    float dist_fade = 1.0 - t_hi / kMaxReach;
    float ssr_weight = min(edge_fade.x, edge_fade.y) * dist_fade;

    vec3 reflect_color = texture(scene_color_tex, suv_hi).rgb;

    // Premultiplied alpha output: a = ssr_weight, rgb = reflect_color * ssr_weight.
    out_color = vec4(reflect_color * ssr_weight, ssr_weight);
}
