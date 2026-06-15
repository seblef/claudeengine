#include "renderer/Light.h"

#include <cmath>

#include "core/Vec3f.h"
#include "renderer/CircleSpotLight.h"
#include "renderer/LightRenderer.h"
#include "renderer/OmniLight.h"
#include "renderer/RectangleSpotLight.h"
#include "renderer/WireframeRenderer.h"

namespace renderer {

namespace {

const core::Color kHighlightBlue(0.f, 0.53f, 1.f, 1.f);

}  // namespace

Light::Light(LightType type, const core::Color& color, float intensity,
             const core::BBox3& local_bbox,
             const core::Mat4f& world_matrix,
             bool always_visible)
    : Renderable(local_bbox, world_matrix, always_visible),
      type_(type),
      color_(color),
      intensity_(intensity) {}

void Light::Enqueue() {
  LightRenderer::Instance().AddLight(this);
  if (WireframeRenderer::Instance().AreGizmosEnabled() && type_ != LightType::kGlobal) {
    const core::Color color = WireframeRenderer::Instance().IsHighlighted(gizmo_key_)
                              ? kHighlightBlue : color_;
    EnqueueWireframe(color);
  }
}

void Light::EnqueueWireframe(const core::Color& color) {
  const core::Mat4f& wm  = GetWorldMatrix();
  const core::Vec3f  pos(wm(0, 3), wm(1, 3), wm(2, 3));

  switch (type_) {
    case LightType::kOmni: {
      const float r = static_cast<const OmniLight*>(this)->GetRadius();
      WireframeRenderer::Instance().PushSphere(r, color, core::Mat4f::Translation(pos));
      break;
    }

    case LightType::kCircleSpot: {
      const auto& spot = *static_cast<const CircleSpotLight*>(this);
      const core::Vec3f dir = spot.GetDirection();
      const core::Vec3f up =
          (std::abs(dir.y) < 0.99f) ? core::Vec3f::kAxisY : core::Vec3f::kAxisX;
      const core::Vec3f right = dir.Cross(up).Normalized();
      const core::Vec3f fwd   = dir.Cross(right);
      const core::Mat4f transform{
          right.x, fwd.x, dir.x, pos.x,
          right.y, fwd.y, dir.y, pos.y,
          right.z, fwd.z, dir.z, pos.z,
          0.f,     0.f,   0.f,   1.f};
      WireframeRenderer::Instance().PushCone(
          spot.GetOuterAngle(), spot.GetRange(), color, transform);
      break;
    }

    case LightType::kRectSpot: {
      const auto& spot  = *static_cast<const RectangleSpotLight*>(this);
      const float range = spot.GetRange();
      const core::Vec3f dir = spot.GetDirection();
      const core::Vec3f up =
          (std::abs(dir.y) < 0.99f) ? core::Vec3f::kAxisY : core::Vec3f::kAxisX;
      const core::Vec3f right = dir.Cross(up).Normalized();
      const core::Vec3f fwd   = dir.Cross(right);
      const float half_w = std::tan(spot.GetHAngle()) * range;
      const float half_h = std::tan(spot.GetVAngle()) * range;
      const core::Vec3f base = pos + dir * range;
      const core::Vec3f c0 = base + right * half_w + fwd * half_h;
      const core::Vec3f c1 = base - right * half_w + fwd * half_h;
      const core::Vec3f c2 = base - right * half_w - fwd * half_h;
      const core::Vec3f c3 = base + right * half_w - fwd * half_h;
      WireframeRenderer::Instance().PushSegment(pos, c0, color);
      WireframeRenderer::Instance().PushSegment(pos, c1, color);
      WireframeRenderer::Instance().PushSegment(pos, c2, color);
      WireframeRenderer::Instance().PushSegment(pos, c3, color);
      WireframeRenderer::Instance().PushSegment(c0, c1, color);
      WireframeRenderer::Instance().PushSegment(c1, c2, color);
      WireframeRenderer::Instance().PushSegment(c2, c3, color);
      WireframeRenderer::Instance().PushSegment(c3, c0, color);
      break;
    }

    default:
      break;
  }
}

// static
float Light::ScreenRadius(const core::Vec3f& sphere_center,
                           float              sphere_radius,
                           const core::Vec3f& eye_pos,
                           float              half_screen_height,
                           float              tan_half_fov) {
  const float dist = (sphere_center - eye_pos).Length();
  if (dist < 1e-4f) return half_screen_height * 2.f;  // camera inside sphere
  return sphere_radius * half_screen_height / (tan_half_fov * dist);
}

}  // namespace renderer
