#pragma once

#include <memory>
#include <vector>

#include "abstract/RenderTargetGroup.h"
#include "abstract/Shader.h"
#include "abstract/VertexBuffer.h"
#include "abstract/VideoDevice.h"
#include "core/Camera.h"
#include "core/Color.h"
#include "core/Mat4f.h"
#include "core/Singleton.h"
#include "core/Vec3f.h"
#include "core/VertexBase.h"

namespace terrain { class TerrainRenderer; }

namespace renderer {

// Singleton debug wireframe renderer for the renderer module.
//
// Replaces editor-side LightWireframeRenderer, ParticleGizmoRenderer, and
// PlayerStartGizmoRenderer with a single unified draw path that lives inside
// the renderer module so that Light::Enqueue() and ParticleRenderable::Enqueue()
// can push geometry during the visibility pass without creating a dependency on
// the editor module.
//
// Two output lists are maintained per frame:
//   depth_list_   — drawn with depth test ON (occluded by opaque geometry).
//   overlay_list_ — drawn with depth test OFF (always on top).
//
// Both lists are cleared by BeginFrame() and uploaded by Render().
// Callers push geometry with PushSegment / PushBox / PushSphere / PushCone /
// PushCylinder / PushCapsule / PushLineList (depth-tested) and their
// PushOverlay* twins (overlay).
//
// Lifecycle: new WireframeRenderer(video) → BeginFrame/Render calls →
//   Shutdown().  Created and destroyed by Renderer.
class WireframeRenderer : public core::Singleton<WireframeRenderer> {
 public:
  // video must outlive this renderer.
  explicit WireframeRenderer(abstract::VideoDevice* video);
  ~WireframeRenderer();

  WireframeRenderer(const WireframeRenderer&)            = delete;
  WireframeRenderer& operator=(const WireframeRenderer&) = delete;

  // ---- Frame lifecycle -------------------------------------------------------

  // Clears both vertex lists. Called by Renderer::Update() at frame start.
  void BeginFrame();

  // Uploads both lists and issues draw calls.
  //
  // Step 1: if terrain_wireframe_enabled_ and terrain_renderer_ != nullptr,
  //         calls terrain_renderer_->RenderWireframe().
  // Step 2: uploads depth_list_, binds depth_fbo, draws with depth test ON /
  //         depth write OFF.
  // Step 3: uploads overlay_list_, binds overlay_fbo, draws with depth test
  //         OFF / depth write OFF.
  // Step 4: restores depth test OFF, depth write ON, back culling, triangle
  //         list.
  //
  // Either fbo may be nullptr; the corresponding step is then skipped.
  void Render(const core::Camera& camera,
              abstract::RenderTargetGroup* depth_fbo,
              abstract::RenderTargetGroup* overlay_fbo);

  // ---- Configuration ---------------------------------------------------------

  void SetGizmosEnabled(bool enabled) { gizmos_enabled_ = enabled; }
  void SetTerrainWireframeEnabled(bool enabled) {
    terrain_wireframe_enabled_ = enabled;
  }
  // Non-owned pointer; caller must ensure it outlives this renderer (or pass
  // nullptr to detach).
  void SetTerrainRenderer(terrain::TerrainRenderer* tr) {
    terrain_renderer_ = tr;
  }
  // Stores an opaque key used to highlight the matching object. The key is
  // never dereferenced — callers pass their own address and compare with
  // IsHighlighted().
  void SetHighlightedObject(const void* key) { highlighted_key_ = key; }

  [[nodiscard]] bool AreGizmosEnabled() const { return gizmos_enabled_; }
  [[nodiscard]] bool IsHighlighted(const void* key) const {
    return key != nullptr && key == highlighted_key_;
  }

  // ---- Depth-tested geometry helpers -----------------------------------------

  // Appends a single line segment to depth_list_.
  void PushSegment(const core::Vec3f& p0, const core::Vec3f& p1,
                   const core::Color& color,
                   const core::Mat4f& transform = core::Mat4f::kIdentity);

  // Appends 12 edges of an axis-aligned box (±half_extents in local space).
  void PushBox(const core::Vec3f& half_extents, const core::Color& color,
               const core::Mat4f& transform = core::Mat4f::kIdentity);

  // Appends three orthogonal great circles approximating a sphere.
  void PushSphere(float radius, const core::Color& color,
                  const core::Mat4f& transform = core::Mat4f::kIdentity);

  // Appends a cone along local +Z: apex at origin, base at (0,0,range),
  // base radius = tan(half_angle)*range, with 4 apex-to-base spokes.
  void PushCone(float half_angle, float range, const core::Color& color,
                const core::Mat4f& transform = core::Mat4f::kIdentity);

  // Appends a cylinder along local Y: two XZ-plane circles at ±half_height
  // connected by 4 vertical edges at 0°, 90°, 180°, 270°.
  void PushCylinder(float radius, float half_height, const core::Color& color,
                    const core::Mat4f& transform = core::Mat4f::kIdentity);

  // Appends a capsule along local Y: cylinder body plus two hemisphere caps
  // (each drawn as two 180° arcs in the XY and ZY planes).
  void PushCapsule(float radius, float half_height, const core::Color& color,
                   const core::Mat4f& transform = core::Mat4f::kIdentity);

  // Appends a sequence of line segments connecting adjacent points in points[].
  void PushLineList(const std::vector<core::Vec3f>& points,
                    const core::Color& color,
                    const core::Mat4f& transform = core::Mat4f::kIdentity);

  // ---- Overlay geometry helpers (no depth test) ------------------------------

  void PushOverlaySegment(const core::Vec3f& p0, const core::Vec3f& p1,
                          const core::Color& color,
                          const core::Mat4f& transform = core::Mat4f::kIdentity);

  void PushOverlayBox(const core::Vec3f& half_extents, const core::Color& color,
                      const core::Mat4f& transform = core::Mat4f::kIdentity);

  void PushOverlaySphere(float radius, const core::Color& color,
                         const core::Mat4f& transform = core::Mat4f::kIdentity);

  void PushOverlayCone(float half_angle, float range, const core::Color& color,
                       const core::Mat4f& transform = core::Mat4f::kIdentity);

  void PushOverlayCylinder(float radius, float half_height,
                            const core::Color& color,
                            const core::Mat4f& transform = core::Mat4f::kIdentity);

  void PushOverlayCapsule(float radius, float half_height,
                           const core::Color& color,
                           const core::Mat4f& transform = core::Mat4f::kIdentity);

  void PushOverlayLineList(const std::vector<core::Vec3f>& points,
                           const core::Color& color,
                           const core::Mat4f& transform = core::Mat4f::kIdentity);

 private:
  // Transforms p by mat (treating p as a point, w=1) and returns world-space.
  static core::Vec3f TransformPoint(const core::Vec3f& p, const core::Mat4f& mat);

  // Appends two vertices for a single segment to list.
  static void AppendSegment(std::vector<core::VertexBase>& list,
                             const core::Vec3f& p0, const core::Vec3f& p1,
                             const core::Color& color,
                             const core::Mat4f& transform);

  // Appends 32 segments approximating a circle.
  // ax and ay are the two local-space axes defining the circle plane.
  static void AppendCircle(std::vector<core::VertexBase>& list,
                            const core::Vec3f& ax, const core::Vec3f& ay,
                            float radius, const core::Color& color,
                            const core::Mat4f& transform);

  // Appends box / sphere / cone / line-list geometry to the given list.
  static void AppendBox(std::vector<core::VertexBase>& list,
                        const core::Vec3f& half_extents, const core::Color& color,
                        const core::Mat4f& transform);

  static void AppendSphere(std::vector<core::VertexBase>& list,
                            float radius, const core::Color& color,
                            const core::Mat4f& transform);

  static void AppendCone(std::vector<core::VertexBase>& list,
                          float half_angle, float range, const core::Color& color,
                          const core::Mat4f& transform);

  static void AppendCylinder(std::vector<core::VertexBase>& list,
                              float radius, float half_height,
                              const core::Color& color,
                              const core::Mat4f& transform);

  static void AppendCapsule(std::vector<core::VertexBase>& list,
                             float radius, float half_height,
                             const core::Color& color,
                             const core::Mat4f& transform);

  static void AppendLineList(std::vector<core::VertexBase>& list,
                              const std::vector<core::Vec3f>& points,
                              const core::Color& color,
                              const core::Mat4f& transform);

  // Grows vertex_buf_ geometrically (power-of-two) to hold at least `needed`
  // vertices.  No-op if current capacity already suffices.
  void EnsureCapacity(int needed);

  // Uploads list to vertex_buf_, binds fbo, and issues a line-list draw call
  // with the depth_test_on flag selecting render state.
  void DrawList(const std::vector<core::VertexBase>& list,
                abstract::RenderTargetGroup* fbo,
                bool depth_test_on);

  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice* video_;
  // cppcheck-suppress unusedStructMember
  abstract::Shader*      shader_;

  // Single shared dynamic vertex buffer, resized on demand.
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<abstract::VertexBuffer> vertex_buf_;
  // cppcheck-suppress unusedStructMember
  int vertex_buf_capacity_ = 0;

  // cppcheck-suppress unusedStructMember
  std::vector<core::VertexBase> depth_list_;
  // cppcheck-suppress unusedStructMember
  std::vector<core::VertexBase> overlay_list_;

  // cppcheck-suppress unusedStructMember
  bool                    gizmos_enabled_           = false;
  // cppcheck-suppress unusedStructMember
  bool                    terrain_wireframe_enabled_ = false;
  // cppcheck-suppress unusedStructMember
  const void*             highlighted_key_           = nullptr;
  // cppcheck-suppress unusedStructMember
  terrain::TerrainRenderer* terrain_renderer_        = nullptr;
};

}  // namespace renderer
