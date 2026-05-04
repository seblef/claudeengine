// Fragment shader for passthrough_color_2d.
// Outputs the interpolated vertex colour unchanged.

#version 460 core

in vec4 v_color;

out vec4 frag_color;

void main() {
    frag_color = v_color;
}
