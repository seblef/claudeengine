#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Renderer/DebugRenderer.h>

namespace physics {

/// JPH::DebugRenderer bridge that forwards all draw calls to WireframeRenderer.
///
/// Shapes are drawn at a 1.02× uniform scale so they remain visible when they
/// perfectly overlay the rendered mesh surface.
///
/// Jolt types are confined to this header and JoltDebugRenderer.cpp; the class
/// must not appear in any header consumed by game/ or editor/.
class JoltDebugRenderer final : public JPH::DebugRenderer {
 public:
    JoltDebugRenderer();

    // ---- JPH::DebugRenderer interface ----------------------------------------

    void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo,
                  JPH::ColorArg inColor) override;

    void DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2,
                      JPH::RVec3Arg inV3, JPH::ColorArg inColor,
                      ECastShadow inCastShadow) override;

    void DrawText3D(JPH::RVec3Arg inPosition, const JPH::string_view& inString,
                    JPH::ColorArg inColor, float inHeight) override;

    /// Creates an opaque batch from a flat triangle array.
    Batch CreateTriangleBatch(const Triangle* inTriangles,
                              int inTriangleCount) override;

    /// Creates an opaque batch from an indexed vertex array.
    Batch CreateTriangleBatch(const Vertex* inVertices, int inVertexCount,
                              const JPH::uint32* inIndices,
                              int inIndexCount) override;

    /// Draws geometry batches via WireframeRenderer with a 1.02× scale applied
    /// to inModelMatrix so that shapes drawn exactly at mesh surface remain
    /// visible.
    void DrawGeometry(JPH::RMat44Arg inModelMatrix,
                      const JPH::AABox& inWorldSpaceBounds, float inLODScaleSq,
                      JPH::ColorArg inModelColor, const GeometryRef& inGeometry,
                      ECullMode inCullMode, ECastShadow inCastShadow,
                      EDrawMode inDrawMode) override;
};

}  // namespace physics
