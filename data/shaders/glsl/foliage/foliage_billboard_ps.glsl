// Foliage billboard pixel shader — rendered in the emissive/forward pass.
//
// Additively blends the billboard texture into the HDR render target.
// Alpha cutout at 0.5 ensures billboard silhouettes match foliage shape.
//
// Sampler binding 0: albedo texture (same as the near-mesh pass).

#version 460 core

in vec2 v_uv;

layout(binding = 0) uniform sampler2D u_albedo;

layout(location = 0) out vec4 out_color;

void main() {
    vec4 albedo = texture(u_albedo, v_uv);
    if (albedo.a < 0.5) discard;
    // Slight dimming to avoid billboards appearing brighter than lit meshes.
    out_color = vec4(albedo.rgb * 0.7, 1.0);
}
