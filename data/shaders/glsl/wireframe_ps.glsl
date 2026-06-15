// Fragment shader for the wireframe pass.
// Outputs the interpolated vertex colour directly; no texture sampling.

#version 460 core

in vec4 v_color;

out vec4 frag_color;

void main() {
    frag_color = v_color;
}
