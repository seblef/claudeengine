// Vertex shader for passthrough_color_2d.
// Passes clip-space position, vertex colour and UV to the fragment stage.

#version 460 core
#include <layouts/vertex_2d.glsl>

out vec4 v_color;
out vec2 v_uv;

void main() {
    gl_Position = vec4(in_position.xy, 0.0, 1.0);
    v_color = in_color;
    v_uv    = in_uv;
}
