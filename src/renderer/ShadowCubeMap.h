#pragma once

#include <array>
#include <memory>

#include "abstract/RenderTargetCube.h"
#include "abstract/RenderTargetGroup.h"
#include "abstract/VideoDevice.h"
#include "core/Mat4f.h"
#include "core/Vec3f.h"

namespace renderer {

// Owns the GPU resources for one omni-light cube shadow map: a depth-only
// cube-map render target and a shared FBO whose depth attachment is swapped
// to a different cube face before each of the 6 depth passes.
//
// ComputeFaceMatrices() fills the 6 light-space VP matrices each frame.
// BindFaceForWriting() / UnbindForWriting() bracket each per-face depth pass.
// GetCubeRT() is later bound as a samplerCubeShadow in the lighting shader.
class ShadowCubeMap {
 public:
  ShadowCubeMap(abstract::VideoDevice* video, int size);

  ShadowCubeMap(const ShadowCubeMap&)            = delete;
  ShadowCubeMap& operator=(const ShadowCubeMap&) = delete;

  // Fills cube_vp_[6] with proj * view for each cube face.
  // proj = PerspectiveRH(π/2, 1.0, 0.1, range).
  void ComputeFaceMatrices(const core::Vec3f& position, float range);

  // Binds the shared FBO and attaches face_idx as the depth attachment.
  void BindFaceForWriting(int face_idx);

  void UnbindForWriting();

  [[nodiscard]] abstract::RenderTargetCube* GetCubeRT()           const { return cube_rt_.get(); }
  [[nodiscard]] const core::Mat4f&          GetLightVP(int face)  const { return cube_vp_[face]; }
  [[nodiscard]] int                         GetSize()             const { return size_; }

 private:
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<abstract::RenderTargetCube>  cube_rt_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<abstract::RenderTargetGroup> fbo_;
  // cppcheck-suppress unusedStructMember
  std::array<core::Mat4f, 6>                   cube_vp_;
  // cppcheck-suppress unusedStructMember
  int                                          size_;
};

}  // namespace renderer
