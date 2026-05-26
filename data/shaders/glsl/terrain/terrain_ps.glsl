// Terrain fragment shader — G-buffer geometry pass.
//
// Writes to 3 render targets:
//   location 0 (Albedo,  RGBA8)   — splatmap-blended layer albedo × optional macro
//   location 1 (Normal,  RGBA16F) — world-space normal encoded as N * 0.5 + 0.5
//   location 2 (Specular, RGBA8)  — matte terrain (zero specular)
//
// Texture slot layout:
//   0  — heightmap    (VS / TESE, not used here)
//   1  — splatmap     (RGBA8, one weight per layer)
//   2–5 — layer albedos (layers 0–3)
//   6–9 — layer normals (layers 0–3, tangent-space RG-encoded)
//  10  — macro texture (optional, coarse large-scale detail)
//
// Splatmap blending:
//   albedo = R*albedo0 + G*albedo1 + B*albedo2 + A*albedo3
//   Each layer is sampled at (v_world_uv * tiling[i]).
//
// Triplanar mapping (steep slopes):
//   When |world_normal.y| < triplanar_threshold the layer textures are sampled
//   from three axis-aligned projections weighted by the surface normal instead
//   of the top-down v_world_uv.  Blend weights:
//     w = pow(abs(N), kTriplanarSharpness), w /= dot(w, vec3(1))
//
// Normal blending:
//   Tangent-space normals from each layer are splatmap-blended, then
//   transformed to world space via a TBN matrix derived from v_world_normal.
//
// Macro texture:
//   When use_macro_texture == 1, the final albedo is modulated by a sample
//   from u_macro_texture at a coarser UV scale (kMacroScale).

#version 460 core

#include <uniforms/terrain_patch_infos.glsl>

// Splatmap and layer textures.
layout(binding = 1)  uniform sampler2D u_splatmap;
layout(binding = 2)  uniform sampler2D u_albedo0;
layout(binding = 3)  uniform sampler2D u_albedo1;
layout(binding = 4)  uniform sampler2D u_albedo2;
layout(binding = 5)  uniform sampler2D u_albedo3;
layout(binding = 6)  uniform sampler2D u_normal0;
layout(binding = 7)  uniform sampler2D u_normal1;
layout(binding = 8)  uniform sampler2D u_normal2;
layout(binding = 9)  uniform sampler2D u_normal3;
layout(binding = 10) uniform sampler2D u_macro_texture;

in vec3 v_world_pos;
in vec3 v_world_normal;
in vec2 v_world_uv;

layout(location = 0) out vec4 out_albedo;
layout(location = 1) out vec4 out_normal;
layout(location = 2) out vec4 out_specular;

// Triplanar blend sharpness exponent; higher = sharper transitions.
const float kTriplanarSharpness = 4.0;
// Coarse UV scale applied to the macro texture.
const float kMacroScale = 0.05;

// Decodes a tangent-space normal from a normalmap texel (RG stored as XY,
// Z reconstructed to point outward in tangent space).
vec3 DecodeNormal(vec4 sp) {
    vec2 n  = sp.rg * 2.0 - 1.0;
    float z = sqrt(max(0.0, 1.0 - dot(n, n)));
    return vec3(n, z);
}

// Computes triplanar blend weights from the surface normal.
// Equation: w = saturate(|N|^k) / sum(|N|^k), where k = kTriplanarSharpness.
vec3 TriplanarWeights(vec3 N) {
    vec3 w = pow(abs(N), vec3(kTriplanarSharpness));
    return w / (w.x + w.y + w.z);
}

// Samples an albedo texture with optional triplanar mapping.
//
// use_triplanar — true on steep slopes where top-down UV would stretch
// tri_w         — blending weights from TriplanarWeights()
vec3 SampleAlbedo(sampler2D tex, float layer_tiling, bool use_triplanar, vec3 tri_w) {
    if (use_triplanar) {
        // XZ (top), XY (front-back), ZY (side) projections.
        vec3 xz = texture(tex, v_world_pos.xz * layer_tiling).rgb;
        vec3 xy = texture(tex, v_world_pos.xy * layer_tiling).rgb;
        vec3 zy = texture(tex, v_world_pos.zy * layer_tiling).rgb;
        // tri_w.y weights the top face, tri_w.z the front/back, tri_w.x the sides.
        return xz * tri_w.y + xy * tri_w.z + zy * tri_w.x;
    }
    return texture(tex, v_world_uv * layer_tiling).rgb;
}

// Samples a normal-map texture with optional triplanar mapping.
//
// For each projection the sampled tangent-space normal is re-oriented into
// world-space using the "whiteout blend" technique so that all three
// directions can be averaged correctly:
//   - XZ (top)  : n.xy perturbs surface_N.xz
//   - XY (front): n.xy perturbs surface_N.xy
//   - ZY (side) : n.xy perturbs surface_N.zy
// The resulting vectors are blended by tri_w and re-normalised.
//
// References:
//   Ben Golus — "Normal Mapping for a Triplanar Shader" (2017)
vec3 SampleNormal(sampler2D tex, float layer_tiling, bool use_triplanar,
                  vec3 tri_w, vec3 surface_N) {
    if (use_triplanar) {
        vec3 n_xz = DecodeNormal(texture(tex, v_world_pos.xz * layer_tiling));
        vec3 n_xy = DecodeNormal(texture(tex, v_world_pos.xy * layer_tiling));
        vec3 n_zy = DecodeNormal(texture(tex, v_world_pos.zy * layer_tiling));

        // Whiteout blend: add tangent-space offset to the matching world-space
        // normal components so each sample "leans" toward the surface normal.
        vec3 w_xz = normalize(vec3(n_xz.x + surface_N.x,
                                   n_xz.y + surface_N.y,
                                   n_xz.z + surface_N.z));
        vec3 w_xy = normalize(vec3(n_xy.x + surface_N.x,
                                   n_xy.y + surface_N.y,
                                   n_xy.z));
        vec3 w_zy = normalize(vec3(n_zy.x,
                                   n_zy.y + surface_N.y,
                                   n_zy.z + surface_N.z));

        return normalize(w_xz * tri_w.y + w_xy * tri_w.z + w_zy * tri_w.x);
    }
    return DecodeNormal(texture(tex, v_world_uv * layer_tiling));
}

void main() {
    vec3 N = normalize(v_world_normal);

    // Triplanar: activate on slopes where the surface normal has low Y component.
    bool use_triplanar = abs(N.y) < triplanar_threshold;
    vec3 tri_w         = use_triplanar ? TriplanarWeights(N) : vec3(0.0);

    // Per-layer tiling factors from the uniform block.
    float t0 = tiling.x;
    float t1 = tiling.y;
    float t2 = tiling.z;
    float t3 = tiling.w;

    // Sample albedos and tangent-space normals for all 4 layers.
    vec3 albedo0 = SampleAlbedo(u_albedo0, t0, use_triplanar, tri_w);
    vec3 albedo1 = SampleAlbedo(u_albedo1, t1, use_triplanar, tri_w);
    vec3 albedo2 = SampleAlbedo(u_albedo2, t2, use_triplanar, tri_w);
    vec3 albedo3 = SampleAlbedo(u_albedo3, t3, use_triplanar, tri_w);

    vec3 tsn0 = SampleNormal(u_normal0, t0, use_triplanar, tri_w, N);
    vec3 tsn1 = SampleNormal(u_normal1, t1, use_triplanar, tri_w, N);
    vec3 tsn2 = SampleNormal(u_normal2, t2, use_triplanar, tri_w, N);
    vec3 tsn3 = SampleNormal(u_normal3, t3, use_triplanar, tri_w, N);

    // Splatmap blend weights.
    vec4 splat = texture(u_splatmap, v_world_uv);

    // Blend albedo: weighted sum over 4 layers.
    vec3 albedo = splat.r * albedo0
                + splat.g * albedo1
                + splat.b * albedo2
                + splat.a * albedo3;

    // Blend tangent-space normals before converting to world space.
    vec3 blended_tsn = normalize(splat.r * tsn0
                                + splat.g * tsn1
                                + splat.b * tsn2
                                + splat.a * tsn3);

    // Build TBN matrix.  Tangent is the world-X axis projected onto the
    // terrain tangent plane (stable for near-horizontal terrain; degenerate
    // case handled by falling back to world-Z).
    vec3 T = (abs(N.y) > 0.99)
           ? normalize(cross(N, vec3(0.0, 0.0, 1.0)))
           : normalize(cross(vec3(0.0, 1.0, 0.0), N));
    vec3 B  = cross(N, T);
    mat3 TBN = mat3(T, B, N);

    // Transform blended tangent-space normal to world space.
    vec3 world_n = normalize(TBN * blended_tsn);

    // Optional macro texture: modulate albedo at a coarser scale.
    if (use_macro_texture == 1) {
        vec3 macro = texture(u_macro_texture, v_world_uv * kMacroScale).rgb;
        albedo *= macro;
    }

    out_albedo   = vec4(albedo, 0.0);
    out_normal   = vec4(world_n * 0.5 + 0.5, 0.0);
    out_specular = vec4(0.0, 0.02, 0.0, 0.0);
}
