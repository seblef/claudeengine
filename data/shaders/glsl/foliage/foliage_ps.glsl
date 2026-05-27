// Foliage geometry-pass pixel shader.
//
// Writes to the three G-buffer render targets:
//   location 0 (Albedo,  RGBA8)   — diffuse texture RGB; alpha-cutout at 0.5
//   location 1 (Normal,  RGBA16F) — world-space normal encoded to [0, 1]
//   location 2 (Specular, RGBA8)  — zeroed (foliage has no specular highlight)
//
// Sampler binding 0: diffuse/albedo texture.

#version 460 core

in vec2 v_uv;
in vec3 v_normal_world;

layout(binding = 0) uniform sampler2D u_albedo;

layout(location = 0) out vec4 out_albedo;
layout(location = 1) out vec4 out_normal;
layout(location = 2) out vec4 out_specular;

void main() {
    vec4 albedo = texture(u_albedo, v_uv);
    // Alpha cutout for foliage cards/transparencies.
    if (albedo.a < 0.5) discard;

    out_albedo   = vec4(albedo.rgb, 0.0);
    out_normal   = vec4(v_normal_world * 0.5 + 0.5, 0.0);
    out_specular = vec4(0.0);
}
