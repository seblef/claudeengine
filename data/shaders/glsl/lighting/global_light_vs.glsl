// Vertex shader for the GlobalLight fullscreen quad pass.
// The quad vertices are already in NDC space — no world/VP transform is needed.
// Screen-space UVs are derived from gl_FragCoord in the fragment shader.
//
// UBO bindings: none needed (no per-object or scene transform applied here).

#version 460 core
#include <layouts/vertex_3d.glsl>

void main() {
    gl_Position = vec4(in_position.xy, 0.0, 1.0);
}
