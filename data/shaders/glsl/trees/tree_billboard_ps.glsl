// Tree billboard pixel shader — rendered in the emissive/forward pass.
//
// Alpha-tested billboard texture composited additively into the HDR RT.
// Slight dimming prevents billboards from appearing brighter than lit meshes.
//
// Sampler binding 0: billboard texture (RGBA, alpha channel used for cutout).

#version 460 core

in vec2 v_uv;

layout(binding = 0) uniform sampler2D u_billboard;

layout(location = 0) out vec4 out_color;

void main() {
    vec4 albedo = texture(u_billboard, v_uv);
    if (albedo.a < 0.5) discard;
    out_color = vec4(albedo.rgb * 0.7, 1.0);
}
