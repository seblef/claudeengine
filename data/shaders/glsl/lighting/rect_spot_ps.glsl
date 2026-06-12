// Fragment shader for the RectangleSpotLight (rectangular beam) pass.
// Applies Blinn-Phong with inverse-square attenuation and independent H/V falloffs.
// Output is additive — blended onto the HDR render target.
//
// The light's local frame is reconstructed from the forward direction:
//   right   = normalize(cross(ref, direction))    ref = {0,1,0} or {1,0,0} when dir≈±Y
//   up      = cross(direction, right)
//
// Per-axis angle from the forward axis (using atan for correctness at large angles):
//   h_angle_px = abs(atan(dot(-L, right),   dot(-L, direction)))
//   v_angle_px = abs(atan(dot(-L, up),      dot(-L, direction)))
//
// Each axis fades from full (inside 80% of half-angle) to zero (at half-angle):
//   h_fall = 1 - smoothstep(h_angle * 0.8, h_angle, h_angle_px)
//   v_fall = 1 - smoothstep(v_angle * 0.8, v_angle, v_angle_px)
//
// UBO binding 2: scene infos (inv_view_proj, eye_pos).
// UBO binding 4: light infos (position, color, intensity, direction, range,
//                             h_angle, v_angle, light_vp, cast_shadow).
// Samplers: binding 5=albedo, 6=normal, 7=specular, 8=depth, 9=shadow map.

#version 460 core
#include <uniforms/scene_infos.glsl>
#include <uniforms/light_infos.glsl>

layout(binding = 5) uniform sampler2D       u_albedo;
layout(binding = 6) uniform sampler2D       u_normal;
layout(binding = 7) uniform sampler2D       u_specular;
layout(binding = 8) uniform sampler2D       u_depth;
layout(binding = 9) uniform sampler2DShadow u_shadow_map;

out vec4 out_color;

void main() {
    // Screen UV derived from fragment position — exact, no interpolation error.
    vec2 v_screen_uv = gl_FragCoord.xy * inv_screen_size;

    // Decode G-buffer.
    vec3  albedo    = texture(u_albedo,   v_screen_uv).rgb;
    vec2  spec_shine = texture(u_specular, v_screen_uv).rg;
    float spec_int  = spec_shine.r;
    float shininess = spec_shine.g * 256.0;
    vec3  N = normalize(texture(u_normal, v_screen_uv).rgb * 2.0 - 1.0);

    // Reconstruct world position from depth.
    float raw_depth = texture(u_depth, v_screen_uv).r;
    vec4  ndc       = vec4(v_screen_uv * 2.0 - 1.0, raw_depth * 2.0 - 1.0, 1.0);
    vec4  world_h   = inv_view_proj * ndc;
    vec3  world_pos = world_h.xyz / world_h.w;

    vec3 V = normalize(eye_pos - world_pos);

    // Point-light component.
    vec3  L_vec = position - world_pos;
    float dist  = length(L_vec);
    vec3  L     = L_vec / dist;
    float atten = 1.0 / (1.0 + (dist * dist) / (range * range));

    vec3 H = normalize(L + V);

    vec3 diff = max(dot(N, L), 0.0) * albedo * color * intensity;
    vec3 spec = pow(max(dot(N, H), 0.0), shininess) * spec_int * color * intensity;

    // Build the light's local right/up frame — mirrors AlignZToDir in C++.
    vec3 ref   = abs(direction.y) > 0.99 ? vec3(1.0, 0.0, 0.0)
                                         : vec3(0.0, 1.0, 0.0);
    vec3 right  = normalize(cross(ref, direction));
    vec3 up_dir = cross(direction, right);

    // Project -L into the local frame and compute per-axis angles.
    vec3  neg_L     = -L;
    float fwd_proj  = dot(neg_L, direction);
    float h_angle_px = abs(atan(dot(neg_L, right),  fwd_proj));
    float v_angle_px = abs(atan(dot(neg_L, up_dir), fwd_proj));

    // Independent H/V falloff with 20% soft edge.
    float h_fall = 1.0 - smoothstep(h_angle * 0.8, h_angle, h_angle_px);
    float v_fall = 1.0 - smoothstep(v_angle * 0.8, v_angle, v_angle_px);

    // Shadow: project world position into light space and sample the shadow map.
    //   shadow_coord.xy = NDC [-1,1] remapped to [0,1] UV.
    //   shadow_coord.z  = depth in [0,1] with bias subtracted for the comparison.
    float shadow = 0.0;
    if (cast_shadow > 0.5) {
        vec4 shadow_coord = light_vp * vec4(world_pos, 1.0);
        shadow_coord.xyz /= shadow_coord.w;
        shadow_coord.xyz  = shadow_coord.xyz * 0.5 + 0.5;
        shadow_coord.z   -= shadow_bias;
        shadow = 1.0 - texture(u_shadow_map, shadow_coord.xyz);
    }

    out_color = vec4((diff + spec) * atten * h_fall * v_fall * (1.0 - shadow), 1.0);
}
