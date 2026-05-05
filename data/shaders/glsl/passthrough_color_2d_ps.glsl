// Fragment shader for passthrough_color_2d.
// Samples a 2D texture at the interpolated UV coordinates.

#version 460 core

in vec2 v_uv;

// binding = 0 matches the texture slot set by the caller.
layout(binding = 0) uniform sampler2D u_texture;

out vec4 frag_color;

void main() {
    frag_color = texture(u_texture, v_uv);
}
