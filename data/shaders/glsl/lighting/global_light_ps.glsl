// Fragment shader for the GlobalLight (ambient + directional + CSM shadow) pass.
// Samples the full G-buffer and applies Blinn-Phong shading with no attenuation.
// Output is additive — blended onto the HDR render target.
//
// UBO binding 2: scene infos (inv_view_proj, eye_pos, view).
// UBO binding 4: light infos (color, intensity, direction, ambient, cast_shadow, shadow_bias).
// UBO binding 5: CSM infos (cascade_vp[4], split_distances).
// Samplers: binding 5=albedo (a=spec_intensity), 6=normal (a=shininess/256), 8=depth,
//           binding 9–12=cascade shadow maps,
//           binding 13=cloud shadow map (world-space R16F, centred on camera).
// Uniforms: cloud_shadow_intensity [0,1]  — max shadow darkening from clouds.
//           cloud_shadow_coverage  float  — half-extent of the shadow texture (metres).

#version 460 core
#include <uniforms/scene_infos.glsl>
#include <uniforms/light_infos.glsl>
#include <uniforms/csm_infos.glsl>

layout(binding = 5)  uniform sampler2D        u_albedo;
layout(binding = 6)  uniform sampler2D        u_normal;
layout(binding = 8)  uniform sampler2D        u_depth;
layout(binding = 9)  uniform sampler2DShadow  u_shadow_cascade0;
layout(binding = 10) uniform sampler2DShadow  u_shadow_cascade1;
layout(binding = 11) uniform sampler2DShadow  u_shadow_cascade2;
layout(binding = 12) uniform sampler2DShadow  u_shadow_cascade3;
layout(binding = 13) uniform sampler2D        u_cloud_shadow;

uniform float cloud_shadow_intensity;
uniform float cloud_shadow_coverage;

// Altitude of the virtual cloud plane above the camera (metres) — must match
// the value in cloud_density.glsl so the shadow UV projection is consistent.
const float kCloudShadowAltitude = 800.0;

out vec4 out_color;

// Samples the correct cascade shadow map for the given shadow UV.
// Returns 1.0 when lit, 0.0 when in shadow.
float SampleCascadeShadow(int cascade, vec3 uv) {
    if      (cascade == 0) return texture(u_shadow_cascade0, uv);
    else if (cascade == 1) return texture(u_shadow_cascade1, uv);
    else if (cascade == 2) return texture(u_shadow_cascade2, uv);
    else                   return texture(u_shadow_cascade3, uv);
}

void main() {
    // Screen UV derived from fragment position — exact, no interpolation error.
    vec2 v_screen_uv = gl_FragCoord.xy * inv_screen_size;

    // Decode G-buffer.
    vec4  albedo_s  = texture(u_albedo, v_screen_uv);
    vec3  albedo    = albedo_s.rgb;
    float spec_int  = albedo_s.a;
    vec4  normal_s  = texture(u_normal, v_screen_uv);
    float shininess = normal_s.a * 256.0;
    // Decode normal: stored as N * 0.5 + 0.5.
    vec3  N = normalize(normal_s.rgb * 2.0 - 1.0);

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

    // CSM shadow: select cascade by view-space depth, sample depth map.
    float shadow_factor = 0.0;
    if (cast_shadow > 0.5) {
        // View-space depth (positive, camera-to-fragment distance).
        float view_depth = abs((view * vec4(world_pos, 1.0)).z);

        // Select cascade: use the nearest whose far plane covers view_depth.
        int cascade = 3;
        if      (view_depth < split_distances.x) cascade = 0;
        else if (view_depth < split_distances.y) cascade = 1;
        else if (view_depth < split_distances.z) cascade = 2;

        // Project world_pos into cascade clip space, remap to [0,1].
        vec4 shadow_clip = cascade_vp[cascade] * vec4(world_pos, 1.0);
        vec3 shadow_ndc  = shadow_clip.xyz / shadow_clip.w;
        vec3 shadow_uv   = shadow_ndc * 0.5 + 0.5;

        // Subtract bias to prevent self-shadowing acne.
        shadow_uv.z -= shadow_bias;

        float lit  = SampleCascadeShadow(cascade, shadow_uv);
        shadow_factor = 1.0 - lit;
    }

    // Cloud shadow: trace from world_pos toward the sun to the cloud plane,
    // then sample the shadow texture for the cloud density above that point.
    //
    // direction (LightInfosBlock) is the incident ray from light → surface,
    // so direction.y < 0 when the sun is above the horizon.
    // At t = kCloudShadowAltitude / (-direction.y) the ray hits the cloud plane;
    // the cloud XZ that casts shadow on world_pos is:
    //   cloud_xz = world_pos.xz + direction.xz / direction.y * kCloudShadowAltitude
    float cloud_shadow_factor = 0.0;
    if (direction.y < 0.0 && cloud_shadow_coverage > 0.0) {
        vec2 cloud_xz   = world_pos.xz + direction.xz / direction.y * kCloudShadowAltitude;
        vec2 shadow_uv  = (cloud_xz - eye_pos.xz) / cloud_shadow_coverage * 0.5 + 0.5;
        if (all(greaterThanEqual(shadow_uv, vec2(0.0))) &&
            all(lessThanEqual(shadow_uv, vec2(1.0)))) {
            cloud_shadow_factor =
                texture(u_cloud_shadow, shadow_uv).r * cloud_shadow_intensity;
        }
    }

    // Apply CSM shadow to full output and cloud shadow to the direct sun term only
    // (diff + spec), preserving the ambient contribution in cloud shadow.
    float sun_factor = 1.0 - shadow_factor * cast_shadow;
    vec3  direct     = (diff + spec) * sun_factor * (1.0 - cloud_shadow_factor);
    vec3  amb        = ambient * albedo;
    out_color = vec4(direct + amb, 1.0);
}
