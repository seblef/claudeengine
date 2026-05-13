#include "renderer/GlobalLight.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

#include "core/Camera.h"
#include "renderer/CSMInfos.h"

namespace renderer {

GlobalLight::GlobalLight(const core::Color& color, float intensity,
                         const core::Vec3f& ambient_color,
                         const core::Vec3f& direction)
    : Light(LightType::kGlobal, color, intensity,
            core::BBox3::kInfinite,
            core::Mat4f::kIdentity,
            /*always_visible=*/true),
      ambient_color_(ambient_color),
      direction_(direction) {}

core::Mat4f GlobalLight::GetVolumeMatrix() const {
  return core::Mat4f::kIdentity;
}

void GlobalLight::ComputeCascadeMatrices(const core::Camera& camera,
                                         CSMInfos& out) const {
  const float z_near  = camera.GetMinDepth();
  const float z_far   = camera.GetMaxDepth();
  const float aspect  = camera.GetScreenCenter().x / camera.GetScreenCenter().y;
  const float thfov_y = std::tan(camera.GetFOV() * 0.5f);
  const float thfov_x = thfov_y * aspect;
  const core::Mat4f& inv_view = camera.GetInverseViewMatrix();

  // Practical split scheme (lambda=0.5): blend logarithmic and uniform splits.
  // split_i = lambda * z_near*(z_far/z_near)^(i/N) + (1-lambda)*(z_near+(z_far-z_near)*i/N)
  static constexpr float kLambda = 0.5f;
  const float ratio = z_far / z_near;
  float splits[kCSMCascadeCount + 1];
  splits[0] = z_near;
  for (int i = 1; i <= kCSMCascadeCount; ++i) {
    const float t     = static_cast<float>(i) / kCSMCascadeCount;
    const float s_log = z_near * std::pow(ratio, t);
    const float s_uni = z_near + (z_far - z_near) * t;
    splits[i] = kLambda * s_log + (1.f - kLambda) * s_uni;
  }

  out.split_x = splits[1];
  out.split_y = splits[2];
  out.split_z = splits[3];
  out.split_w = splits[4];

  // Up vector for the light camera — must not be parallel to direction_.
  core::Vec3f up = {0.f, 1.f, 0.f};
  if (std::fabs(direction_.y) > 0.95f) up = {1.f, 0.f, 0.f};

  for (int i = 0; i < kCSMCascadeCount; ++i) {
    const float cn = splits[i];
    const float cf = splits[i + 1];

    // 8 corners of the cascade sub-frustum in camera view space (RH: z < 0 in front).
    const core::Vec3f cv[8] = {
      { thfov_x * cn,  thfov_y * cn, -cn},
      {-thfov_x * cn,  thfov_y * cn, -cn},
      { thfov_x * cn, -thfov_y * cn, -cn},
      {-thfov_x * cn, -thfov_y * cn, -cn},
      { thfov_x * cf,  thfov_y * cf, -cf},
      {-thfov_x * cf,  thfov_y * cf, -cf},
      { thfov_x * cf, -thfov_y * cf, -cf},
      {-thfov_x * cf, -thfov_y * cf, -cf},
    };

    // Transform to world space and compute centroid.
    core::Vec3f cw[8];
    core::Vec3f centroid{0.f, 0.f, 0.f};
    for (int k = 0; k < 8; ++k) {
      cw[k]    = cv[k] * inv_view;
      centroid += cw[k];
    }
    centroid *= (1.f / 8.f);

    // Bounding sphere radius used to place the light eye and extend z_far.
    const float half_depth = std::accumulate(
        cw, cw + 8, 0.f,
        [&centroid](float acc, const core::Vec3f& c) {
          return std::max(acc, (c - centroid).Length());
        });

    // Light eye is placed behind the scene in the opposite direction of travel.
    const core::Vec3f  light_eye  = centroid - direction_ * half_depth;
    const core::Mat4f  light_view = core::Mat4f::LookAtRH(light_eye, centroid, up);

    // Find tight AABB of the frustum corners in light-view space.
    const float kInf = std::numeric_limits<float>::max();
    float min_x = kInf, max_x = -kInf;
    float min_y = kInf, max_y = -kInf;
    float min_z = kInf, max_z = -kInf;
    for (const auto& c : cw) {
      const core::Vec3f lc = c * light_view;
      min_x = std::min(min_x, lc.x);  max_x = std::max(max_x, lc.x);
      min_y = std::min(min_y, lc.y);  max_y = std::max(max_y, lc.y);
      min_z = std::min(min_z, lc.z);  max_z = std::max(max_z, lc.z);
    }

    // z_near/z_far from light-view z (negative for objects in front of camera).
    // Extend z_far by half_depth to capture casters behind the cascade frustum.
    static constexpr float kMinNear = 0.1f;
    const float ls_near = std::max(kMinNear, -max_z);
    const float ls_far  = -min_z + half_depth;

    const core::Mat4f ortho = core::Mat4f::OrthoOffCenterRH(
        min_x, max_x, min_y, max_y, ls_near, ls_far);
    out.cascade_vp[i] = ortho * light_view;
  }
}

}  // namespace renderer
