#pragma once

#include <algorithm>
#include <cmath>

#include "core/Color.h"
#include "core/Vec3f.h"
#include "environment/WorldTime.h"
#include "renderer/GlobalLight.h"

namespace game {

// sin(15°) — elevation below which the sun colour warms to orange/pink.
inline constexpr float kDawnDuskElevationY = 0.259f;

// Updates direction, colour, intensity, and ambient of a GlobalLight to
// match the current WorldTime.
//
// Daytime  — direction follows the sun; colour blends from warm orange at
//            low elevation (<15°) to near-white at noon; intensity scales
//            with sin(elevation), clamped to [0.05, 1.0].
// Night    — direction follows the moon; cool blue-grey at intensity 0.05.
// Ambient  — transitions from orange-pink at dawn/dusk through light grey
//            at noon to deep blue at night.
//
// The sun direction fed here must be identical to the one passed to
// SkyRenderer so the shadow direction matches the visible disc.
//
// Call once per frame after WorldTime::Advance(dt).
inline void UpdateGlobalLight(renderer::GlobalLight& light,
                               const environment::WorldTime& time) {
    if (time.IsDaytime()) {
        const core::Vec3f sun_dir = time.GetSunDirection();
        // sun_dir points toward the sun; light direction is the ray from source
        // to surface (downward for a sun above the horizon), so negate.
        light.SetDirection(-sun_dir);

        // sun_dir.y == sin(elevation); blend dawn/dusk→noon over [0, sin(15°)].
        const float t = std::min(sun_dir.y / kDawnDuskElevationY, 1.f);

        const core::Color dawn_color(1.0f, 0.6f, 0.3f);
        const core::Color noon_color(1.0f, 0.97f, 0.9f);
        light.SetColor(core::Color(
            dawn_color.r + t * (noon_color.r - dawn_color.r),
            dawn_color.g + t * (noon_color.g - dawn_color.g),
            dawn_color.b + t * (noon_color.b - dawn_color.b)));
        light.SetIntensity(std::max(0.05f, std::min(1.0f, sun_dir.y)));

        const core::Vec3f dawn_ambient(0.4f, 0.25f, 0.2f);
        const core::Vec3f noon_ambient(0.35f, 0.35f, 0.38f);
        light.SetAmbientColor(core::Vec3f(
            dawn_ambient.x + t * (noon_ambient.x - dawn_ambient.x),
            dawn_ambient.y + t * (noon_ambient.y - dawn_ambient.y),
            dawn_ambient.z + t * (noon_ambient.z - dawn_ambient.z)));
    } else {
        light.SetDirection(-time.GetMoonDirection());
        light.SetColor(core::Color(0.5f, 0.55f, 0.7f));
        light.SetIntensity(0.05f);
        light.SetAmbientColor(core::Vec3f(0.02f, 0.02f, 0.05f));
    }
}

}  // namespace game
