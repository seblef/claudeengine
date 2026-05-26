// Terrain vertex shader for the G-buffer geometry pass.
//
// Vertex positions are stored in the shared patch mesh as XZ coordinates in
// mesh space [0, patch_size]. World position is reconstructed using the
// per-patch uniform block (slot 6), and height is sampled from the heightmap
// using explicit LOD 0 (mandatory in vertex shaders in GLSL).
//
// CDLOD morph snaps odd-numbered vertices toward the nearest even-numbered
// (coarser-LOD) vertex, blended by morph_factor. This prevents popping at
// LOD boundaries.
//
// UBO binding 2: scene_infos (view_proj)
// UBO binding 6: terrain_patch_infos (patch_origin, patch_scale, ...)
// Sampler 0: u_heightmap — GL_R16 unsigned-normalised; .r in [0, 1]

#version 460 core
#include <layouts/vertex_terrain.glsl>

#include <uniforms/scene_infos.glsl>
#include <uniforms/terrain_patch_infos.glsl>

layout(binding = 0) uniform sampler2D u_heightmap;

out vec3 v_world_pos;
out vec3 v_world_normal;
out vec2 v_world_uv;

// Returns world-space height at the given heightmap UV (explicit LOD required in VS).
float SampleHeight(vec2 uv) {
    return heightmap_offset + textureLod(u_heightmap, uv, 0.0).r * heightmap_scale;
}

void main() {
    // CDLOD morph: snap vertex to next coarser LOD grid, then lerp.
    // Odd-index vertices shift to the nearest even index; even vertices are stable.
    vec2 snapped  = floor(in_position / 2.0) * 2.0;
    vec2 local    = mix(in_position, snapped, morph_factor);

    vec2 world_xz = patch_origin + local * patch_scale;
    vec2 uv       = world_xz * inv_terrain_world;

    float world_y = SampleHeight(uv);

    // Central-difference normal in world space.
    // mpt = metres per texel at the current LOD level.
    float mpt  = patch_scale / float(1 << lod_level);
    vec2  step = inv_terrain_world * mpt;

    float h_r = SampleHeight(uv + vec2(step.x, 0.0));
    float h_l = SampleHeight(uv - vec2(step.x, 0.0));
    float h_f = SampleHeight(uv + vec2(0.0, step.y));
    float h_b = SampleHeight(uv - vec2(0.0, step.y));

    // Equation: N = normalize(-(dH/dX), 2*mpt, -(dH/dZ)) in Y-up space.
    // dH/dX ≈ (h_r - h_l) / (2*mpt), similarly for Z.
    v_world_normal = normalize(vec3(h_l - h_r, 2.0 * mpt, h_b - h_f));

    v_world_pos = vec3(world_xz.x, world_y, world_xz.y);
    v_world_uv  = uv;

    gl_Position = view_proj * vec4(v_world_pos, 1.0);
}
