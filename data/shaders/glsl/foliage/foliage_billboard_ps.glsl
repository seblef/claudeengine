// Foliage billboard pixel shader — rendered in the emissive/forward pass.
//
// Applies a forward-pass directional-light approximation so billboards
// respond to the global light consistently with near-mesh instances.
// Alpha cutout at 0.5 ensures billboard silhouettes match foliage shape.
//
// Normal: world-up (0, 1, 0) — mirrors the bent-normal bias used by mesh
// instances so both LOD tiers react identically to top-down lighting.
//
// Lighting equation (matches global_light_ps.glsl, global light only):
//   L    = normalize(-direction)        // surface-to-light
//   diff = max(dot(world_up, L), 0)
//   lit  = albedo * (ambient + color * intensity * diff)
//
// Sampler binding 0: albedo texture (same as the near-mesh pass).
// UBO    binding 4: light infos (direction, color, intensity, ambient).

#version 460 core
#include <uniforms/light_infos.glsl>

in vec2 v_uv;

layout(binding = 0) uniform sampler2D u_albedo;

layout(location = 0) out vec4 out_color;

void main() {
    vec4 albedo = texture(u_albedo, v_uv);
    if (albedo.a < 0.5) discard;

    // Forward global-light approximation with world-up normal.
    vec3  L    = normalize(-direction);
    float diff = max(dot(vec3(0.0, 1.0, 0.0), L), 0.0);
    vec3  lit  = albedo.rgb * (ambient + color * intensity * diff);

    out_color = vec4(lit, 1.0);
}
