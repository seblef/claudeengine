#pragma once

#include <cstddef>

#include "core/Mat4f.h"

namespace renderer {

// GPU constant buffer layout for per-light data (UBO slot 4).
// Matches layout(std140, binding=4) exactly — do not reorder fields.
//
// Offset map:
//   [  0] px,py,pz    vec3  (12 B — world-space position)
//         type        float ( 4 B — 0=global,1=omni,2=circle_spot,3=rect_spot)
//   [ 16] cr,cg,cb    vec3  (12 B — RGB color)
//         intensity   float ( 4 B)
//   [ 32] dx,dy,dz    vec3  (12 B — world-space direction, normalized)
//         range       float ( 4 B — max influence distance)
//   [ 48] inner_angle float ( 4 B)  outer_angle float ( 4 B)
//         h_angle     float ( 4 B)  v_angle     float ( 4 B)
//   [ 64] ar,ag,ab    vec3  (12 B — ambient RGB, GlobalLight only)
//         cast_shadow float ( 4 B — 1.0 if pool assigned a shadow map, 0.0 otherwise)
//   [ 80] light_vp    mat4  (64 B — light-space VP; identity when cast_shadow==0)
//   [144] shadow_bias float ( 4 B)
//   [148] pad1_–pad3_ float (12 B — alignment padding to 160 B)
//   [160] end
struct LightInfos {
  float px, py, pz;  float type;                         //   0
  float cr, cg, cb;  float intensity;                    //  16
  float dx, dy, dz;  float range;                        //  32
  float inner_angle, outer_angle, h_angle, v_angle;      //  48
  float ar, ag, ab;  float cast_shadow;                  //  64
  core::Mat4f light_vp;                                  //  80
  float shadow_bias;                                     // 144
  float pad1_, pad2_, pad3_;                             // 148
};                                                       // 160

static_assert(sizeof(LightInfos) == 160);
static_assert(offsetof(LightInfos, cast_shadow) ==  76);
static_assert(offsetof(LightInfos, light_vp)    ==  80);
static_assert(offsetof(LightInfos, shadow_bias) == 144);

}  // namespace renderer
