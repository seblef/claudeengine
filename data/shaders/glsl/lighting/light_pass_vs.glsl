// Shared vertex shader body for local-light volumes (omni, circle spot, rect spot).
// This file is a plain include — it has no #version and no header guard.
// Each per-type _vs.glsl prepends "#version 460 core" before including this.
//
// Transforms a light volume mesh vertex into clip space and computes the
// screen-space UV used to sample the G-buffer in the fragment shader.
//
// UBO binding 1: per-renderable infos  — world matrix (set to GetVolumeMatrix()).
// UBO binding 2: per-frame scene infos — view_proj.
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
