// Terrain tessellation-control shader.
//
// Processes one quad patch (4 control points). Computes per-edge tessellation
// factors based on camera distance so nearby patches are subdivided more:
//
//   tess_factor(dist) = mix(max_tess, 1.0, clamp(dist / tess_falloff_dist, 0, 1))
//
// Edge assignment (quads layout):
//   Edge 0 (left,   u=0): cp[0] ↔ cp[3]
//   Edge 1 (bottom, v=0): cp[0] ↔ cp[1]
//   Edge 2 (right,  u=1): cp[1] ↔ cp[2]
//   Edge 3 (top,    v=1): cp[2] ↔ cp[3]
//
// Inner levels are the average of their two opposing outer edges.
//
// Control-point data is passed through unchanged; displacement happens in the
// tessellation-evaluation shader.
//
// UBO binding 2: scene_infos   (eye_pos)
// UBO binding 6: terrain_patch_infos (tess_falloff_dist, max_tess)

#version 460 core

layout(vertices = 4) out;

#include <uniforms/scene_infos.glsl>
#include <uniforms/terrain_patch_infos.glsl>

in vec3 v_world_pos[];
in vec3 v_world_normal[];
in vec2 v_world_uv[];

out vec3 tc_world_pos[];
out vec3 tc_world_normal[];
out vec2 tc_world_uv[];

// Returns camera distance for a world-space point.
float CamDist(vec3 p) {
    return length(p - eye_pos);
}

// Maps camera distance to a tessellation factor in [1, max_tess].
float TessFactor(float dist) {
    return mix(max_tess, 1.0, clamp(dist / tess_falloff_dist, 0.0, 1.0));
}

void main() {
    // Pass through this invocation's control point unchanged.
    tc_world_pos[gl_InvocationID]    = v_world_pos[gl_InvocationID];
    tc_world_normal[gl_InvocationID] = v_world_normal[gl_InvocationID];
    tc_world_uv[gl_InvocationID]     = v_world_uv[gl_InvocationID];
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;

    // Tessellation levels only need to be written once per patch.
    if (gl_InvocationID != 0) return;

    // Per-edge factor: average camera distance of the edge's two endpoints.
    float d0 = (CamDist(v_world_pos[0]) + CamDist(v_world_pos[3])) * 0.5; // left
    float d1 = (CamDist(v_world_pos[0]) + CamDist(v_world_pos[1])) * 0.5; // bottom
    float d2 = (CamDist(v_world_pos[1]) + CamDist(v_world_pos[2])) * 0.5; // right
    float d3 = (CamDist(v_world_pos[2]) + CamDist(v_world_pos[3])) * 0.5; // top

    gl_TessLevelOuter[0] = TessFactor(d0);
    gl_TessLevelOuter[1] = TessFactor(d1);
    gl_TessLevelOuter[2] = TessFactor(d2);
    gl_TessLevelOuter[3] = TessFactor(d3);

    // Inner levels are averages of opposing outer edges.
    gl_TessLevelInner[0] = (gl_TessLevelOuter[1] + gl_TessLevelOuter[3]) * 0.5;
    gl_TessLevelInner[1] = (gl_TessLevelOuter[0] + gl_TessLevelOuter[2]) * 0.5;
}
