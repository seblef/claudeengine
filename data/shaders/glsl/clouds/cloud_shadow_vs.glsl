// Cloud shadow map vertex shader — fullscreen quad passthrough.
//
// Outputs a UV in [0, 1]² covering the world-space shadow coverage area
// centred on the camera.  The fragment shader derives world XZ from this UV
// and the cloud_shadow_coverage uniform.

#version 460 core
#include <layouts/vertex_3d.glsl>

out vec2 v_uv;

void main() {
    gl_Position = vec4(in_position.xy, 0.0, 1.0);
    v_uv        = in_position.xy * 0.5 + 0.5;
}
