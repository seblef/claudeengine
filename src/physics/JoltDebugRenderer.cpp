#include <Jolt/Jolt.h>
#include "physics/JoltDebugRenderer.h"

#include <atomic>

#include <Jolt/Math/Vec3.h>

#include "core/Color.h"
#include "core/Vec3f.h"
#include "renderer/WireframeRenderer.h"

namespace physics {

namespace {

/// Converts a Jolt RVec3 (may be double-precision) to engine Vec3f.
core::Vec3f ToVec3f(JPH::RVec3Arg v) {
    return {static_cast<float>(v.GetX()),
            static_cast<float>(v.GetY()),
            static_cast<float>(v.GetZ())};
}

/// Converts a Jolt Color (uint8 RGBA) to engine Color (float RGBA).
core::Color ToColor(JPH::ColorArg c) {
    constexpr float kInv255 = 1.f / 255.f;
    return {static_cast<float>(c.r) * kInv255,
            static_cast<float>(c.g) * kInv255,
            static_cast<float>(c.b) * kInv255,
            static_cast<float>(c.a) * kInv255};
}

/// Triangle batch stored per geometry LOD.  Mirrors DebugRendererSimple's
/// private BatchImpl but lives here so DrawGeometry can safely cast to it.
struct BatchImpl : public JPH::RefTargetVirtual {
    JPH_OVERRIDE_NEW_DELETE

    void AddRef() override { ++ref_count_; }
    void Release() override { if (--ref_count_ == 0) delete this; }

    JPH::Array<JPH::DebugRenderer::Triangle> triangles;

 private:
    std::atomic<JPH::uint32> ref_count_{0};
};

}  // namespace

// ---- Construction -----------------------------------------------------------

JoltDebugRenderer::JoltDebugRenderer() {
    Initialize();
}

// ---- DrawLine ---------------------------------------------------------------

void JoltDebugRenderer::DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo,
                                  JPH::ColorArg inColor) {
    if (!renderer::WireframeRenderer::IsInstanced()) return;
    renderer::WireframeRenderer::Instance().PushOverlaySegment(
        ToVec3f(inFrom), ToVec3f(inTo), ToColor(inColor));
}

// ---- DrawTriangle -----------------------------------------------------------

void JoltDebugRenderer::DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2,
                                      JPH::RVec3Arg inV3, JPH::ColorArg inColor,
                                      ECastShadow /*inCastShadow*/) {
    DrawLine(inV1, inV2, inColor);
    DrawLine(inV2, inV3, inColor);
    DrawLine(inV3, inV1, inColor);
}

// ---- DrawText3D -------------------------------------------------------------

void JoltDebugRenderer::DrawText3D(JPH::RVec3Arg /*inPosition*/,
                                    const JPH::string_view& /*inString*/,
                                    JPH::ColorArg /*inColor*/,
                                    float /*inHeight*/) {
    // Text rendering is not supported; no-op.
}

// ---- CreateTriangleBatch ----------------------------------------------------

JPH::DebugRenderer::Batch JoltDebugRenderer::CreateTriangleBatch(
        const Triangle* inTriangles, int inTriangleCount) {
    auto* batch = new BatchImpl;
    if (inTriangles && inTriangleCount > 0)
        batch->triangles.assign(inTriangles, inTriangles + inTriangleCount);
    return batch;
}

JPH::DebugRenderer::Batch JoltDebugRenderer::CreateTriangleBatch(
        const Vertex* inVertices, int inVertexCount,
        const JPH::uint32* inIndices, int inIndexCount) {
    auto* batch = new BatchImpl;
    if (!inVertices || inVertexCount == 0 || !inIndices || inIndexCount == 0)
        return batch;

    const int tri_count = inIndexCount / 3;
    batch->triangles.resize(tri_count);
    for (int t = 0; t < tri_count; ++t) {
        batch->triangles[t].mV[0] = inVertices[inIndices[t * 3 + 0]];
        batch->triangles[t].mV[1] = inVertices[inIndices[t * 3 + 1]];
        batch->triangles[t].mV[2] = inVertices[inIndices[t * 3 + 2]];
    }
    return batch;
}

// ---- DrawGeometry -----------------------------------------------------------

void JoltDebugRenderer::DrawGeometry(JPH::RMat44Arg inModelMatrix,
                                      const JPH::AABox& /*inWorldSpaceBounds*/,
                                      float /*inLODScaleSq*/,
                                      JPH::ColorArg inModelColor,
                                      const GeometryRef& inGeometry,
                                      ECullMode /*inCullMode*/,
                                      ECastShadow inCastShadow,
                                      EDrawMode inDrawMode) {
    if (inGeometry->mLODs.empty()) return;

    // Scale by 1.02 in local space so shapes remain visible above mesh surfaces.
    const JPH::RMat44 scaled = inModelMatrix.PreScaled(
        JPH::Vec3::sReplicate(1.02f));

    const LOD& lod = inGeometry->mLODs[0];
    const auto* batch =
        static_cast<const BatchImpl*>(lod.mTriangleBatch.GetPtr());

    for (const Triangle& tri : batch->triangles) {
        const JPH::RVec3 v0 = scaled * JPH::Vec3(tri.mV[0].mPosition);
        const JPH::RVec3 v1 = scaled * JPH::Vec3(tri.mV[1].mPosition);
        const JPH::RVec3 v2 = scaled * JPH::Vec3(tri.mV[2].mPosition);
        const JPH::Color color = inModelColor * tri.mV[0].mColor;

        if (inDrawMode == EDrawMode::Wireframe) {
            DrawLine(v0, v1, color);
            DrawLine(v1, v2, color);
            DrawLine(v2, v0, color);
        } else {
            DrawTriangle(v0, v1, v2, color, inCastShadow);
        }
    }
}

}  // namespace physics
