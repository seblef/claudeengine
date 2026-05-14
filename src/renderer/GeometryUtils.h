#pragma once

#include <memory>

#include "abstract/VideoDevice.h"
#include "renderer/GeometryData.h"

namespace renderer {

class Material;
class RenderableMesh;

// ---- Primitive geometry builders --------------------------------------------
//
// All functions return a GeometryData using the k3D vertex layout (Vertex3D).
// Only the position field is populated; normal/binormal/tangent/UV are zeroed.
// The returned object is owned by the caller.

// Fullscreen quad: two triangles covering NDC [-1,1]² at z=0.
// Intended for full-screen passes (GlobalLight, composite, debug blit).
[[nodiscard]] std::unique_ptr<GeometryData> CreateQuad(
    abstract::VideoDevice* video);

// UV sphere of radius 1.
// stacks: number of latitude bands; rings: number of longitude bands.
// Vertex count = (stacks+1)×(rings+1); triangle count = stacks×rings×2.
[[nodiscard]] std::unique_ptr<GeometryData> CreateSphere(
    abstract::VideoDevice* video, int stacks, int rings);

// Unit cone: apex at origin, base circle (radius=1) at z=1.
// n: number of segments on the base circle; produces 2×n triangles.
[[nodiscard]] std::unique_ptr<GeometryData> CreateCone(
    abstract::VideoDevice* video, int n);

// Unit pyramid: apex at origin, rectangular base (±0.5 on X and Y) at z=1.
// 6 triangles (4 side faces + 2 base triangles).
[[nodiscard]] std::unique_ptr<GeometryData> CreatePyramid(
    abstract::VideoDevice* video);

// Horizontal plane facing up (+Y normal), centered at the origin in the XZ plane.
// half_size: half-extent along both X and Z axes.
// material: ownership is transferred — do not use the pointer after this call.
// UVs are tiled: one tile covers 5 world units (tiles = half_size / 5).
// Returns a heap-allocated RenderableMesh; ownership belongs to the caller.
[[nodiscard]] RenderableMesh* CreatePlaneMesh(
    abstract::VideoDevice* video, float half_size, Material* material);

}  // namespace renderer
