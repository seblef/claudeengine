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

    PhysicsShapeType type;  ///< Active shape variant.

    union {
        Box      box;
        Sphere   sphere;
        Cylinder cylinder;
        Capsule  capsule;
        // ConvexHull, Exact, and Terrain carry no extra parameters here;
        // geometry is supplied via MeshData / TerrainData at body-creation time.
    };

    /// Offset from the body's local origin to the shape centre, in local space.
    /// Applied via RotatedTranslatedShape so the shape aligns with the mesh
    /// geometry even when the mesh local origin is not at the geometry centre
    /// (e.g. a tree whose pivot is at the base).
    core::Vec3f center_offset{0.f, 0.f, 0.f};

    // Vec3f has default member-initializers, making Box's default constructor
    // non-trivial and the anonymous union's default constructor deleted.
    // An explicit default constructor initialises the union through Box with
    // safe non-zero half-extents (Jolt rejects zero-size shapes at runtime).
    PhysicsShapeDesc() : type(PhysicsShapeType::Box), box{{0.5f, 0.5f, 0.5f}} {}

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

    [[nodiscard]] bool operator==(const PhysicsShapeDesc& o) const {
        if (type != o.type) return false;
        if (center_offset != o.center_offset) return false;
        switch (type) {
            case PhysicsShapeType::Box:
                return box.half_extents == o.box.half_extents;
            case PhysicsShapeType::Sphere:
                return sphere.radius == o.sphere.radius;
            case PhysicsShapeType::Cylinder:
                return cylinder.radius      == o.cylinder.radius
                    && cylinder.half_height == o.cylinder.half_height;
            case PhysicsShapeType::Capsule:
                return capsule.radius      == o.capsule.radius
                    && capsule.half_height == o.capsule.half_height;
            default:
                return true;  // Terrain / ConvexHull / Exact carry no parameters.
        }
    }
    [[nodiscard]] bool operator!=(const PhysicsShapeDesc& o) const {
        return !(*this == o);
    }
};

}  // namespace physics
