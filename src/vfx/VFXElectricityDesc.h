#pragma once

#include "core/Color.h"

namespace vfx {

// Tunable parameters for a vfx::VFXElectricity instance: an arc that connects
// two world-space points with a jittered line + spark particles.
//
// All fields carry sensible defaults so a caller can use VFXElectricityDesc{}
// as-is.
struct VFXElectricityDesc {
  // Number of jittered line segments composing the arc; higher looks more
  // jagged/detailed but costs more line-draw vertices.
  int arc_segments = 8;

  // Spark particles/sec emitted along the arc.
  float spark_rate = 40.f;

  // Screech/crackle sound gain multiplier (0-1), applied the same way as
  // physics::ScrapeDesc::base_gain.
  float crackle_volume = 0.6f;

  // Arc + spark tint.
  core::Color color = core::Color(0.6f, 0.8f, 1.f, 1.f);

  // Distance (m) between the arc's two endpoints, measured along Play()'s
  // direction from world_pos.
  float arc_length = 4.f;

  // Total lifetime (s) of the effect before VFXElectricity self-expires.
  float duration = 1.f;
};

}  // namespace vfx
