// Foliage geometry-pass vertex shader — GPU instanced mesh rendering.
//
// Per-instance world matrix is fetched from the SSBO at binding 0 using
// gl_InstanceID.  The matrix layout is row_major and matches core::Mat4f.
//
// Outputs:
//   v_uv          — UV coordinates passed to the pixel shader.
//   v_normal_world — world-space normal (non-uniform scale not supported).

#version 460 core
#include <layouts/vertex_3d.glsl>
#include <uniforms/scene_infos.glsl>

// Per-instance world matrices uploaded by FoliageRenderer::ClassifyAndUpload().
layout(std430, row_major, binding = 0) readonly buffer FoliageInstances {
    mat4 world[];
};

out vec2 v_uv;
out vec3 v_normal_world;

void main() {
    mat4 w = world[gl_InstanceID];
    gl_Position    = view_proj * w * vec4(in_position, 1.0);
    v_uv           = in_uv;
    // Uniform scale only: normal transform = upper-left 3×3 of world matrix.
    v_normal_world = normalize(mat3(w) * in_normal);
}
