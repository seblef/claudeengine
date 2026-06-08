// Fragment shader for the bloom downsample pass.
//
// Applies a 13-tap Kawase-style box filter to the source texture.
// At level 0 (u_threshold > 0), a luminance threshold is applied:
//   color = max(color - u_threshold, 0.0)
// At deeper levels (u_threshold == 0) the filter is a plain box blur.
//
// Tap layout (relative to texel_size):
//
//   a . b . c
//   . d . e .
//   f . g . h
//   . i . j .
//   k . l . m
//
// where '.' are unsampled positions. Positions:
//   a=(-2,+2) b=( 0,+2) c=(+2,+2)
//   d=(-1,+1) e=(+1,+1)
//   f=(-2, 0) g=( 0, 0) h=(+2, 0)
//   i=(-1,-1) j=(+1,-1)
//   k=(-2,-2) l=( 0,-2) m=(+2,-2)
//
// Weights:
//   inner box {d,e,i,j} : 0.125 each  → sum 0.5
//   edge midpoints{b,f,h,l}: 0.0625 each → sum 0.25
//   center  g            : 0.125       → sum 0.125
//   corners {a,c,k,m}   : 0.03125 each → sum 0.125
//   total = 1.0

#version 460 core

layout(binding = 0) uniform sampler2D u_src;
uniform vec2  u_texel_size;
uniform float u_threshold;

in  vec2 v_uv;
out vec4 out_color;

void main() {
    vec3 a = texture(u_src, v_uv + u_texel_size * vec2(-2.0,  2.0)).rgb;
    vec3 b = texture(u_src, v_uv + u_texel_size * vec2( 0.0,  2.0)).rgb;
    vec3 c = texture(u_src, v_uv + u_texel_size * vec2( 2.0,  2.0)).rgb;
    vec3 d = texture(u_src, v_uv + u_texel_size * vec2(-1.0,  1.0)).rgb;
    vec3 e = texture(u_src, v_uv + u_texel_size * vec2( 1.0,  1.0)).rgb;
    vec3 f = texture(u_src, v_uv + u_texel_size * vec2(-2.0,  0.0)).rgb;
    vec3 g = texture(u_src, v_uv                                   ).rgb;
    vec3 h = texture(u_src, v_uv + u_texel_size * vec2( 2.0,  0.0)).rgb;
    vec3 i = texture(u_src, v_uv + u_texel_size * vec2(-1.0, -1.0)).rgb;
    vec3 j = texture(u_src, v_uv + u_texel_size * vec2( 1.0, -1.0)).rgb;
    vec3 k = texture(u_src, v_uv + u_texel_size * vec2(-2.0, -2.0)).rgb;
    vec3 l = texture(u_src, v_uv + u_texel_size * vec2( 0.0, -2.0)).rgb;
    vec3 m = texture(u_src, v_uv + u_texel_size * vec2( 2.0, -2.0)).rgb;

    vec3 color = g    * 0.125
               + (d + e + i + j) * 0.125
               + (b + f + h + l) * 0.0625
               + (a + c + k + m) * 0.03125;

    // Luminance threshold: only applied at level 0 (u_threshold > 0).
    color = max(color - u_threshold, vec3(0.0));

    out_color = vec4(color, 1.0);
}
