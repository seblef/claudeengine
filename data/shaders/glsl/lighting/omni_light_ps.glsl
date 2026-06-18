// Fragment shader for the OmniLight (point light) pass.
// Applies Blinn-Phong with inverse-square attenuation, smooth range fadeoff,
// and optional cube shadow.
// Output is additive — blended onto the HDR render target.
//
// Attenuation: 1 / (1 + dist² / range²)
//   → 1.0 at the light position, ~0.5 at distance=range.
//
// Range fadeoff: smoothstep on normalised distance in [0.8, 1.0]
//   → 1.0 inside 80 % of range, fades to 0.0 at the sphere boundary.
//   Prevents the hard edge visible when geometry clipping cuts the light volume.
//
// Shadow: direction = world_pos - position selects the cube face; comparison
//   value = length(direction) / range normalises depth to [0,1].
//
// UBO binding 2: scene infos (inv_view_proj, eye_pos).
// UBO binding 4: light infos (position, color, intensity, range,
//                             cast_shadow, shadow_bias).
// Samplers: binding 5=albedo (a=spec_intensity), 6=normal (a=shininess/256), 8=depth,
//           13=shadow cube.

#version 460 core
#include <uniforms/scene_infos.glsl>
#include <uniforms/light_infos.glsl>

layout(binding = 5)  uniform sampler2D       u_albedo;
layout(binding = 6)  uniform sampler2D       u_normal;
layout(binding = 8)  uniform sampler2D       u_depth;
layout(binding = 13) uniform samplerCubeShadow u_shadow_cube;

out vec4 out_color;

void main() {
    // Screen UV derived from fragment position — exact, no interpolation error.
    vec2 v_screen_uv = gl_FragCoord.xy * inv_screen_size;

    // Decode G-buffer.
    vec4  albedo_s  = texture(u_albedo, v_screen_uv);
    vec3  albedo    = albedo_s.rgb;
    float spec_int  = albedo_s.a;
    vec4  normal_s  = texture(u_normal, v_screen_uv);
    float shininess = normal_s.a * 256.0;
    vec3  N = normalize(normal_s.rgb * 2.0 - 1.0);

    // Reconstruct world position from depth.
    float raw_depth = texture(u_depth, v_screen_uv).r;
    vec4  ndc       = vec4(v_screen_uv * 2.0 - 1.0, raw_depth * 2.0 - 1.0, 1.0);
    vec4  world_h   = inv_view_proj * ndc;
    vec3  world_pos = world_h.xyz / world_h.w;

    vec3 V = normalize(eye_pos - world_pos);

    // Point light direction and distance.
    vec3  L_vec = position - world_pos;
    float dist  = length(L_vec);
    vec3  L     = L_vec / dist;

    // Inverse-square attenuation normalised so atten=1 at the source.
    float atten = 1.0 / (1.0 + (dist * dist) / (range * range));

    // Smooth fadeoff to zero at the sphere boundary.
    //   d_norm = 1 at range; fade window is the outer 20% of the radius.
    float d_norm    = clamp(dist / range, 0.0, 1.0);
    float edge_fade = 1.0 - smoothstep(0.8, 1.0, d_norm);

    vec3 H = normalize(L + V);

    vec3 diff = max(dot(N, L), 0.0) * albedo * color * intensity;
    vec3 spec = pow(max(dot(N, H), 0.0), shininess) * spec_int * color * intensity;

    // Shadow: direction from light to fragment selects the cube face.
    //   compare = normalised linear distance in [0,1]; bias subtracted to avoid acne.
    float shadow = 0.0;
    if (cast_shadow > 0.5) {
        vec3  light_dir = world_pos - position;
        float compare   = length(light_dir) / range - shadow_bias;
        shadow = 1.0 - texture(u_shadow_cube, vec4(light_dir, compare));
    }

    out_color = vec4((diff + spec) * atten * edge_fade * (1.0 - shadow), 1.0);
}
