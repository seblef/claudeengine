#include "editor/ParticleGizmoRenderer.h"

#include <cmath>

#include "abstract/BufferUsage.h"
#include "abstract/CullFace.h"
#include "abstract/PrimitiveType.h"
#include "core/Color.h"
#include "core/Mat4f.h"
#include "core/Vec3f.h"
#include "core/VertexType.h"
#include "game/GameObjectType.h"
#include "game/GameParticleSystem.h"
#include "particles/EmitterShape.h"
#include "particles/ParticleSubSystemDesc.h"
#include "particles/ParticleSystemTemplate.h"

namespace editor {

namespace {

constexpr float kTwoPi = 6.283185307f;

const core::Color kGizmoColor(0.2f, 0.8f, 1.0f, 1.0f);          // cyan
const core::Color kGizmoColorSelected(0.5f, 1.0f, 1.0f, 1.0f);  // bright cyan

}  // namespace

ParticleGizmoRenderer::ParticleGizmoRenderer(abstract::VideoDevice* video)
    : video_(video), shader_(video->CreateShader("light_wireframe")) {}

ParticleGizmoRenderer::~ParticleGizmoRenderer() {
  shader_->Release();
}

void ParticleGizmoRenderer::PushSegment(const core::Vec3f& p0,
                                         const core::Vec3f& p1,
                                         const core::Color& color) {
  const core::Vec2f uv(0.f, 0.f);
  vertices_.emplace_back(p0, color, uv);
  vertices_.emplace_back(p1, color, uv);
}

void ParticleGizmoRenderer::PushCircle(const core::Vec3f& center,
                                        float radius,
                                        const core::Vec3f& ax,
                                        const core::Vec3f& ay,
                                        const core::Color& color) {
  for (int i = 0; i < kCircleSegments; ++i) {
    const float t0 = kTwoPi * static_cast<float>(i)     / kCircleSegments;
    const float t1 = kTwoPi * static_cast<float>(i + 1) / kCircleSegments;
    const core::Vec3f p0 = center + ax * (radius * std::cos(t0))
                                  + ay * (radius * std::sin(t0));
    const core::Vec3f p1 = center + ax * (radius * std::cos(t1))
                                  + ay * (radius * std::sin(t1));
    PushSegment(p0, p1, color);
  }
}

void ParticleGizmoRenderer::BuildVertices(
    const std::vector<game::GameObject*>& objects,
    const game::GameObject* selected) {
  vertices_.clear();

  for (const game::GameObject* obj : objects) {
    if (obj->GetType() != game::GameObjectType::kParticleSystem) continue;

    const auto* ps = static_cast<const game::GameParticleSystem*>(obj);
    const auto* tmpl = ps->GetTemplate();

    float radius = kDefaultRadius;
    if (tmpl && !tmpl->GetSubSystems().empty()) {
      const auto& sub = tmpl->GetSubSystems().front();
      if (sub.emitter_shape != particles::EmitterShape::kPoint)
        radius = sub.emitter_radius;
    }

    const core::Mat4f& wt  = obj->GetWorldTransform();
    const core::Vec3f  pos(wt(0, 3), wt(1, 3), wt(2, 3));

    const core::Color& color =
        (obj == selected) ? kGizmoColorSelected : kGizmoColor;

    // Three orthogonal great circles: XY, XZ, YZ planes.
    PushCircle(pos, radius,
               core::Vec3f(1.f, 0.f, 0.f), core::Vec3f(0.f, 1.f, 0.f), color);
    PushCircle(pos, radius,
               core::Vec3f(1.f, 0.f, 0.f), core::Vec3f(0.f, 0.f, 1.f), color);
    PushCircle(pos, radius,
               core::Vec3f(0.f, 1.f, 0.f), core::Vec3f(0.f, 0.f, 1.f), color);
  }
}

void ParticleGizmoRenderer::EnsureVertexBuffer() {
  const int needed = static_cast<int>(vertices_.size());
  if (needed <= vertex_buf_capacity_) return;

  int cap = std::max(64, vertex_buf_capacity_);
  while (cap < needed) cap *= 2;

  vertex_buf_ = video_->CreateVertexBuffer(
      core::VertexType::kBase, cap, abstract::BufferUsage::kDynamic);
  vertex_buf_capacity_ = cap;
}

void ParticleGizmoRenderer::Render(
    const std::vector<game::GameObject*>& objects,
    const game::GameObject* selected,
    abstract::RenderTargetGroup* fbo) {
  BuildVertices(objects, selected);
  if (vertices_.empty()) return;

  EnsureVertexBuffer();
  vertex_buf_->Fill(vertices_.data(), static_cast<int>(vertices_.size()));

  fbo->BindForWriting();
  video_->SetDepthTestEnabled(false);
  video_->SetDepthWriteEnabled(false);
  video_->SetFaceCulling(abstract::CullFace::kNone);
  video_->SetPrimitiveType(abstract::PrimitiveType::kLineList);

  shader_->Activate();
  vertex_buf_->Bind();
  video_->Render(static_cast<int>(vertices_.size()));

  fbo->UnbindForWriting();

  video_->SetDepthWriteEnabled(true);
  video_->SetFaceCulling(abstract::CullFace::kBack);
  video_->SetPrimitiveType(abstract::PrimitiveType::kTriangleList);
}

}  // namespace editor
