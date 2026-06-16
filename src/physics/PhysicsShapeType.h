#pragma once

namespace physics {

/// Primitive or mesh-based shape variants for physics bodies.
enum class PhysicsShapeType {
    Box,         ///< Axis-aligned box defined by half-extents.
    Sphere,      ///< Sphere defined by a radius.
    Cylinder,    ///< Upright cylinder defined by radius and half-height.
    Capsule,     ///< Capsule (cylinder + hemispherical caps) defined by radius and half-height.
    Terrain,     ///< Heightfield; geometry supplied via TerrainData at body-creation time.
    ConvexHull,  ///< Convex hull; geometry supplied via MeshData at body-creation time.
    Exact,       ///< Exact triangle mesh; geometry supplied via MeshData at body-creation time.
};

}  // namespace physics
