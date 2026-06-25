// Vertex shader for tire track ground decals.
//
// Inputs come from VertexTrack layout:
//   location 0: position (vec3) — world-space quad corner
//   location 1: uv       (vec2) — texture coordinate
//   location 2: alpha    (float) — per-vertex fade factor [0, 1]
//
// A small Y offset (kZFightBias) is added in world space along world-up before
// the MVP transform.  This avoids z-fighting with the underlying geometry
// without storing a per-quad contact normal.
//
// UBO binding 2: SceneInfosBlock — view_proj matrix.

#version 460 core

layout(location = 0) in vec3  in_position;
layout(location = 1) in vec2  in_uv;
layout(location = 2) in float in_alpha;

#include <uniforms/scene_infos.glsl>

out vec2  v_uv;
out float v_alpha;

const float kZFightBias = 0.005;  // metres pushed up along world-up (0,1,0)

void main() {
    vec3 biased = in_position + vec3(0.0, kZFightBias, 0.0);
    gl_Position = view_proj * vec4(biased, 1.0);
    v_uv    = in_uv;
    v_alpha = in_alpha;
}
