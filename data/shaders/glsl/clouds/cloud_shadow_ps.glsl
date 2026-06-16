// Cloud shadow map fragment shader.
//
// Renders cloud density top-down into a world-space R16F render target centred
// on the camera.  Each texel stores a shadow density in [0, 1] derived from
// the same FBM used by clouds_ps.glsl, sampled at the cloud plane altitude.
//
// The resulting texture is sampled by global_light_ps.glsl with a sun-direction
// offset to project soft cloud shadows onto lit ground fragments.
//
// UBO binding 2: scene_infos (eye_pos)
// UBO binding 7: wind_infos  (wind_displacement)
// Uniform: cloud_density         [0, 1]  coverage threshold
// Uniform: cloud_shadow_coverage float   half-extent of the covered area (metres)

#version 460 core
#include <uniforms/scene_infos.glsl>
#include <uniforms/wind_infos.glsl>
#include <clouds/cloud_density.glsl>

in  vec2  v_uv;
out float out_shadow;

uniform float cloud_density;
uniform float cloud_shadow_coverage;

void main() {
    // Map UV [0,1]² to world XZ centred on camera with ±cloud_shadow_coverage extent.
    vec2 world_xz = eye_pos.xz + (v_uv * 2.0 - 1.0) * cloud_shadow_coverage;

    float density = CloudDensity(world_xz);

    // Apply the same coverage threshold as clouds_ps.glsl so shadow density
    // matches the visual cloud coverage.
    const float kEdge = 0.15;
    out_shadow = smoothstep(1.0 - cloud_density - kEdge,
                            1.0 - cloud_density + kEdge,
                            density);
}
