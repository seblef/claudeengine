// Fragment shader for the G-buffer geometry pass.
// Writes to 3 render targets simultaneously:
//   location 0 (Albedo, RGBA8)   — diffuse_texture.rgb × diffuse_color.rgb
//   location 1 (Normal, RGBA16F) — world-space normal encoded as N × 0.5 + 0.5
//   location 2 (Specular, RGBA8) — R=specular intensity, G=shininess/256
//
// UBO binding 3: per-material infos (renderer::MaterialInfos).
// Sampler slots: 0=diffuse, 1=normal, 2=specular.

#version 460 core
#include <uniforms/material_infos.glsl>

in vec2 v_uv;
in vec3 v_normal_world;
in vec3 v_tangent_world;
in vec3 v_binormal_world;

layout(binding = 0) uniform sampler2D u_diffuse;
layout(binding = 1) uniform sampler2D u_normal;
layout(binding = 2) uniform sampler2D u_specular;

uniform bool u_has_normal_map;

layout(location = 0) out vec4 out_albedo;
layout(location = 1) out vec4 out_normal;
layout(location = 2) out vec4 out_specular;

void main() {
    // Albedo: diffuse texture tinted by material color.
    vec3 albedo = texture(u_diffuse, v_uv).rgb * diffuse_color.rgb;
    out_albedo  = vec4(albedo, 0.0);

    // Normal: TBN-mapped if a normal map is bound, else interpolated vertex normal.
    vec3 world_normal;
    if (u_has_normal_map) {
        // Remap tangent-space normal from [0,1] to [-1,1], then to world space.
        vec3 tangent_normal = texture(u_normal, v_uv).rgb * 2.0 - 1.0;
        mat3 tbn = mat3(v_tangent_world, v_binormal_world, v_normal_world);
        world_normal = normalize(tbn * tangent_normal);
    } else {
        world_normal = normalize(v_normal_world);
    }
    out_normal = vec4(world_normal * 0.5 + 0.5, 0.0);

    // Specular: intensity in R, packed shininess in G (range 0–256 mapped to 0–1).
    float spec_intensity = texture(u_specular, v_uv).r;
    out_specular = vec4(spec_intensity, shininess / 256.0, 0.0, 0.0);
}
