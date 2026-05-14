// Vertex shader for shadow-map debug thumbnail tiles.
// Scales and offsets the [-1,1] unit quad to a NDC-space tile defined by
// u_tile_min (bottom-left) and u_tile_max (top-right).

#version 460 core
#include <layouts/vertex_3d.glsl>

uniform vec2 u_tile_min;
uniform vec2 u_tile_max;

out vec2 v_uv;

void main() {
    vec2 t  = in_position.xy * 0.5 + 0.5;           // remap [-1,1] to [0,1]
    gl_Position = vec4(mix(u_tile_min, u_tile_max, t), 0.0, 1.0);
    v_uv = t;
}
