// Fragment shader for the particle forward pass (kAdditive and kAlphaBlend emitters).
// Outputs to a single HDR render target (no MRT).
//
// Frame interpolation: samples both the current (v_uv) and next (v_uv2) sprite-sheet
// frames and blends between them:
//   tex_color = mix(texture(u_texture, v_uv), texture(u_texture, v_uv2), v_frame_blend)
// When v_frame_blend == 0 (smooth_transition disabled), only the current frame is used.
//
// When u_lit is true (kAlphaBlend + lit), applies a forward directional-light
// approximation consistent with the G-buffer particle normal:
//   N       = -view[2].xyz  (world-space camera-facing billboard normal)
//   L       = normalize(-direction)       // surface-to-light
//   diff    = max(dot(N, L), 0.0)
//   lighting = ambient + color * intensity * diff
//   result   = tex_color.rgb * v_color.rgb * lighting
//
// kAdditive particles never apply lighting (emissive by nature).
//
// UBO binding 2: SceneInfosBlock (view matrix, row_major).
// UBO binding 4: LightInfosBlock (direction, color, intensity, ambient).

#version 460 core

#include <uniforms/scene_infos.glsl>
#include <uniforms/light_infos.glsl>

in vec2 v_uv;
in vec2 v_uv2;
in float v_frame_blend;
in vec4 v_color;

layout(binding = 0) uniform sampler2D u_texture;
uniform int u_lit;  // 1 = apply directional lighting, 0 = unlit

layout(location = 0) out vec4 out_color;

void main() {
    // Blend between the current and next sprite-sheet frame.
    vec4 tex_color = mix(texture(u_texture, v_uv), texture(u_texture, v_uv2), v_frame_blend);

    vec4 final_color = tex_color * v_color;

    if (u_lit != 0) {
        // World-space camera-facing normal — the view matrix third row encodes
        // the camera Z axis in world space; negating it gives the toward-viewer direction.
        // Equation matches particle_gbuffer_ps.glsl normal computation.
        vec3 N    = -normalize(vec3(view[2][0], view[2][1], view[2][2]));
        vec3 L    = normalize(-direction);
        float diff = max(dot(N, L), 0.0);
        vec3 lighting = ambient + color * intensity * diff;
        final_color.rgb *= lighting;
    }

    out_color = final_color;
}
