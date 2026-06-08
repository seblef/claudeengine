// Vertex shader for the bloom upsample pass.
// Passes clip-space position and UV to the fragment shader.
// Uses the same fullscreen NDC quad as the composite pass.

#version 460 core
#include <layouts/vertex_3d.glsl>

out vec2 v_uv;

void main() {
    gl_Position = vec4(in_position.xy, 0.0, 1.0);
    v_uv = in_position.xy * 0.5 + 0.5;
}
