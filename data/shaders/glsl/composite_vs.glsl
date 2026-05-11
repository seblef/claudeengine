// Vertex shader for the composite (HDR → screen) pass.
// Uses the same NDC fullscreen quad as the GlobalLight pass.
// Sampler slot 0: HDR render target (RGBA16F).

#version 460 core
#include <layouts/vertex_3d.glsl>

out vec2 v_uv;

void main() {
    gl_Position = vec4(in_position.xy, 0.0, 1.0);
    v_uv = in_position.xy * 0.5 + 0.5;
}
