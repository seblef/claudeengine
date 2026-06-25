// Fragment shader for tire track ground decals.
//
// Samples the track texture (grayscale alpha mask) at slot 0.
// The final alpha is albedo.a * v_alpha so the track fades smoothly as
// v_alpha decays from 1.0 (fresh) to 0.0 (fully faded).
// No lighting is applied — tracks are flat decals.
//
// Output is blended SrcAlpha / OneMinusSrcAlpha into the HDR RT by the caller.

#version 460 core

in vec2  v_uv;
in float v_alpha;

layout(binding = 0) uniform sampler2D track_tex;

layout(location = 0) out vec4 out_color;

void main() {
    vec4 albedo = texture(track_tex, v_uv);
    out_color   = vec4(albedo.rgb, albedo.a * v_alpha);
}
