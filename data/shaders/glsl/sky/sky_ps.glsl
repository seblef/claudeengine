// Procedural sky fragment shader — Preetham atmospheric scattering model.
// Reference: Preetham et al., "A Practical Analytic Model for Daylight", 1999.
//
// Pipeline: daytime Rayleigh + Mie sky → HDR sun disc → moon disc → star noise.
// All values are output as linear HDR (no gamma); the composite pass applies γ.
//
// UBO binding 2: scene_infos
// UBO binding 8: sky_infos (sun_direction, time_of_day, turbidity)

#version 460 core
#include <uniforms/scene_infos.glsl>
#include <uniforms/sky_infos.glsl>

in  vec3 v_view_ray;
out vec4 out_color;

const float PI = 3.14159265358979;

// ---- Preetham Perez distribution function ----------------------------------
//
// F(cos_theta, gamma, cos_gamma) = (1 + A·exp(B/cos_theta))
//                                 · (1 + C·exp(D·gamma) + E·cos²(gamma))
//
// cos_theta : cosine of view-zenith angle (view direction vs. +Y)
// gamma     : angle between view direction and sun direction (radians)
// cos_gamma : cosine of gamma
float PerezF(float cos_theta, float gamma, float cos_gamma,
             float A, float B, float C, float D, float E) {
    return (1.0 + A * exp(B / max(cos_theta, 0.001))) *
           (1.0 + C * exp(D * gamma) + E * cos_gamma * cos_gamma);
}

// ---- CIE Yxy → linear sRGB ------------------------------------------------
vec3 YxyToRGB(float Y, float x, float y) {
    if (y < 1e-5 || Y < 1e-5) return vec3(0.0);
    float X = Y / y * x;
    float Z = Y / y * (1.0 - x - y);
    // XYZ to linear sRGB, column-major (D65, sRGB primaries).
    mat3 m = mat3(
         3.2404542, -0.9692660,  0.0556434,
        -1.5371385,  1.8760108, -0.2040259,
        -0.4985314,  0.0415560,  1.0572252
    );
    return max(m * vec3(X, Y, Z), vec3(0.0));
}

// ---- Preetham sky colour for a single view direction ----------------------
//
// view_dir: normalized view direction, y > 0 (sky hemisphere)
// sun_dir : normalized sun direction, y > 0 (sun above horizon)
// T       : turbidity (1.7 = clear, 2.0 = default, 10 = hazy)
vec3 PreethamSky(vec3 view_dir, vec3 sun_dir, float T) {
    float cos_theta_s = clamp(sun_dir.y, 0.001, 1.0);
    float theta_s = acos(cos_theta_s);  // sun zenith angle

    float cos_gamma = clamp(dot(view_dir, sun_dir), -1.0, 1.0);
    float gamma     = acos(cos_gamma);  // angle between view and sun
    float cos_theta = max(view_dir.y, 0.001);

    float T2  = T * T;
    float ts2 = theta_s * theta_s;
    float ts3 = ts2 * theta_s;

    // Zenith luminance (kcd/m²) — eq. (7).
    float chi = (4.0 / 9.0 - T / 120.0) * (PI - 2.0 * theta_s);
    float Y_z = max((4.0453 * T - 4.9710) * tan(chi) - 0.2155 * T + 2.4192, 0.0);

    // Zenith chromaticity x_z — Table 2.
    float x_z = dot(vec3(T2, T, 1.0),
          vec3( 0.00166, -0.02903,  0.11693) * ts3
        + vec3(-0.00375,  0.06377, -0.21196) * ts2
        + vec3( 0.00209, -0.03202,  0.06052) * theta_s
        + vec3( 0.0,      0.00394,  0.25886));

    // Zenith chromaticity y_z — Table 3.
    float y_z = dot(vec3(T2, T, 1.0),
          vec3( 0.00275, -0.04214,  0.15346) * ts3
        + vec3(-0.00610,  0.08970, -0.26756) * ts2
        + vec3( 0.00317, -0.04153,  0.06670) * theta_s
        + vec3( 0.0,      0.00516,  0.26688));

    // Perez A–E coefficients for Y, x, y — Table 1.
    float AY =  0.17872 * T - 1.46303;
    float BY = -0.35540 * T + 0.42749;
    float CY = -0.02266 * T + 5.32505;
    float DY =  0.12064 * T - 2.57705;
    float EY = -0.06696 * T + 0.37027;

    float Ax = -0.01925 * T - 0.25922;
    float Bx = -0.06651 * T + 0.00081;
    float Cx = -0.00041 * T + 0.21247;
    float Dx = -0.06409 * T - 0.89887;
    float Ex = -0.00325 * T + 0.04517;

    float Ay = -0.01669 * T - 0.26078;
    float By = -0.09495 * T + 0.00921;
    float Cy = -0.00792 * T + 0.21023;
    float Dy = -0.04405 * T - 1.65369;
    float Ey = -0.01092 * T + 0.05291;

    // Normalise at the zenith (theta = 0, gamma = theta_s).
    float normY = PerezF(1.0, theta_s, cos_theta_s, AY, BY, CY, DY, EY);
    float normx = PerezF(1.0, theta_s, cos_theta_s, Ax, Bx, Cx, Dx, Ex);
    float normy = PerezF(1.0, theta_s, cos_theta_s, Ay, By, Cy, Dy, Ey);

    float Y = Y_z * PerezF(cos_theta, gamma, cos_gamma, AY, BY, CY, DY, EY) / normY;
    float x = x_z * PerezF(cos_theta, gamma, cos_gamma, Ax, Bx, Cx, Dx, Ex) / normx;
    float y = y_z * PerezF(cos_theta, gamma, cos_gamma, Ay, By, Cy, Dy, Ey) / normy;

    // Scale from kcd/m² to a display-friendly HDR range (~0–2 for clear sky).
    const float kLuminanceScale = 0.05;
    return YxyToRGB(Y * kLuminanceScale, x, y);
}

// ---- Simple 2D hash for star noise -----------------------------------------
float StarHash(vec2 p) {
    p = fract(p * vec2(127.1, 311.7));
    p += dot(p, p + 19.19);
    return fract(p.x * p.y);
}

// ===========================================================================
void main() {
    vec3 view_dir = normalize(v_view_ray);

    // Smoothly blend day ↔ night over a 10° band around the horizon.
    float day_factor   = clamp(sun_direction.y * 10.0 + 0.5, 0.0, 1.0);
    float night_factor = 1.0 - day_factor;

    // --- Daytime sky --------------------------------------------------------
    // Clamp view direction to sky hemisphere; reflects ground-facing rays upward.
    vec3 sky_view = normalize(vec3(view_dir.x, max(view_dir.y, 0.001), view_dir.z));
    // Clamp sun direction to above horizon for the Preetham model.
    vec3 sun_sky   = normalize(vec3(sun_direction.x,
                                    max(sun_direction.y, 0.001),
                                    sun_direction.z));

    vec3 sky_rgb = vec3(0.0);
    if (day_factor > 0.001) {
        sky_rgb = PreethamSky(sky_view, sun_sky, turbidity) * day_factor;
    }

    // --- Night sky ----------------------------------------------------------
    // Dark-blue gradient fading toward the horizon.
    float horizon_t = clamp(1.0 - view_dir.y * 4.0, 0.0, 1.0);
    vec3 night_zenith  = vec3(0.005, 0.006, 0.025);
    vec3 night_horizon = vec3(0.015, 0.012, 0.020);
    sky_rgb += mix(night_zenith, night_horizon, horizon_t) * night_factor;

    // --- Stars (night only, sky hemisphere) ---------------------------------
    // Each star is placed at a random sub-cell position and rendered as a tight
    // Gaussian point plus axis-aligned diffraction spikes, so it looks like a
    // point of light rather than a uniform cell-wide quad.
    if (night_factor > 0.05 && view_dir.y > 0.01) {
        // Project view direction onto a spherical (lon, lat) grid.
        float lat = asin(clamp(view_dir.y, -1.0, 1.0));
        float lon = atan(view_dir.z, view_dir.x);
        vec2  star_coord = vec2(lon, lat) * 80.0;
        vec2  star_grid  = floor(star_coord);
        vec2  star_uv    = fract(star_coord);   // [0, 1) within cell

        float h = StarHash(star_grid);
        // Only ~3 % of cells contain a visible star.
        const float kStarThresh = 0.97;
        if (h > kStarThresh) {
            float brightness = pow((h - kStarThresh) / (1.0 - kStarThresh), 2.0) * 1.5;

            // Random centre within the cell so the star is not at the grid corner.
            vec2 centre = vec2(StarHash(star_grid + vec2(3.7, 1.3)),
                               StarHash(star_grid + vec2(2.1, 5.9)));
            // Offset from current pixel to star centre, in cell-width units [-1, 1].
            vec2 d = star_uv - centre;

            // Tight Gaussian core — σ ≈ 0.07 cell → ~1 px at typical resolution.
            float core = exp(-dot(d, d) * 100.0);

            // Axis-aligned diffraction spikes (horizontal + vertical).
            //   Length falloff : exp(-d_along² * 3)   → half-power ≈ 0.5 cell
            //   Width  falloff : exp(-d_perp² * 120)  → half-power ≈ 0.08 cell (~1 px)
            float spike_h = exp(-d.x * d.x * 3.0) * exp(-d.y * d.y * 120.0);
            float spike_v = exp(-d.y * d.y * 3.0) * exp(-d.x * d.x * 120.0);
            float spikes  = (spike_h + spike_v) * 0.4;

            // Per-star colour tint spanning blue-white to yellow-white.
            float tint       = StarHash(star_grid + vec2(7.3, 4.1));
            vec3  star_color = mix(vec3(0.75, 0.9, 1.0), vec3(1.0, 0.95, 0.8), tint);

            sky_rgb += star_color * brightness * (core + spikes) * night_factor;
        }
    }

    // --- Sun disc -----------------------------------------------------------
    // Visible while sun is near or above the horizon.
    float sun_vis = clamp(sun_direction.y * 8.0 + 0.4, 0.0, 1.0);
    if (sun_vis > 0.0) {
        float cos_sun = dot(view_dir, sun_direction);
        // Inner disc: ~0.5° half-angle.
        float sun_disc = smoothstep(0.99970, 0.99990, cos_sun);
        // Outer halo: Mie forward-scattering glow.
        float sun_halo = smoothstep(0.99000, 0.99970, cos_sun) * 0.25;
        // HDR values; composite pass clamps to white naturally.
        sky_rgb += vec3(60.0, 48.0, 24.0) * sun_disc * sun_vis;
        sky_rgb += vec3(2.0,  1.6,  0.9)  * sun_halo * sun_vis;
    }

    // --- Moon disc ----------------------------------------------------------
    // Visible only when sun is below the horizon; direction is antipodal.
    float moon_vis = clamp(-sun_direction.y * 8.0 + 0.4, 0.0, 1.0);
    if (moon_vis > 0.0 && view_dir.y > 0.0) {
        vec3  moon_dir  = -sun_direction;
        float cos_moon  = dot(view_dir, moon_dir);
        float moon_disc = smoothstep(0.99960, 0.99985, cos_moon);
        // Softer silver colour, lower HDR value than the sun.
        sky_rgb += vec3(0.80, 0.85, 0.95) * moon_disc * moon_vis * 4.0;
    }

    out_color = vec4(sky_rgb, 1.0);
}
