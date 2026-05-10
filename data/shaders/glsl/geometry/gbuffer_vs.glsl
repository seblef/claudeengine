// Vertex shader for the G-buffer geometry pass.
// UBO binding 1: per-renderable infos (renderer::RenderableInfos) — world matrix.
// UBO binding 2: per-frame scene infos (renderer::SceneInfos) — view_proj.
//
// Transforms per-vertex position to clip space and passes world-space
// TBN vectors to the fragment shader for normal mapping.

#version 460 core
#include <layouts/vertex_3d.glsl>

#include <uniforms/renderable_infos.glsl>
#include <uniforms/scene_infos.glsl>

out vec2 v_uv;
out vec3 v_normal_world;
out vec3 v_tangent_world;
out vec3 v_binormal_world;

void main() {
    gl_Position = view_proj * world * vec4(in_position, 1.0);
    v_uv = in_uv;

    mat3 world3 = mat3(world);
    v_normal_world   = normalize(world3 * in_normal);
    v_tangent_world  = normalize(world3 * in_tangent);
    v_binormal_world = normalize(world3 * in_binormal);
}
