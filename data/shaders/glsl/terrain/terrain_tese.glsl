// Terrain tessellation-evaluation shader.
//
// Bilinearly interpolates position and UV from the 4 control points, displaces
// Y from the heightmap, then applies Phong tessellation to smooth silhouettes:
//
//   Phong correction (per control point i, blended by bilinear weight w_i):
//     correction_i = dot(linear_pos - ctrl_pos_i, ctrl_normal_i) * ctrl_normal_i
//   phong_correction = sum_i(w_i * correction_i)
//   phong_pos  = linear_pos - phong_correction
//   final_pos  = mix(linear_pos, phong_pos, kPhongAlpha)
//
// Control-point ordering (matches TerrainPatchMesh tessellation IBO):
//   cp[0] = (u=0, v=0)  bottom-left
//   cp[1] = (u=1, v=0)  bottom-right
//   cp[2] = (u=1, v=1)  top-right
//   cp[3] = (u=0, v=1)  top-left
//
// UBO binding 2: scene_infos         (view_proj)
// UBO binding 6: terrain_patch_infos (heightmap_scale, heightmap_offset)
// Sampler 0: u_heightmap — GL_R16 unsigned-normalised; .r in [0, 1]

#version 460 core

layout(quads, fractional_even_spacing, ccw) in;

#include <uniforms/scene_infos.glsl>
#include <uniforms/terrain_patch_infos.glsl>

layout(binding = 0) uniform sampler2D u_heightmap;

in vec3 tc_world_pos[];
in vec3 tc_world_normal[];
in vec2 tc_world_uv[];

out vec3 v_world_pos;
out vec3 v_world_normal;
out vec2 v_world_uv;

// Phong alpha: blend factor toward the projected (smooth) position.
const float kPhongAlpha = 0.75;

// Returns world-space height at the given heightmap UV.
float SampleHeight(vec2 uv) {
    return heightmap_offset + texture(u_heightmap, uv).r * heightmap_scale;
}

void main() {
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;

    // Bilinear weights for the 4 control points.
    float w0 = (1.0 - u) * (1.0 - v);  // cp[0] — bottom-left
    float w1 = u         * (1.0 - v);  // cp[1] — bottom-right
    float w2 = u         * v;           // cp[2] — top-right
    float w3 = (1.0 - u) * v;           // cp[3] — top-left

    // Bilinearly interpolate heightmap UV and world XZ.
    v_world_uv = tc_world_uv[0] * w0 + tc_world_uv[1] * w1 +
                 tc_world_uv[2] * w2 + tc_world_uv[3] * w3;

    vec3 linear_pos;
    linear_pos.xz = (tc_world_pos[0].xz * w0 + tc_world_pos[1].xz * w1 +
                     tc_world_pos[2].xz * w2 + tc_world_pos[3].xz * w3);
    linear_pos.y  = SampleHeight(v_world_uv);

    // Phong tessellation: project linear_pos onto each control-point tangent plane,
    // then blend corrections with bilinear weights.
    // correction_i = dot(linear_pos - ctrl_pos_i, ctrl_normal_i) * ctrl_normal_i
    vec3 phong_correction = vec3(0.0);
    float weights[4] = float[](w0, w1, w2, w3);
    for (int i = 0; i < 4; ++i) {
        vec3 n = normalize(tc_world_normal[i]);
        phong_correction += weights[i] * dot(linear_pos - tc_world_pos[i], n) * n;
    }
    v_world_pos = mix(linear_pos, linear_pos - phong_correction, kPhongAlpha);

    // Interpolate and renormalise world-space normal.
    v_world_normal = normalize(
        tc_world_normal[0] * w0 + tc_world_normal[1] * w1 +
        tc_world_normal[2] * w2 + tc_world_normal[3] * w3);

    gl_Position = view_proj * vec4(v_world_pos, 1.0);
}
