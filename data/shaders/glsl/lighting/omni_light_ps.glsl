// Fragment shader for the OmniLight (point light) pass.
// Applies Blinn-Phong with inverse-square attenuation.
// Output is additive — blended onto the HDR render target.
//
// Attenuation: 1 / (1 + dist² / range²)
//   → 1.0 at the light position, ~0.5 at distance=range.
//
// UBO binding 2: scene infos (inv_view_proj, eye_pos).
// UBO binding 4: light infos (position, color, intensity, range).
// Samplers: binding 5=albedo, 6=normal, 7=specular, 8=depth.

#version 460 core
#include <uniforms/scene_infos.glsl>
#include <uniforms/light_infos.glsl>

layout(binding = 5) uniform sampler2D u_albedo;
layout(binding = 6) uniform sampler2D u_normal;
layout(binding = 7) uniform sampler2D u_specular;
layout(binding = 8) uniform sampler2D u_depth;

out vec4 out_color;

void main() {
    // Screen UV derived from fragment position — exact, no interpolation error.
    vec2 v_screen_uv = gl_FragCoord.xy * inv_screen_size;

    // Decode G-buffer.
    vec3  albedo    = texture(u_albedo,   v_screen_uv).rgb;
    float spec_int  = texture(u_specular, v_screen_uv).r;
    float shininess = texture(u_specular, v_screen_uv).g * 256.0;
    vec3  N = normalize(texture(u_normal, v_screen_uv).rgb * 2.0 - 1.0);

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

    vec3 H = normalize(L + V);

    vec3 diff = max(dot(N, L), 0.0) * albedo * color * intensity;
    vec3 spec = pow(max(dot(N, H), 0.0), shininess) * spec_int * color * intensity;

    out_color = vec4((diff + spec) * atten, 1.0);
}
