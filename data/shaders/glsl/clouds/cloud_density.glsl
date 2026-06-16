// Shared cloud density functions — included by clouds_ps.glsl and cloud_shadow_ps.glsl.
//
// Provides a 4-octave FBM noise field with domain warping used to model
// procedural cloud density on a virtual plane at kCloudAltitude.
//
// Requires:
//   uniforms/wind_infos.glsl   (wind_displacement)

// Altitude of the virtual cloud plane above the camera (metres).
const float kCloudAltitude = 800.0;
// World-unit scale of the noise field — larger = bigger clouds.
const float kCloudScale    = 400.0;
// Strength of the domain warp displacement applied to density UVs.
const float kWarpScale     = 0.8;

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

// Rotation matrix (~30°) applied between FBM octaves to break grid alignment.
// cos(30°) ≈ 0.866, sin(30°) = 0.5.
const mat2 kRot = mat2(0.866, -0.5, 0.5, 0.866);

// 4-octave FBM built from ValueNoise.
// Returns a value in [0, 1].
// A 30° rotation is applied at each octave so successive frequency bands
// sample along different axes, eliminating axis-aligned banding.
float FBM(vec2 p) {
    float v   = 0.0;
    float amp = 0.5;
    for (int i = 0; i < 4; ++i) {
        v   += amp * ValueNoise(p);
        p    = kRot * p * 2.1;
        amp *= 0.5;
    }
    return v;
}

// Evaluates warped cloud density at the given world XZ position.
// Applies domain warping (Inigo Quilez) before the density FBM.
// wind_displacement (from WindInfosBlock) scrolls the UV over time.
// Returns a raw density in [0, ~0.93] before coverage thresholding.
float CloudDensity(vec2 world_xz) {
    vec2 uv   = world_xz / kCloudScale + wind_displacement / kCloudScale;
    vec2 warp = vec2(FBM(uv), FBM(uv + vec2(5.2, 1.3)));
    return FBM(uv + kWarpScale * warp);
}
