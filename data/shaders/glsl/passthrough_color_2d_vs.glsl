// Vertex shader for passthrough_color_2d.
// Passes clip-space position, vertex colour and UV to the fragment stage.

#version 460 core

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_uv;

out vec4 v_color;
out vec2 v_uv;

void main() {
    gl_Position = vec4(in_position.xy, 0.0, 1.0);
    v_color = in_color;
    v_uv    = in_uv;
}
