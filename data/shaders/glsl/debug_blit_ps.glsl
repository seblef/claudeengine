// Fragment shader for the G-buffer debug blit pass.
// Samples the selected G-buffer RT and remaps it for display.
// This shader is never part of the production render path.
//
// u_debug_mode values:
//   1 = albedo   — RGB pass-through
//   2 = normal   — encoded [0,1] → decoded [-1,1] displayed as colour
//   3 = specular — albedo.a = spec_intensity displayed as greyscale
//   4 = depth    — linearized from NDC depth using z_near/z_far from SceneInfos

#version 460 core
#include <uniforms/scene_infos.glsl>

layout(binding = 0) uniform sampler2D u_source;
uniform int u_debug_mode;

in  vec2 v_uv;
out vec4 out_color;

void main() {
    vec4 s = texture(u_source, v_uv);

    if (u_debug_mode == 1) {
        // Albedo: diffuse colour, pass-through.
        out_color = vec4(s.rgb, 1.0);
    } else if (u_debug_mode == 2) {
        // Normal: encoded as N*0.5+0.5 in G-buffer — decode to [-1,1] for display.
        out_color = vec4(s.rgb * 2.0 - 1.0, 1.0);
    } else if (u_debug_mode == 3) {
        // Specular: spec_intensity packed in albedo.a — display as greyscale.
        out_color = vec4(vec3(s.a), 1.0);
    } else {
        // Depth: NDC z → linear view-space depth → normalized [0,1].
        //   lin = (2 * near * far) / (far + near - z_ndc * (far - near))
        float z   = s.r * 2.0 - 1.0;
        float lin = (2.0 * z_near * z_far) / (z_far + z_near - z * (z_far - z_near));
        out_color = vec4(vec3(lin / z_far), 1.0);
    }
}
