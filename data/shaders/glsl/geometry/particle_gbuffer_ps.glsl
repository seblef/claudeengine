// Fragment shader for the particle G-buffer geometry pass (kGBuffer blend mode).
// Solid particles are written into the three G-buffer MRTs so they receive
// full deferred lighting for free.
//
// Outputs:
//   location 0 (Albedo,  RGBA8)   — texture_color.rgb * color.rgb
//   location 1 (Normal,  RGBA16F) — world-space camera-facing normal, encoded [0,1]
//   location 2 (Specular, RGBA8)  — zero (particles are non-specular by default)
//
// Frame interpolation: samples both the current (v_uv) and next (v_uv2) sprite-sheet
// frames and blends between them:
//   tex_color = mix(texture(u_texture, v_uv), texture(u_texture, v_uv2), v_frame_blend)
// When v_frame_blend == 0 (smooth_transition disabled), only the current frame is used.
//
// Alpha test: discard if tex_color.a * color.a < 0.5 (keeps particles opaque).
//
// The world-space camera-facing normal is derived from the view matrix third row
// (negated), which is the direction pointing toward the viewer.
// Equation: view_normal_world = (view^T * (0,0,1)) = col 2 of view
//           = vec3(view[0][2], view[1][2], view[2][2]) in row_major layout,
//           then negated so it points toward the camera.

#version 460 core

#include <uniforms/scene_infos.glsl>

in vec2 v_uv;
in vec2 v_uv2;
in float v_frame_blend;
in vec4 v_color;

layout(binding = 0) uniform sampler2D u_texture;

layout(location = 0) out vec4 out_albedo;
layout(location = 1) out vec4 out_normal;
layout(location = 2) out vec4 out_specular;

void main() {
    // Blend between the current and next sprite-sheet frame.
    vec4 tex_color = mix(texture(u_texture, v_uv), texture(u_texture, v_uv2), v_frame_blend);

    // Alpha test — discard transparent fragments.
    if (tex_color.a * v_color.a < 0.5)
        discard;

    // Albedo: modulate texture color by particle tint.
    out_albedo = vec4(tex_color.rgb * v_color.rgb, 0.0);

    // Normal: world-space direction pointing toward the camera.
    // The view matrix row_major third row encodes the camera Z axis in world
    // space; negating it gives the toward-viewer direction.
    vec3 world_normal = -normalize(vec3(view[2][0], view[2][1], view[2][2]));
    out_normal = vec4(world_normal * 0.5 + 0.5, 0.0);

    // Particles carry no specular contribution.
    out_specular = vec4(0.0);
}
