// Terrain wireframe vertex shader — editor debug overlay.
//
// Reproduces the same CDLOD morph + heightmap displacement as terrain_vs.glsl
// so wireframe edges align exactly with the rendered terrain surface.
// Only gl_Position is output; the fragment shader needs no interpolants.
//
// UBO binding 2: scene_infos (view_proj)
// UBO binding 6: terrain_patch_infos (patch_origin, patch_scale, ...)
// Sampler 0: u_heightmap — GL_R16 unsigned-normalised; .r in [0, 1]

#version 460 core
#include <layouts/vertex_terrain.glsl>

#include <uniforms/scene_infos.glsl>
#include <uniforms/terrain_patch_infos.glsl>

layout(binding = 0) uniform sampler2D u_heightmap;

// Returns world-space height at the given heightmap UV (explicit LOD required in VS).
float SampleHeight(vec2 uv) {
    return heightmap_offset + textureLod(u_heightmap, uv, 0.0).r * heightmap_scale;
}

void main() {
    // CDLOD morph: identical to terrain_vs.glsl for correct edge alignment.
    vec2 snapped  = floor(in_position / 2.0) * 2.0;
    vec2 local    = mix(in_position, snapped, morph_factor);

    vec2 world_xz = patch_origin + local * patch_scale;
    vec2 uv       = world_xz * inv_terrain_world;

    float world_y  = SampleHeight(uv);
    vec3  world_pos = vec3(world_xz.x, world_y, world_xz.y);

    gl_Position = view_proj * vec4(world_pos, 1.0);
}
