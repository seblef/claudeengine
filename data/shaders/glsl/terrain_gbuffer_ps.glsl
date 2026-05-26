// Terrain fragment shader for the G-buffer geometry pass.
// Writes to 3 render targets simultaneously:
//   location 0 (Albedo, RGBA8)   — flat grass-green terrain colour
//   location 1 (Normal, RGBA16F) — world-space normal encoded as N × 0.5 + 0.5
//   location 2 (Specular, RGBA8) — no specular for terrain

#version 460 core

in vec3 v_normal;

layout(location = 0) out vec4 out_albedo;
layout(location = 1) out vec4 out_normal;
layout(location = 2) out vec4 out_specular;

void main() {
    out_albedo   = vec4(0.35, 0.55, 0.20, 0.0);
    out_normal   = vec4(normalize(v_normal) * 0.5 + 0.5, 0.0);
    out_specular = vec4(0.0);
}
