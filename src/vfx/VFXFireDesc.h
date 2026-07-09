#pragma once

namespace vfx {

// Tunable parameters for a vfx::VFXFire instance.
//
// Deliberately minimal: VFXFire's only authoring-time parameter beyond the
// particle template itself is the heat-distortion stub flag (see
// VFXFire::IsHeatDistortionEnabled) — everything else about the flame/smoke
// look is authored in the "fire" particle template, not here.
struct VFXFireDesc {
  // Stub toggle: authoring intent only, not yet wired to a post-process pass.
  // See VFXFire's constructor doc comment.
  bool heat_distortion = false;
};

}  // namespace vfx
