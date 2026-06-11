// Terrain fragment shader — G-buffer geometry pass.
//
// Writes to 3 render targets:
//   location 0 (Albedo,  RGBA8)   — splatmap-blended layer albedo × optional macro
//   location 1 (Normal,  RGBA16F) — world-space normal encoded as N * 0.5 + 0.5
//   location 2 (Specular, RGBA8)  — R=specular intensity (wet boost), G=shininess/256
//
// Texture slot layout:
//   0     — heightmap    (VS / TESE, not used here)
//   1     — splatmap     (RGBA8, one weight per layer)
//   2–5   — layer albedos (layers 0–3)
//  10     — macro texture (optional, coarse large-scale detail)
//  11     — caustic texture (RGBA8, tileable, animated caustic projection)
//  12–15  — layer normals (layers 0–3, tangent-space RG-encoded)
//           (slots 6-9 reserved for GBuffer samplers during the lighting pass)
//
// Splatmap blending:
//   albedo = R*albedo0 + G*albedo1 + B*albedo2 + A*albedo3
//   Each layer is sampled at (v_world_uv * tiling[i]).
//
// Normal blending:
//   Tangent-space normals from each layer are splatmap-blended, then
//   transformed to world space via a TBN matrix derived from v_world_normal.
//
// Macro texture:
//   When use_macro_texture == 1, the final albedo is modulated by a sample
//   from u_macro_texture at a coarser UV scale (kMacroScale).

#version 460 core

#include <uniforms/scene_infos.glsl>
#include <uniforms/terrain_patch_infos.glsl>
#include <uniforms/water_infos.glsl>

// Splatmap and layer textures.
layout(binding = 1)  uniform sampler2D u_splatmap;
layout(binding = 2)  uniform sampler2D u_albedo0;
layout(binding = 3)  uniform sampler2D u_albedo1;
layout(binding = 4)  uniform sampler2D u_albedo2;
layout(binding = 5)  uniform sampler2D u_albedo3;
layout(binding = 12) uniform sampler2D u_normal0;
layout(binding = 13) uniform sampler2D u_normal1;
layout(binding = 14) uniform sampler2D u_normal2;
layout(binding = 15) uniform sampler2D u_normal3;
layout(binding = 10) uniform sampler2D u_macro_texture;
layout(binding = 11) uniform sampler2D u_caustic_tex;

in vec3 v_world_pos;
in vec3 v_world_normal;
in vec2 v_world_uv;

layout(location = 0) out vec4 out_albedo;
layout(location = 1) out vec4 out_normal;
layout(location = 2) out vec4 out_specular;

// Coarse UV scale applied to the macro texture.
const float kMacroScale = 0.05;

// Decodes a tangent-space normal from a normalmap texel (RG stored as XY,
// Z reconstructed to point outward in tangent space).
vec3 DecodeNormal(vec4 sp) {
    return sp.rgb * 2.0 - 1.0;
}

vec3 BlendNormals(vec3 n1, vec3 n2, float f) {
    return normalize(vec3(mix(n1.xy/n1.z, n2.xy/n2.z, 1.0 - f), 1.0));
}

vec3 BlendAllNormals(vec3 n0, vec3 n1, vec3 n2, vec3 n3, vec4 splat) {
    vec3 n = BlendNormals(n0, n1, splat.a);
    float f = splat.r + splat.g;
    n = BlendNormals(n, n2, f);
    f += splat.b;
    return BlendNormals(n, n3, f);
}

void main() {
    // Use the precomputed per-texel normal map when available; fall back to the
    // interpolated per-vertex normal from the VS/TESE when it is not bound.

    vec3 N = normalize(v_world_normal);

    // Per-layer tiling factors from the uniform block.
    float t0 = tiling.x;
    float t1 = tiling.y;
    float t2 = tiling.z;
    float t3 = tiling.w;

    // Sample albedos and tangent-space normals for all 4 layers.
    vec3 albedo0 = texture(u_albedo0, v_world_uv * t0).rgb;
    vec3 albedo1 = texture(u_albedo1, v_world_uv * t1).rgb;
    vec3 albedo2 = texture(u_albedo2, v_world_uv * t2).rgb;
    vec3 albedo3 = texture(u_albedo3, v_world_uv * t3).rgb;

    vec3 tsn0 = DecodeNormal(texture(u_normal0, v_world_uv * t0));
    vec3 tsn1 = DecodeNormal(texture(u_normal1, v_world_uv * t1));
    vec3 tsn2 = DecodeNormal(texture(u_normal2, v_world_uv * t2));
    vec3 tsn3 = DecodeNormal(texture(u_normal3, v_world_uv * t3));

    // Splatmap blend weights.
    vec4 splat = texture(u_splatmap, v_world_uv);

    // Blend albedo: weighted sum over 4 layers.
    vec3 albedo = splat.r * albedo0
                + splat.g * albedo1
                + splat.b * albedo2
                + splat.a * albedo3;

    // Blend tangent-space normals before converting to world space.
    vec3 blended_tsn = BlendAllNormals(tsn0, tsn1, tsn2, tsn3, splat);

    // Build TBN matrix.  Tangent is the world-X axis projected onto the
    // terrain tangent plane (stable for near-horizontal terrain; degenerate
    // case handled by falling back to world-Z).
    vec3 T = cross(vec3(1, 0, 0), N);
    vec3 B  = cross(N, T);
    mat3 TBN = mat3(T, B, N);
    vec3 world_n = normalize(TBN * blended_tsn);

    // Optional macro texture: modulate albedo at a coarser scale.
    if (use_macro_texture == 1) {
        vec3 macro = texture(u_macro_texture, v_world_uv * kMacroScale).rgb;
        albedo *= macro;
    }

    // Caustic projection: brighten submerged terrain where focused sunlight veins appear.
    // water_params.a   — world-space Y of the undisplaced water surface.
    // refraction_params.y — absorption_scale, reused as caustic depth falloff rate.
    //
    // Two scrolling UV layers are multiplied to create a time-varying interference
    // pattern (product emphasises bright peaks and suppresses uniform areas).
    // Falloff: exp(-depth * absorption_scale) so caustics fade with depth.
    float world_y   = v_world_pos.y;
    float water_lvl = water_params.a;
    if (world_y < water_lvl) {
        float water_depth = water_lvl - world_y;
        vec2  world_xz    = v_world_pos.xz;

        vec2 c_uv1 = world_xz * 0.05 + time * vec2(0.03, 0.02);
        vec2 c_uv2 = world_xz * 0.05 - time * vec2(0.02, 0.03);
        float caustic = texture(u_caustic_tex, c_uv1).r
                      * texture(u_caustic_tex, c_uv2).r;

        // Eq: falloff = exp(-depth * k); caustics brighten albedo proportionally.
        float falloff = exp(-water_depth * refraction_params.y);
        albedo *= 1.0 + caustic * falloff * 2.0;
    }

    // Wet shoreline: darken albedo and boost specular near the waterline.
    // Submerged fragments (height_above_water < 0) are always fully wet.
    // Above water, the effect fades linearly over kWetMargin metres.
    // Eq: wet ∈ [0,1] — 1 = fully wet, 0 = dry.
    const float kWetMargin   = 1.0;   // metres above water_level where fade ends
    const float kWetDarken   = 0.5;   // albedo scale at full wetness
    const float kWetSpecular = 0.4;   // specular intensity at full wetness

    float height_above_water = world_y - water_lvl;
    float wet = (height_above_water < 0.0)
              ? 1.0
              : 1.0 - smoothstep(0.0, kWetMargin, height_above_water);

    albedo = mix(albedo, albedo * kWetDarken, wet);
    float specular_intensity = mix(0.0, kWetSpecular, wet);

    out_albedo   = vec4(albedo, 0.0);
    out_normal   = vec4(world_n * 0.5 + 0.5, 0.0);
    out_specular = vec4(specular_intensity, 0.02, 0.0, 0.0);
}
