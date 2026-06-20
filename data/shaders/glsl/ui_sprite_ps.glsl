// Fragment shader for 2D UI sprites.
// Samples the sprite texture and multiplies by the per-vertex tint.
// Alpha blending (SrcAlpha / OneMinusSrcAlpha) is enabled by the renderer.

#version 460 core

uniform sampler2D u_sprite;

in vec2 v_uv;
in vec4 v_tint;

out vec4 out_color;

void main() {
    out_color = texture(u_sprite, v_uv) * v_tint;
}
