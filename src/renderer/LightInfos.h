#pragma once

#include <cstddef>

namespace renderer {

// GPU constant buffer layout for per-light data (UBO slot 4).
// Matches layout(std140, binding=4) exactly — do not reorder fields.
//
// Each group of four floats packs a vec3 + float to respect std140 alignment:
// vec3 (12 B) + float (4 B) = 16 B per row (no padding between vec3 and float
// in std140).  Core Vec3f is 16-byte aligned, so it cannot be used here —
// plain floats are used instead to keep the C++ sizeof at 80.
//
// std140 offsets:
//   [  0] px,py,pz   vec3  (12 B — world-space light position)
//         type       float ( 4 B — 0=global,1=omni,2=circle_spot,3=rect_spot)
//   [ 16] cr,cg,cb   vec3  (12 B — light RGB color)
//         intensity  float ( 4 B)
//   [ 32] dx,dy,dz   vec3  (12 B — world-space light direction, normalized)
//         range      float ( 4 B — max influence distance)
//   [ 48] inner_angle float ( 4 B — spot inner half-angle, radians)
//         outer_angle float ( 4 B — spot outer half-angle, radians)
//         h_angle     float ( 4 B — rect spot horizontal half-angle, radians)
//         v_angle     float ( 4 B — rect spot vertical half-angle, radians)
//   [ 64] ar,ag,ab   vec3  (12 B — ambient RGB color, GlobalLight only)
//         pad_       float ( 4 B)
//   [ 80] end
struct LightInfos {
  float px, py, pz;  float type;           // position + light type
  float cr, cg, cb;  float intensity;      // color + intensity
  float dx, dy, dz;  float range;          // direction + range
  float inner_angle, outer_angle, h_angle, v_angle;
  float ar, ag, ab;  float pad_;           // ambient + padding
};

static_assert(sizeof(LightInfos) == 80);
static_assert(offsetof(LightInfos, cr)          ==  16);
static_assert(offsetof(LightInfos, dx)          ==  32);
static_assert(offsetof(LightInfos, inner_angle) ==  48);
static_assert(offsetof(LightInfos, ar)          ==  64);

}  // namespace renderer
