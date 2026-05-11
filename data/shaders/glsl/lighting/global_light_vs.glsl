// Vertex shader for the GlobalLight fullscreen quad pass.
// The quad vertices are already in NDC space — no world/VP transform is needed.
// UVs are derived directly from position so no UV attribute is required.
//
// UBO bindings: none needed (no per-object or scene transform applied here).

#version 460 core
#include <layouts/vertex_3d.glsl>

out vec2 v_screen_uv;

void main() {
    gl_Position = vec4(in_position.xy, 0.0, 1.0);
    v_screen_uv = in_position.xy * 0.5 + 0.5;
}
