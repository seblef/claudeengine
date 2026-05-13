#include "renderer/ShadowCubeMap.h"

#include <numbers>
#include <span>

#include "abstract/TextureFormat.h"

namespace renderer {

ShadowCubeMap::ShadowCubeMap(abstract::VideoDevice* video, int size)
    : cube_rt_(video->CreateRenderTargetCube(size,
                                             abstract::TextureFormat::kDepth32F)),
      fbo_(video->CreateRenderTargetGroup(
          std::span<abstract::RenderTarget*>{}, nullptr)),
      cube_vp_{},
      size_(size) {}

void ShadowCubeMap::ComputeFaceMatrices(const core::Vec3f& position, float range) {
  static constexpr float kNear  = 0.1f;
  static constexpr float kFov90 = std::numbers::pi_v<float> * 0.5f;

  const core::Mat4f proj = core::Mat4f::PerspectiveRH(kFov90, 1.f, kNear, range);

  // Standard cube-map face orientations (OpenGL convention).
  const core::Vec3f p = position;
  const core::Mat4f views[6] = {
    core::Mat4f::LookAtRH(p, p + core::Vec3f( 1.f,  0.f,  0.f), { 0.f, -1.f,  0.f}),
    core::Mat4f::LookAtRH(p, p + core::Vec3f(-1.f,  0.f,  0.f), { 0.f, -1.f,  0.f}),
    core::Mat4f::LookAtRH(p, p + core::Vec3f( 0.f,  1.f,  0.f), { 0.f,  0.f,  1.f}),
    core::Mat4f::LookAtRH(p, p + core::Vec3f( 0.f, -1.f,  0.f), { 0.f,  0.f, -1.f}),
    core::Mat4f::LookAtRH(p, p + core::Vec3f( 0.f,  0.f,  1.f), { 0.f, -1.f,  0.f}),
    core::Mat4f::LookAtRH(p, p + core::Vec3f( 0.f,  0.f, -1.f), { 0.f, -1.f,  0.f}),
  };

  for (int i = 0; i < 6; ++i)
    cube_vp_[i] = proj * views[i];
}

void ShadowCubeMap::BindFaceForWriting(int face_idx) {
  fbo_->BindForWriting();
  cube_rt_->BindFaceAsOutput(face_idx);
}

void ShadowCubeMap::UnbindForWriting() {
  fbo_->UnbindForWriting();
}

}  // namespace renderer
