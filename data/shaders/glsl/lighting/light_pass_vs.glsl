// Shared vertex shader for local-light volumes (omni, circle spot, rect spot).
// Included by the per-type _vs.glsl wrappers; also valid as a standalone shader.
//
// Transforms a light volume mesh vertex into clip space.  Screen-space UVs for
// G-buffer sampling are derived from gl_FragCoord in each fragment shader — this
// avoids the incorrect double-perspective-correction that occurs when a
// pre-divided NDC quantity is passed as a perspective-interpolated varying.
//
// UBO binding 1: per-renderable infos  — world matrix (set to GetVolumeMatrix()).
// UBO binding 2: per-frame scene infos — view_proj.

#version 460 core
#include <layouts/vertex_3d.glsl>

#include <uniforms/renderable_infos.glsl>
#include <uniforms/scene_infos.glsl>

void main() {
    gl_Position = view_proj * world * vec4(in_position, 1.0);
}
