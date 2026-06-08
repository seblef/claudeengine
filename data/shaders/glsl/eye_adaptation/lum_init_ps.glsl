// Fragment shader for the eye-adaptation luminance initialisation pass.
// Converts an RGB HDR sample into log-luminance (R16F output).
//
// Luminance (BT.709):  L = dot(rgb, vec3(0.2126, 0.7152, 0.0722))
// Log-luminance:       log(max(L, 1e-4))  — floor prevents log(0) → -inf

#version 460 core

layout(binding = 0) uniform sampler2D u_hdr;

in  vec2  v_uv;
out float out_log_lum;

void main() {
    vec3  hdr = texture(u_hdr, v_uv).rgb;
    float lum = dot(hdr, vec3(0.2126, 0.7152, 0.0722));
    out_log_lum = log(max(lum, 0.0001));
}
