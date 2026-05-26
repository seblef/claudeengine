#include "editor/LightWireframeRenderer.h"

#include <cmath>

#include "abstract/BufferUsage.h"
#include "abstract/CullFace.h"
#include "abstract/PrimitiveType.h"
#include "core/Color.h"
#include "core/Mat4f.h"
#include "core/Vec3f.h"
#include "core/VertexType.h"
#include "game/GameLight.h"
#include "game/GameObjectType.h"
#include "renderer/CircleSpotLight.h"
#include "renderer/Light.h"
#include "renderer/OmniLight.h"
#include "renderer/RectangleSpotLight.h"

namespace editor {

namespace {

constexpr int   kCircleSegments = 32;
constexpr float kPi             = 3.14159265f;

const core::Color kColorUnselected(1.f, 0.8f, 0.f, 1.f);  // yellow
const core::Color kColorSelected(0.f, 0.53f, 1.f, 1.f);   // blue

}  // namespace

LightWireframeRenderer::LightWireframeRenderer(abstract::VideoDevice* video)
    : video_(video), shader_(video->CreateShader("light_wireframe")) {}

LightWireframeRenderer::~LightWireframeRenderer() {
  shader_->Release();
}

void LightWireframeRenderer::PushSegment(const core::Vec3f& p0,
                                          const core::Vec3f& p1,
                                          const core::Color& color) {
  const core::Vec2f uv(0.f, 0.f);
  vertices_.emplace_back(p0, color, uv);
  vertices_.emplace_back(p1, color, uv);
}

void LightWireframeRenderer::PushCircle(const core::Vec3f& center,
                                         const core::Vec3f& ax,
                                         const core::Vec3f& ay,
                                         float radius,
                                         const core::Color& color) {
  for (int i = 0; i < kCircleSegments; ++i) {
    const float a0 = static_cast<float>(i)     / kCircleSegments * 2.f * kPi;
    const float a1 = static_cast<float>(i + 1) / kCircleSegments * 2.f * kPi;
    const core::Vec3f p0 = center + ax * (std::cos(a0) * radius)
                                  + ay * (std::sin(a0) * radius);
    const core::Vec3f p1 = center + ax * (std::cos(a1) * radius)
                                  + ay * (std::sin(a1) * radius);
    PushSegment(p0, p1, color);
  }
}

void LightWireframeRenderer::BuildVertices(
    const std::vector<game::GameObject*>& objects,
    const game::GameObject* selected) {
  vertices_.clear();

  for (const game::GameObject* obj : objects) {
    if (obj->GetType() != game::GameObjectType::kLight) continue;

    const auto* gl    = static_cast<const game::GameLight*>(obj);
    const renderer::Light* light = gl->GetLight();
    if (!light || light->GetType() == renderer::LightType::kGlobal) continue;

    const core::Mat4f& wt = obj->GetWorldTransform();
    const core::Vec3f  pos(wt(0, 3), wt(1, 3), wt(2, 3));
    const core::Color& color = (obj == selected) ? kColorSelected : kColorUnselected;

    switch (light->GetType()) {
      case renderer::LightType::kOmni: {
        const float r = static_cast<const renderer::OmniLight&>(*light).GetRadius();
        PushCircle(pos, core::Vec3f::kAxisX, core::Vec3f::kAxisY, r, color);
        PushCircle(pos, core::Vec3f::kAxisX, core::Vec3f::kAxisZ, r, color);
        PushCircle(pos, core::Vec3f::kAxisY, core::Vec3f::kAxisZ, r, color);
        break;
      }

      case renderer::LightType::kCircleSpot: {
        const auto& spot  = static_cast<const renderer::CircleSpotLight&>(*light);
        const float range = spot.GetRange();
        const core::Vec3f& dir = spot.GetDirection();
        const core::Vec3f up =
            (std::abs(dir.y) < 0.99f) ? core::Vec3f::kAxisY : core::Vec3f::kAxisX;
        const core::Vec3f right = dir.Cross(up).Normalized();
        const core::Vec3f fwd   = dir.Cross(right);
        const float base_r      = std::tan(spot.GetOuterAngle()) * range;
        const core::Vec3f base_center = pos + dir * range;
        PushSegment(pos, base_center + right * base_r, color);
        PushSegment(pos, base_center - right * base_r, color);
        PushSegment(pos, base_center + fwd   * base_r, color);
        PushSegment(pos, base_center - fwd   * base_r, color);
        PushCircle(base_center, right, fwd, base_r, color);
        break;
      }

      case renderer::LightType::kRectSpot: {
        const auto& spot  = static_cast<const renderer::RectangleSpotLight&>(*light);
        const float range = spot.GetRange();
        const core::Vec3f& dir = spot.GetDirection();
        const core::Vec3f up =
            (std::abs(dir.y) < 0.99f) ? core::Vec3f::kAxisY : core::Vec3f::kAxisX;
        const core::Vec3f right = dir.Cross(up).Normalized();
        const core::Vec3f fwd   = dir.Cross(right);
        const float half_w = std::tan(spot.GetHAngle()) * range;
        const float half_h = std::tan(spot.GetVAngle()) * range;
        const core::Vec3f base_center = pos + dir * range;
        const core::Vec3f c0 = base_center + right * half_w + fwd * half_h;
        const core::Vec3f c1 = base_center - right * half_w + fwd * half_h;
        const core::Vec3f c2 = base_center - right * half_w - fwd * half_h;
        const core::Vec3f c3 = base_center + right * half_w - fwd * half_h;
        PushSegment(pos, c0, color);
        PushSegment(pos, c1, color);
        PushSegment(pos, c2, color);
        PushSegment(pos, c3, color);
        PushSegment(c0, c1, color);
        PushSegment(c1, c2, color);
        PushSegment(c2, c3, color);
        PushSegment(c3, c0, color);
        break;
      }

      default:
        break;
    }
  }
}

void LightWireframeRenderer::EnsureVertexBuffer() {
  const int needed = static_cast<int>(vertices_.size());
  if (needed <= vertex_buf_capacity_) return;

  // Grow to next power-of-two above needed.
  int cap = std::max(256, vertex_buf_capacity_);
  while (cap < needed) cap *= 2;

  vertex_buf_ = video_->CreateVertexBuffer(
      core::VertexType::kBase, cap, abstract::BufferUsage::kDynamic);
  vertex_buf_capacity_ = cap;
}

void LightWireframeRenderer::Render(
    const std::vector<game::GameObject*>& objects,
    const game::GameObject* selected,
    abstract::RenderTargetGroup* fbo) {
  BuildVertices(objects, selected);
  if (vertices_.empty()) return;

  EnsureVertexBuffer();
  vertex_buf_->Fill(vertices_.data(), static_cast<int>(vertices_.size()));

  fbo->BindForWriting();
  video_->SetDepthTestEnabled(true);
  video_->SetDepthWriteEnabled(false);
  video_->SetFaceCulling(abstract::CullFace::kNone);
  video_->SetPrimitiveType(abstract::PrimitiveType::kLineList);

  shader_->Activate();
  vertex_buf_->Bind();
  video_->Render(static_cast<int>(vertices_.size()));

  fbo->UnbindForWriting();

  video_->SetDepthWriteEnabled(true);
  video_->SetDepthTestEnabled(false);
  video_->SetFaceCulling(abstract::CullFace::kBack);
  video_->SetPrimitiveType(abstract::PrimitiveType::kTriangleList);
}

}  // namespace editor
