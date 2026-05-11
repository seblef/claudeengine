// Fragment shader for the GlobalLight (ambient + directional) pass.
// Samples the full G-buffer and applies Blinn-Phong shading with no attenuation.
// Output is additive — blended onto the HDR render target.
//
// UBO binding 2: scene infos (inv_view_proj, eye_pos).
// UBO binding 4: light infos (color, intensity, direction, ambient).
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
    // Decode normal: stored as N * 0.5 + 0.5.
    vec3  N = normalize(texture(u_normal, v_screen_uv).rgb * 2.0 - 1.0);

    // Reconstruct world position from the depth buffer.
    // ndc.z in [-1, 1] for GL clip space.
    float raw_depth = texture(u_depth, v_screen_uv).r;
    vec4  ndc       = vec4(v_screen_uv * 2.0 - 1.0, raw_depth * 2.0 - 1.0, 1.0);
    vec4  world_h   = inv_view_proj * ndc;
    vec3  world_pos = world_h.xyz / world_h.w;

    vec3 V = normalize(eye_pos - world_pos);

    // Blinn-Phong — directional light, no attenuation.
    vec3 L = normalize(-direction);
    vec3 H = normalize(L + V);

    // diffuse = N·L * albedo * light_color * intensity
    vec3 diff = max(dot(N, L), 0.0) * albedo * color * intensity;
    // specular = (N·H)^shininess * spec_int * light_color * intensity
    vec3 spec = pow(max(dot(N, H), 0.0), shininess) * spec_int * color * intensity;

    out_color = vec4(diff + spec + ambient * albedo, 1.0);
}
