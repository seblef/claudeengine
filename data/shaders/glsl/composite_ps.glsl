// Fragment shader for the composite (HDR → screen) pass.
// Applies ACES filmic tone mapping followed by gamma correction (γ = 2.2).
// No debug branches — debug views use a separate debug_blit shader.
//
// Pipeline:
//   hdr  = texture(u_hdr)
//   hdr *= u_exposure                        // pre-scale
//   ldr  = AcesFilmic(hdr)                   // HDR → [0,1]
//   out  = pow(ldr, 1/2.2)                   // linear → sRGB
//
// ACES filmic approximation (Krzysztof Narkowicz 2015):
//   f(x) = clamp((x(ax+b)) / (x(cx+d)+e), 0, 1)
//   a=2.51, b=0.03, c=2.43, d=0.59, e=0.14

#version 460 core

#include <uniforms/post_process_infos.glsl>

layout(binding = 0) uniform sampler2D u_hdr;

in  vec2 v_uv;
out vec4 out_color;

// ACES filmic approximation (Krzysztof Narkowicz 2015).
vec3 AcesFilmic(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 hdr = texture(u_hdr, v_uv).rgb;
    hdr *= u_exposure;
    vec3 ldr = AcesFilmic(hdr);
    out_color = vec4(pow(ldr, vec3(1.0 / 2.2)), 1.0);
}
