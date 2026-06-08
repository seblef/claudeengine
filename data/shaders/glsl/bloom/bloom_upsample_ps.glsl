// Fragment shader for the bloom upsample pass.
//
// Applies a 9-tap tent filter to u_src (the smaller mip level) and outputs
// the result scaled by u_intensity. The base accumulation (adding the current
// level) is performed by GPU additive blending in the render state, which
// avoids a texture feedback loop while producing:
//
//   framebuffer[i] = levels[i]_base + tent9(levels[i+1]) * u_intensity
//
// Tent filter kernel (weights sum to 1.0):
//
//   1/16  1/8  1/16
//   1/8   1/4  1/8
//   1/16  1/8  1/16

#version 460 core

layout(binding = 0) uniform sampler2D u_src;
uniform vec2  u_texel_size;
uniform float u_intensity;

in  vec2 v_uv;
out vec4 out_color;

void main() {
    vec3 a = texture(u_src, v_uv + u_texel_size * vec2(-1.0,  1.0)).rgb;
    vec3 b = texture(u_src, v_uv + u_texel_size * vec2( 0.0,  1.0)).rgb;
    vec3 c = texture(u_src, v_uv + u_texel_size * vec2( 1.0,  1.0)).rgb;
    vec3 d = texture(u_src, v_uv + u_texel_size * vec2(-1.0,  0.0)).rgb;
    vec3 e = texture(u_src, v_uv                                   ).rgb;
    vec3 f = texture(u_src, v_uv + u_texel_size * vec2( 1.0,  0.0)).rgb;
    vec3 g = texture(u_src, v_uv + u_texel_size * vec2(-1.0, -1.0)).rgb;
    vec3 h = texture(u_src, v_uv + u_texel_size * vec2( 0.0, -1.0)).rgb;
    vec3 i = texture(u_src, v_uv + u_texel_size * vec2( 1.0, -1.0)).rgb;

    // 9-tap tent filter (bilinear distribution, weights sum to 1.0).
    vec3 filtered = (a + c + g + i) * 0.0625
                  + (b + d + f + h) * 0.125
                  +  e              * 0.25;

    out_color = vec4(filtered * u_intensity, 1.0);
}
