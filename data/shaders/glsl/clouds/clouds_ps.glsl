// Procedural cloud layer fragment shader.
//
// Projects a view ray onto a virtual cloud plane at a fixed altitude above the
// camera (kCloudAltitude = 800 m) and samples a 4-octave FBM noise field
// scrolled by the wind vector.  Domain warping (Inigo Quilez's technique) is
// applied before the density FBM: a first FBM pass produces a 2D warp vector
// that displaces the UVs fed to the density FBM, breaking self-similar tiling
// and producing swirling, organic cloud formations at the cost of one extra
// FBM evaluation (~4 additional value-noise samples) per fragment.
// The resulting density is thresholded against the per-frame cloud_density
// uniform and alpha-blended over the incoming sky pixel.  Rays directed
// downward (below the horizon) receive zero cloud alpha.
//
// UBO binding 2: scene_infos (eye_pos)
// UBO binding 7: wind_infos  (wind_xz, wind_time, wind_displacement)
// Uniform:       cloud_density  [0, 1]   (0 = clear, 1 = overcast)

#version 460 core
#include <uniforms/scene_infos.glsl>
#include <uniforms/wind_infos.glsl>
#include <clouds/cloud_density.glsl>

in  vec3 v_view_ray;
out vec4 out_color;

uniform float cloud_density;

void main() {
    vec3 view_dir = normalize(v_view_ray);

    // Rays aimed at or below the horizon carry no clouds.
    if (view_dir.y <= 0.0) {
        out_color = vec4(0.0);
        return;
    }

    // Intersect the view ray with the horizontal cloud plane at eye_pos.y + kCloudAltitude.
    // t = kCloudAltitude / view_dir.y
    float t              = kCloudAltitude / view_dir.y;
    vec2  cloud_plane_xz = eye_pos.xz + view_dir.xz * t;

    float density = CloudDensity(cloud_plane_xz);

    // Remap so that cloud_density controls the coverage threshold:
    //   cloud_density = 0 → all density values below threshold → clear sky
    //   cloud_density = 1 → all density values above threshold → overcast
    // Use a smooth ramp width of 0.15 to avoid hard edges.
    const float kEdge = 0.15;
    float alpha = smoothstep(1.0 - cloud_density - kEdge,
                             1.0 - cloud_density + kEdge,
                             density);

    // Fade clouds near the horizon to avoid a sharp cutoff.
    float horizon_fade = smoothstep(0.0, 0.08, view_dir.y);
    alpha *= horizon_fade;

    // White cloud colour, alpha-blended over the incoming sky pixel.
    out_color = vec4(1.0, 1.0, 1.0, alpha);
}
