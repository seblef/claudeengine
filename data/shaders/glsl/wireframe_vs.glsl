// Vertex shader for the wireframe pass.
// Vertices are supplied in world space; only the VP transform is applied.
// UBO binding 2: SceneInfosBlock — only view_proj is consumed here.

#version 460 core
#include <layouts/base_vertex.glsl>
#include <uniforms/scene_infos.glsl>

out vec4 v_color;

void main() {
    gl_Position = view_proj * vec4(in_position, 1.0);
    v_color = in_color;
}
