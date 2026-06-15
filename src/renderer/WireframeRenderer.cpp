#include "renderer/WireframeRenderer.h"

#include <algorithm>
#include <cmath>

#include "abstract/BufferUsage.h"
#include "abstract/CullFace.h"
#include "abstract/PrimitiveType.h"
#include "core/Vec2f.h"
#include "core/VertexType.h"
#include "terrain/TerrainRenderer.h"

namespace renderer {

namespace {

constexpr int   kCircleSegments = 32;
constexpr float kTwoPi         = 6.28318530718f;

}  // namespace

// ---- Construction -----------------------------------------------------------

WireframeRenderer::WireframeRenderer(abstract::VideoDevice* video)
    : video_(video), shader_(video->CreateShader("wireframe")) {}

WireframeRenderer::~WireframeRenderer() {
  shader_->Release();
}

// ---- Frame lifecycle --------------------------------------------------------

void WireframeRenderer::BeginFrame() {
  depth_list_.clear();
  overlay_list_.clear();
}

void WireframeRenderer::Render(const core::Camera& camera,
                                abstract::RenderTargetGroup* depth_fbo,
                                abstract::RenderTargetGroup* overlay_fbo) {
  if (terrain_wireframe_enabled_ && terrain_renderer_ != nullptr)
    terrain_renderer_->RenderWireframe(video_, camera, depth_fbo);

  if (!depth_list_.empty() && depth_fbo != nullptr)
    DrawList(depth_list_, depth_fbo, /*depth_test_on=*/true);

  if (!overlay_list_.empty() && overlay_fbo != nullptr)
    DrawList(overlay_list_, overlay_fbo, /*depth_test_on=*/false);

  video_->SetDepthTestEnabled(false);
  video_->SetDepthWriteEnabled(true);
  video_->SetFaceCulling(abstract::CullFace::kBack);
  video_->SetPrimitiveType(abstract::PrimitiveType::kTriangleList);
}

// ---- Depth-tested geometry helpers ------------------------------------------

void WireframeRenderer::PushSegment(const core::Vec3f& p0,
                                     const core::Vec3f& p1,
                                     const core::Color& color,
                                     const core::Mat4f& transform) {
  AppendSegment(depth_list_, p0, p1, color, transform);
}

void WireframeRenderer::PushBox(const core::Vec3f& half_extents,
                                 const core::Color& color,
                                 const core::Mat4f& transform) {
  AppendBox(depth_list_, half_extents, color, transform);
}

void WireframeRenderer::PushSphere(float radius, const core::Color& color,
                                    const core::Mat4f& transform) {
  AppendSphere(depth_list_, radius, color, transform);
}

void WireframeRenderer::PushCone(float half_angle, float range,
                                  const core::Color& color,
                                  const core::Mat4f& transform) {
  AppendCone(depth_list_, half_angle, range, color, transform);
}

void WireframeRenderer::PushLineList(const std::vector<core::Vec3f>& points,
                                      const core::Color& color,
                                      const core::Mat4f& transform) {
  AppendLineList(depth_list_, points, color, transform);
}

// ---- Overlay geometry helpers -----------------------------------------------

void WireframeRenderer::PushOverlaySegment(const core::Vec3f& p0,
                                            const core::Vec3f& p1,
                                            const core::Color& color,
                                            const core::Mat4f& transform) {
  AppendSegment(overlay_list_, p0, p1, color, transform);
}

void WireframeRenderer::PushOverlayBox(const core::Vec3f& half_extents,
                                        const core::Color& color,
                                        const core::Mat4f& transform) {
  AppendBox(overlay_list_, half_extents, color, transform);
}

void WireframeRenderer::PushOverlaySphere(float radius, const core::Color& color,
                                           const core::Mat4f& transform) {
  AppendSphere(overlay_list_, radius, color, transform);
}

void WireframeRenderer::PushOverlayCone(float half_angle, float range,
                                         const core::Color& color,
                                         const core::Mat4f& transform) {
  AppendCone(overlay_list_, half_angle, range, color, transform);
}

void WireframeRenderer::PushOverlayLineList(
    const std::vector<core::Vec3f>& points,
    const core::Color& color,
    const core::Mat4f& transform) {
  AppendLineList(overlay_list_, points, color, transform);
}

// ---- Private helpers --------------------------------------------------------

core::Vec3f WireframeRenderer::TransformPoint(const core::Vec3f& p,
                                               const core::Mat4f& mat) {
  return {
    mat(0, 0) * p.x + mat(0, 1) * p.y + mat(0, 2) * p.z + mat(0, 3),
    mat(1, 0) * p.x + mat(1, 1) * p.y + mat(1, 2) * p.z + mat(1, 3),
    mat(2, 0) * p.x + mat(2, 1) * p.y + mat(2, 2) * p.z + mat(2, 3),
  };
}

void WireframeRenderer::AppendSegment(std::vector<core::VertexBase>& list,
                                       const core::Vec3f& p0,
                                       const core::Vec3f& p1,
                                       const core::Color& color,
                                       const core::Mat4f& transform) {
  const core::Vec2f uv(0.f, 0.f);
  list.emplace_back(TransformPoint(p0, transform), color, uv);
  list.emplace_back(TransformPoint(p1, transform), color, uv);
}

void WireframeRenderer::AppendCircle(std::vector<core::VertexBase>& list,
                                      const core::Vec3f& ax,
                                      const core::Vec3f& ay,
                                      float radius,
                                      const core::Color& color,
                                      const core::Mat4f& transform) {
  for (int i = 0; i < kCircleSegments; ++i) {
    const float t0 = kTwoPi * static_cast<float>(i)     / kCircleSegments;
    const float t1 = kTwoPi * static_cast<float>(i + 1) / kCircleSegments;
    const core::Vec3f p0 = ax * (radius * std::cos(t0)) + ay * (radius * std::sin(t0));
    const core::Vec3f p1 = ax * (radius * std::cos(t1)) + ay * (radius * std::sin(t1));
    AppendSegment(list, p0, p1, color, transform);
  }
}

void WireframeRenderer::AppendBox(std::vector<core::VertexBase>& list,
                                   const core::Vec3f& h,
                                   const core::Color& color,
                                   const core::Mat4f& transform) {
  // 8 corners in local space.
  const core::Vec3f c[8] = {
    { h.x,  h.y,  h.z}, {-h.x,  h.y,  h.z},
    {-h.x, -h.y,  h.z}, { h.x, -h.y,  h.z},
    { h.x,  h.y, -h.z}, {-h.x,  h.y, -h.z},
    {-h.x, -h.y, -h.z}, { h.x, -h.y, -h.z},
  };
  // 12 edges.
  const int edges[12][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0},  // front face
    {4, 5}, {5, 6}, {6, 7}, {7, 4},  // back face
    {0, 4}, {1, 5}, {2, 6}, {3, 7},  // connecting edges
  };
  for (const auto* e : edges)
    AppendSegment(list, c[e[0]], c[e[1]], color, transform);
}

void WireframeRenderer::AppendSphere(std::vector<core::VertexBase>& list,
                                      float radius,
                                      const core::Color& color,
                                      const core::Mat4f& transform) {
  AppendCircle(list, core::Vec3f::kAxisX, core::Vec3f::kAxisY,
               radius, color, transform);
  AppendCircle(list, core::Vec3f::kAxisX, core::Vec3f::kAxisZ,
               radius, color, transform);
  AppendCircle(list, core::Vec3f::kAxisY, core::Vec3f::kAxisZ,
               radius, color, transform);
}

void WireframeRenderer::AppendCone(std::vector<core::VertexBase>& list,
                                    float half_angle, float range,
                                    const core::Color& color,
                                    const core::Mat4f& transform) {
  const float base_radius = std::tan(half_angle) * range;
  const core::Vec3f apex(0.f, 0.f, 0.f);
  const core::Vec3f base_center(0.f, 0.f, range);

  // Base circle in the XY plane at z=range.
  // Offset the transform to draw at the base center.
  // We handle this by building a composite transform that translates by range
  // along Z inside local space — instead, we pass the base_center offset into
  // AppendCircle by constructing a shifted transform.
  // Simpler: pre-transform base circle points manually.
  for (int i = 0; i < kCircleSegments; ++i) {
    const float t0 = kTwoPi * static_cast<float>(i)     / kCircleSegments;
    const float t1 = kTwoPi * static_cast<float>(i + 1) / kCircleSegments;
    const core::Vec3f p0 = base_center
        + core::Vec3f::kAxisX * (base_radius * std::cos(t0))
        + core::Vec3f::kAxisY * (base_radius * std::sin(t0));
    const core::Vec3f p1 = base_center
        + core::Vec3f::kAxisX * (base_radius * std::cos(t1))
        + core::Vec3f::kAxisY * (base_radius * std::sin(t1));
    AppendSegment(list, p0, p1, color, transform);
  }

  // 4 spokes: apex to base edge at ±X and ±Y.
  const core::Vec3f spoke_dirs[4] = {
    core::Vec3f::kAxisX, -core::Vec3f::kAxisX,
    core::Vec3f::kAxisY, -core::Vec3f::kAxisY,
  };
  for (const core::Vec3f& d : spoke_dirs) {
    const core::Vec3f edge = base_center + d * base_radius;
    AppendSegment(list, apex, edge, color, transform);
  }
}

void WireframeRenderer::AppendLineList(std::vector<core::VertexBase>& list,
                                        const std::vector<core::Vec3f>& points,
                                        const core::Color& color,
                                        const core::Mat4f& transform) {
  const int n = static_cast<int>(points.size());
  for (int i = 0; i + 1 < n; ++i)
    AppendSegment(list, points[i], points[i + 1], color, transform);
}

void WireframeRenderer::EnsureCapacity(int needed) {
  if (needed <= vertex_buf_capacity_) return;

  int cap = std::max(256, vertex_buf_capacity_);
  while (cap < needed) cap *= 2;

  vertex_buf_ = video_->CreateVertexBuffer(
      core::VertexType::kBase, cap, abstract::BufferUsage::kDynamic);
  vertex_buf_capacity_ = cap;
}

void WireframeRenderer::DrawList(const std::vector<core::VertexBase>& list,
                                  abstract::RenderTargetGroup* fbo,
                                  bool depth_test_on) {
  EnsureCapacity(static_cast<int>(list.size()));
  vertex_buf_->Fill(list.data(), static_cast<int>(list.size()));

  fbo->BindForWriting();
  video_->SetDepthTestEnabled(depth_test_on);
  video_->SetDepthWriteEnabled(false);
  video_->SetFaceCulling(abstract::CullFace::kNone);
  video_->SetPrimitiveType(abstract::PrimitiveType::kLineList);

  shader_->Activate();
  vertex_buf_->Bind();
  video_->Render(static_cast<int>(list.size()));

  fbo->UnbindForWriting();
}

}  // namespace renderer
