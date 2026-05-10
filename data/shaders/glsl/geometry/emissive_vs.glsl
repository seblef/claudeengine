// Vertex shader for the emissive pass (shares position + UV transform with gbuffer_vs).
// UBO binding 1: per-renderable infos (renderer::RenderableInfos) — world matrix.
// UBO binding 2: per-frame scene infos (renderer::SceneInfos) — view_proj.

#version 460 core
#include <layouts/vertex_3d.glsl>

#include <uniforms/renderable_infos.glsl>
#include <uniforms/scene_infos.glsl>

out vec2 v_uv;

void main() {
    gl_Position = view_proj * world * vec4(in_position, 1.0);
    v_uv = in_uv;
}
