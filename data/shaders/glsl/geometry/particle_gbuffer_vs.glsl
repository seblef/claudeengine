// Vertex shader for the particle G-buffer geometry pass.
// Expands billboard quads in view space from per-vertex center/size/color/uv data.
//
// All four vertices of one quad share the same center, size, color, and
// uv_offset. The corner is determined by gl_VertexID % 4:
//   0 = bottom-left, 1 = bottom-right, 2 = top-right, 3 = top-left
//
// Billboard right and up vectors are extracted from the view matrix rows so
// the quad always faces the camera regardless of world orientation.
//
// UBO binding 2: SceneInfosBlock — view, proj matrices.

#version 460 core

// kParticle layout: location 0=center(vec3), 1=size(float),
//                  2=color(vec4), 3=uv_offset(vec2).
layout(location = 0) in vec3  in_center;
layout(location = 1) in float in_size;
layout(location = 2) in vec4  in_color;
layout(location = 3) in vec2  in_uv_offset;

#include <uniforms/scene_infos.glsl>

uniform vec2 u_uv_size;  // vec2(1/sprite_cols, 1/sprite_rows)

out vec2 v_uv;
out vec4 v_color;

// Local UV for each corner of the quad.
// Corner order: BL, BR, TR, TL
const vec2 kCornerUV[4] = vec2[4](
    vec2(0.0, 1.0),
    vec2(1.0, 1.0),
    vec2(1.0, 0.0),
    vec2(0.0, 0.0)
);

// Signed offsets in the billboard plane for each corner (half-extents).
const vec2 kCornerOffset[4] = vec2[4](
    vec2(-0.5, -0.5),
    vec2( 0.5, -0.5),
    vec2( 0.5,  0.5),
    vec2(-0.5,  0.5)
);

void main() {
    int corner = gl_VertexID % 4;

    // Extract camera right and up from the view matrix rows (row_major).
    // view[0].xyz = right vector in world space
    // view[1].xyz = up vector in world space
    vec3 right = vec3(view[0][0], view[0][1], view[0][2]);
    vec3 up    = vec3(view[1][0], view[1][1], view[1][2]);

    // Expand the billboard center to a world-space corner position.
    vec2 co = kCornerOffset[corner];
    vec3 world_pos = in_center
                   + right * (co.x * in_size)
                   + up    * (co.y * in_size);

    gl_Position = proj * view * vec4(world_pos, 1.0);

    // Map local corner UV into the sprite-sheet cell.
    v_uv    = in_uv_offset + kCornerUV[corner] * u_uv_size;
    v_color = in_color;
}
