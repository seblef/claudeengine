// Fragment shader for successive log-luminance downsampling.
//
// 4-tap bilinear average: samples at ±0.5 texel half-offsets so that the
// hardware bilinear filter contributes additional smoothing for free.
//
// Output is the arithmetic mean of the four fetches (valid because the
// source is already in log-space, making the average a geometric-mean proxy).

#version 460 core

layout(binding = 0) uniform sampler2D u_src;
uniform vec2 u_texel_size;

in  vec2  v_uv;
out float out_log_lum;

void main() {
    float a = texture(u_src, v_uv + u_texel_size * vec2(-0.5, -0.5)).r;
    float b = texture(u_src, v_uv + u_texel_size * vec2( 0.5, -0.5)).r;
    float c = texture(u_src, v_uv + u_texel_size * vec2(-0.5,  0.5)).r;
    float d = texture(u_src, v_uv + u_texel_size * vec2( 0.5,  0.5)).r;
    out_log_lum = (a + b + c + d) * 0.25;
}
