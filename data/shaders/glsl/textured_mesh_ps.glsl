// Fragment shader for textured 3D geometry.

#version 460 core

in vec2 v_uv;

layout(binding = 0) uniform sampler2D u_texture;

out vec4 frag_color;

void main() {
    frag_color = texture(u_texture, v_uv);
}
