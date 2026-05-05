// Fragment shader for passthrough_color_2d.
// Outputs a uniform tint color supplied via the ColorBlock constant buffer.

#version 460 core

// Per-draw color tint supplied by the CPU via a constant/uniform buffer.
// binding = 0 matches ConstantBuffer slot 0 set by the caller.
layout(std140, binding = 0) uniform ColorBlock {
  vec4 tint_color;
};

out vec4 frag_color;

void main() {
  frag_color = tint_color;
}
