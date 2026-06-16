// Vertex shader for the particle forward pass (kAdditive and kAlphaBlend emitters).
// Expands billboard quads in view space from per-vertex center/size/color/uv data.
//
// All four vertices of one quad share the same center, size, color, and
// uv_offset. The corner is determined by gl_VertexID % 4:
//   0 = bottom-left, 1 = bottom-right, 2 = top-right, 3 = top-left
//
// Billboard right and up vectors are extracted from the view matrix rows so
// the quad always faces the camera regardless of world orientation.
//
// Smooth frame interpolation: when in_frame_blend > 0, the fragment shader blends
// the sample at v_uv toward the sample at v_uv2 using mix(sample0, sample1, blend).
//
// UBO binding 2: SceneInfosBlock — view, proj matrices.

#version 460 core

// kParticle layout: location 0=center(vec3), 1=size(float),
//                  2=color(vec4), 3=uv_offset(vec2), 4=rotation(float),
//                  5=uv_offset2(vec2), 6=frame_blend(float).
layout(location = 0) in vec3  in_center;
layout(location = 1) in float in_size;
layout(location = 2) in vec4  in_color;
layout(location = 3) in vec2  in_uv_offset;
layout(location = 4) in float in_rotation;
layout(location = 5) in vec2  in_uv_offset2;
layout(location = 6) in float in_frame_blend;

#include <uniforms/scene_infos.glsl>

uniform vec2 u_uv_size;  // vec2(1/sprite_cols, 1/sprite_rows)

out vec2 v_uv;
out vec2 v_uv2;
out float v_frame_blend;
out vec4 v_color;

// Local UV for each corner of the quad.
// Corner order: BL, BR, TR, TL
// V=0 is the bottom of the texture (stb_image loads with flip, so V=0 = image bottom).
const vec2 kCornerUV[4] = vec2[4](
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(1.0, 1.0),
    vec2(0.0, 1.0)
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

    // Transform the particle center to view space.
    // In view space the axes are already camera-aligned (x = right, y = up),
    // so expanding the billboard here guarantees screen-facing quads regardless
    // of camera orientation.
    vec4 view_center = view * vec4(in_center, 1.0);

    // Rotate corner offset in the billboard plane by in_rotation (radians).
    // rot(θ) * co = (cos θ·co.x − sin θ·co.y, sin θ·co.x + cos θ·co.y)
    vec2 co = kCornerOffset[corner];
    float cos_r = cos(in_rotation);
    float sin_r = sin(in_rotation);
    vec2 rotated_co = vec2(co.x * cos_r - co.y * sin_r,
                           co.x * sin_r + co.y * cos_r);
    vec4 view_pos = view_center
                  + vec4(rotated_co.x * in_size, rotated_co.y * in_size, 0.0, 0.0);

    gl_Position = proj * view_pos;

    // Map local corner UV into the sprite-sheet cells (current and next frame).
    vec2 corner_uv = kCornerUV[corner] * u_uv_size;
    v_uv          = in_uv_offset  + corner_uv;
    v_uv2         = in_uv_offset2 + corner_uv;
    v_frame_blend = in_frame_blend;
    v_color       = in_color;
}
