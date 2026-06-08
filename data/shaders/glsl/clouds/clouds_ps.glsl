// Procedural cloud layer fragment shader.
//
// Projects a view ray onto a virtual cloud plane at a fixed altitude above the
// camera (kCloudAltitude = 800 m) and samples a 4-octave FBM noise field
// scrolled by the wind vector.  The resulting density is thresholded against
// the per-frame cloud_density uniform and alpha-blended over the incoming sky
// pixel.  Rays directed downward (below the horizon) receive zero cloud alpha.
//
// UBO binding 2: scene_infos (eye_pos)
// UBO binding 7: wind_infos  (wind_xz, wind_time)
// Uniform:       cloud_density  [0, 1]   (0 = clear, 1 = overcast)

#version 460 core
#include <uniforms/scene_infos.glsl>
#include <uniforms/wind_infos.glsl>

in  vec3 v_view_ray;
out vec4 out_color;

uniform float cloud_density;

// Altitude of the virtual cloud plane above the camera (metres).
const float kCloudAltitude = 800.0;
// World-unit scale of the noise field — larger = bigger clouds.
const float kCloudScale    = 400.0;
// UV drift speed in UV-units per second at 1 m/s wind.
const float kCloudSpeed    = 0.0001;

// ---- Value noise helpers ---------------------------------------------------

// 2D hash (0–1).
float Hash2(vec2 p) {
    p  = fract(p * vec2(127.1, 311.7));
    p += dot(p, p + 19.19);
    return fract(p.x * p.y);
}

// Bilinear value noise (0–1).
float ValueNoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);  // smoothstep

    float a = Hash2(i + vec2(0.0, 0.0));
    float b = Hash2(i + vec2(1.0, 0.0));
    float c = Hash2(i + vec2(0.0, 1.0));
    float d = Hash2(i + vec2(1.0, 1.0));

    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

// 4-octave FBM built from ValueNoise.
// Returns a value in [0, 1].
float FBM(vec2 p) {
    float v   = 0.0;
    float amp = 0.5;
    for (int i = 0; i < 4; ++i) {
        v   += amp * ValueNoise(p);
        p   *= 2.1;
        amp *= 0.5;
    }
    return v;
}

// ===========================================================================
void main() {
    vec3 view_dir = normalize(v_view_ray);

    // Rays aimed at or below the horizon carry no clouds.
    if (view_dir.y <= 0.0) {
        out_color = vec4(0.0);
        return;
    }

    // Intersect the view ray with the horizontal cloud plane at eye_pos.y + kCloudAltitude.
    // t = kCloudAltitude / view_dir.y
    float t = kCloudAltitude / view_dir.y;
    vec2  cloud_plane_xz = eye_pos.xz + view_dir.xz * t;

    // Scroll UVs with wind.
    vec2 scroll = wind_xz * wind_time * kCloudSpeed;
    vec2 uv     = cloud_plane_xz / kCloudScale + scroll;

    float density = FBM(uv);

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
