#include "editor/PlayerStartGizmoRenderer.h"

#include "abstract/BufferUsage.h"
#include "abstract/CullFace.h"
#include "abstract/PrimitiveType.h"
#include "core/Color.h"
#include "core/Mat4f.h"
#include "core/Vec3f.h"
#include "core/VertexType.h"
#include "game/GameObjectType.h"

namespace editor {

namespace {

// Pole height above the world-space position (in world units).
constexpr float kPoleHeight  = 2.0f;
// Flag dimensions relative to the pole top.
constexpr float kFlagWidth   = 0.8f;
constexpr float kFlagHeight  = 0.5f;

const core::Color kPoleColor(1.f, 1.f, 1.f, 1.f);               // white
const core::Color kFlagColor(0.f, 0.8f, 0.267f, 1.f);            // #00CC44
const core::Color kFlagColorSelected(0.2f, 1.f, 0.467f, 1.f);    // brighter green

}  // namespace

PlayerStartGizmoRenderer::PlayerStartGizmoRenderer(abstract::VideoDevice* video)
    : video_(video), shader_(video->CreateShader("light_wireframe")) {}

PlayerStartGizmoRenderer::~PlayerStartGizmoRenderer() {
  shader_->Release();
}

void PlayerStartGizmoRenderer::PushSegment(const core::Vec3f& p0,
                                            const core::Vec3f& p1,
                                            const core::Color& color) {
  const core::Vec2f uv(0.f, 0.f);
  vertices_.emplace_back(p0, color, uv);
  vertices_.emplace_back(p1, color, uv);
}

void PlayerStartGizmoRenderer::BuildVertices(
    const std::vector<game::GameObject*>& objects,
    const game::GameObject* selected) {
  vertices_.clear();

  for (const game::GameObject* obj : objects) {
    if (obj->GetType() != game::GameObjectType::kPlayerStart) continue;

    const core::Mat4f& wt  = obj->GetWorldTransform();
    const core::Vec3f  base(wt(0, 3), wt(1, 3), wt(2, 3));
    const core::Vec3f  top = base + core::Vec3f(0.f, kPoleHeight, 0.f);

    // Pole: base → top, white.
    PushSegment(base, top, kPoleColor);

    // Flag: right-pointing triangle at the top of the pole.
    //   A = top of pole
    //   B = top + (kFlagWidth, 0, 0)
    //   C = top + (kFlagWidth, -kFlagHeight, 0)  ← could also use (0, -kFlagHeight, 0) at pole
    // Layout matches the ▶ shape: left edge is the pole, tip is to the right.
    const core::Vec3f flag_a = top;
    const core::Vec3f flag_b = top + core::Vec3f(kFlagWidth, 0.f, 0.f);
    const core::Vec3f flag_c = top + core::Vec3f(0.f, -kFlagHeight, 0.f);

    const core::Color& flag_color =
        (obj == selected) ? kFlagColorSelected : kFlagColor;

    PushSegment(flag_a, flag_b, flag_color);
    PushSegment(flag_b, flag_c, flag_color);
    PushSegment(flag_c, flag_a, flag_color);
  }
}

void PlayerStartGizmoRenderer::EnsureVertexBuffer() {
  const int needed = static_cast<int>(vertices_.size());
  if (needed <= vertex_buf_capacity_) return;

  int cap = std::max(64, vertex_buf_capacity_);
  while (cap < needed) cap *= 2;

  vertex_buf_ = video_->CreateVertexBuffer(
      core::VertexType::kBase, cap, abstract::BufferUsage::kDynamic);
  vertex_buf_capacity_ = cap;
}

void PlayerStartGizmoRenderer::Render(
    const std::vector<game::GameObject*>& objects,
    const game::GameObject* selected,
    abstract::RenderTargetGroup* fbo) {
  BuildVertices(objects, selected);
  if (vertices_.empty()) return;

  EnsureVertexBuffer();
  vertex_buf_->Fill(vertices_.data(), static_cast<int>(vertices_.size()));

  fbo->BindForWriting();
  // No depth testing — always visible over terrain and meshes.
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
