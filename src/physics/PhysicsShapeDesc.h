#pragma once

#include "core/Vec3f.h"
#include "physics/PhysicsShapeType.h"

namespace physics {

/// Per-shape parameter sets — only the active union member is meaningful.
struct PhysicsShapeDesc {
    /// Axis-aligned box parameters.
    struct Box {
        core::Vec3f half_extents;  ///< Half-sizes along X, Y, Z axes.
    };

    /// Sphere parameters.
    struct Sphere {
        float radius;  ///< Sphere radius.
    };

    /// Upright cylinder parameters.
    struct Cylinder {
        float radius;       ///< Cylinder radius.
        float half_height;  ///< Half the total height of the cylinder.
    };

    /// Capsule parameters.
    struct Capsule {
        float radius;       ///< Hemisphere radius and cylinder radius.
        float half_height;  ///< Half the height of the cylindrical section (excluding caps).
    };

    PhysicsShapeType type = PhysicsShapeType::Box;  ///< Active shape variant.

    union {
        Box      box;
        Sphere   sphere;
        Cylinder cylinder;
        Capsule  capsule;
        // ConvexHull, Exact, and Terrain carry no extra parameters here;
        // geometry is supplied via MeshData / TerrainData at body-creation time.
    };

    // Factory helpers for concise construction.

    static PhysicsShapeDesc MakeBox(const core::Vec3f& half_extents) {
        PhysicsShapeDesc d;
        d.type           = PhysicsShapeType::Box;
        d.box.half_extents = half_extents;
        return d;
    }

    static PhysicsShapeDesc MakeSphere(float radius) {
        PhysicsShapeDesc d;
        d.type          = PhysicsShapeType::Sphere;
        d.sphere.radius = radius;
        return d;
    }

    static PhysicsShapeDesc MakeCylinder(float radius, float half_height) {
        PhysicsShapeDesc d;
        d.type                 = PhysicsShapeType::Cylinder;
        d.cylinder.radius      = radius;
        d.cylinder.half_height = half_height;
        return d;
    }

    static PhysicsShapeDesc MakeCapsule(float radius, float half_height) {
        PhysicsShapeDesc d;
        d.type                = PhysicsShapeType::Capsule;
        d.capsule.radius      = radius;
        d.capsule.half_height = half_height;
        return d;
    }

    static PhysicsShapeDesc MakeTerrain() {
        PhysicsShapeDesc d;
        d.type = PhysicsShapeType::Terrain;
        return d;
    }

    static PhysicsShapeDesc MakeConvexHull() {
        PhysicsShapeDesc d;
        d.type = PhysicsShapeType::ConvexHull;
        return d;
    }

    static PhysicsShapeDesc MakeExact() {
        PhysicsShapeDesc d;
        d.type = PhysicsShapeType::Exact;
        return d;
    }
};

}  // namespace physics
