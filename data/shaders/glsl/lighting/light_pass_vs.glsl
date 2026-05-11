// Shared vertex shader for local-light volumes (omni, circle spot, rect spot).
// Included by the per-type _vs.glsl wrappers; also valid as a standalone shader.
//
// Transforms a light volume mesh vertex into clip space and computes the
// screen-space UV used to sample the G-buffer in the fragment shader.
//
// UBO binding 1: per-renderable infos  — world matrix (set to GetVolumeMatrix()).
// UBO binding 2: per-frame scene infos — view_proj.

#version 460 core
#include <layouts/vertex_3d.glsl>

#include <uniforms/renderable_infos.glsl>
#include <uniforms/scene_infos.glsl>

out vec2 v_screen_uv;

void main() {
    vec4 clip   = view_proj * world * vec4(in_position, 1.0);
    gl_Position = clip;
    // Perspective-correct screen UV for G-buffer sampling.
    v_screen_uv = (clip.xy / clip.w) * 0.5 + 0.5;
}
