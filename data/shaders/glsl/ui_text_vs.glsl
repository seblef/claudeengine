// Vertex shader for 2D UI text glyphs.
// Input positions are in screen pixels (top-left origin, y-down).
// The orthographic matrix in slot 4 maps pixels to NDC.

#version 460 core
#include <layouts/base_vertex.glsl>
#include <uniforms/ui_projection_infos.glsl>

out vec2 v_uv;
out vec4 v_color;

void main() {
    gl_Position = u_ortho * vec4(in_position.xy, 0.0, 1.0);
    v_uv    = in_uv;
    v_color = in_color;
}
