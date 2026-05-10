// Fragment shader for the emissive pass (shares gbuffer_vs.glsl).
// Only drawn for objects where Material::IsEmissive() is true.
// Additively blends emissive and ambient contributions into the HDR render target.
//
// UBO binding 3: per-material infos (renderer::MaterialInfos).
// Sampler slot 3: emissive texture.

#version 460 core
#include <uniforms/material_infos.glsl>

in vec2 v_uv;

layout(binding = 3) uniform sampler2D u_emissive;

out vec4 frag_color;

void main() {
    // Equation: emissive_texture × emissive_color + ambient_color (additive blend into HDR RT).
    vec3 emissive = texture(u_emissive, v_uv).rgb * emissive_color.rgb;
    frag_color = vec4(emissive + ambient_color.rgb, 1.0);
}
