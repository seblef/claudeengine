// Tree geometry-pass pixel shader — standard G-buffer output.
//
// Writes to two G-buffer render targets:
//   location 0 (Albedo,   RGBA8)   — rgb=diffuse texture RGB; alpha-cutout at 0.5,
//                                     a=0 (bark/foliage has no specular)
//   location 1 (Normal,   RGBA16F) — rgb=world-space normal encoded to [0, 1],
//                                     a=0 (shininess)
//
// Sampler binding 0: albedo/diffuse texture.

#version 460 core

in vec2 v_uv;
in vec3 v_normal_world;

layout(binding = 0) uniform sampler2D u_albedo;

layout(location = 0) out vec4 out_albedo;
layout(location = 1) out vec4 out_normal;

void main() {
    vec4 albedo = texture(u_albedo, v_uv);
    if (albedo.a < 0.5) discard;

    out_albedo = vec4(albedo.rgb, 0.0);
    out_normal = vec4(v_normal_world * 0.5 + 0.5, 0.0);
}
