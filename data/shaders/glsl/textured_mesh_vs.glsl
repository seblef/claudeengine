// Vertex shader for textured 3D geometry.
// UBO binding 1: per-renderable infos (renderer::RenderableInfos) — world matrix.
// UBO binding 2: per-frame scene infos (renderer::SceneInfos, 352 bytes, std140 row-major).
//   Only view_proj is consumed here; remaining fields are available for other shaders.

#version 460 core
#include <layouts/vertex_3d.glsl>

#include <uniforms/renderable_infos.glsl>
#include <uniforms/scene_infos.glsl>

out vec2 v_uv;

void main() {
    // clip = view_proj * world * position
    gl_Position = view_proj * world * vec4(in_position, 1.0);
    v_uv = in_uv;
}
