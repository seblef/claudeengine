// Fragment shader for the composite (HDR → screen) pass.
// Reads the HDR accumulation buffer and applies gamma correction.
// No debug branches — debug views use a separate debug_blit shader.
//
// Gamma: linear → sRGB approximation (γ = 2.2).
//   out = pow(hdr, 1/2.2)

#version 460 core

layout(binding = 0) uniform sampler2D u_hdr;

in  vec2 v_uv;
out vec4 out_color;

void main() {
    vec3 hdr = texture(u_hdr, v_uv).rgb;
    out_color = vec4(pow(hdr, vec3(1.0 / 2.2)), 1.0);
}
