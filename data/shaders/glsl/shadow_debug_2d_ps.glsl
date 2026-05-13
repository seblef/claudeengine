// Fragment shader for 2D shadow-map debug thumbnails.
// Samples a depth texture with compare mode disabled (bound as sampler2D,
// not sampler2DShadow) and displays the raw depth as greyscale.
// For orthographic projections (CSM) the depth is already linear.
// For perspective projections (spot lights) it is non-linear but still
// useful for verifying coverage and caster geometry.

#version 460 core
layout(binding = 0) uniform sampler2D u_depth;

in  vec2 v_uv;
out vec4 out_color;

void main() {
    float d = texture(u_depth, v_uv).r;
    out_color = vec4(d, d, d, 1.0);
}
