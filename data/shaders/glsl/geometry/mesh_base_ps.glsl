// Fragment shader for base mesh rendering. Samples the diffuse texture.

#version 460 core

in vec2 v_uv;

layout(binding = 0) uniform sampler2D u_diffuse;

out vec4 frag_color;

void main() {
    frag_color = texture(u_diffuse, v_uv);
}
