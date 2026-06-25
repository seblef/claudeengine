#pragma once

#include <array>
#include <memory>
#include <vector>

#include "core/Vec3f.h"
#include "physics/SurfaceMaterial.h"

namespace abstract {
class IndexBuffer;
class Shader;
class Texture;
class VertexBuffer;
class VideoDevice;
}  // namespace abstract

namespace core { class Camera; }

namespace physics { class PhysicsVehicle; }

namespace track {

/// Maximum number of track quads maintained per wheel.
constexpr int kMaxQuadsPerWheel = 512;

/// Single oriented quad emitted by a wheel on ground contact.
struct TrackQuad {
    // cppcheck-suppress unusedStructMember
    core::Vec3f           center;   ///< World-space center of the quad.
    // cppcheck-suppress unusedStructMember
    core::Vec3f           forward;  ///< Normalised wheel heading at emission time.
    // cppcheck-suppress unusedStructMember
    float                 alpha;    ///< Fade factor: 1.0 at emission, 0.0 when dead.
    // cppcheck-suppress unusedStructMember
    physics::SurfaceType  surface;  ///< Determines texture selection.
};

/// Manages per-wheel tire track ribbons for all registered vehicles.
///
/// One TireTrackSystem is created at game/world level (e.g., in GameSystem) and
/// registered with the Renderer.  GameVehicle calls RegisterVehicle / UnregisterVehicle
/// when its physics vehicle is activated or deactivated.
///
/// Update() must be called after PhysicsSystem::Step() each frame.
/// Render() is invoked by Renderer in the forward pass (SrcAlpha blend).
class TireTrackSystem {
 public:
    /// @param video  VideoDevice used to allocate GPU resources.  Must outlive this system.
    explicit TireTrackSystem(abstract::VideoDevice* video);
    ~TireTrackSystem();

    TireTrackSystem(const TireTrackSystem&)            = delete;
    TireTrackSystem& operator=(const TireTrackSystem&) = delete;
    TireTrackSystem(TireTrackSystem&&)                 = delete;
    TireTrackSystem& operator=(TireTrackSystem&&)      = delete;

    /// Assigns the texture used for the given surface type.
    /// Tracks on that surface will sample this texture.  Pass nullptr to use no texture.
    void SetTrackTexture(physics::SurfaceType type, abstract::Texture* tex);

    /// Minimum travel distance in metres before a new quad is emitted (default 0.10 m).
    void SetEmitInterval(float metres)    { emit_interval_  = metres; }

    /// Time in seconds for a quad to fade from alpha=1 to alpha=0 (default 8 s).
    void SetFadeDuration(float seconds)   { fade_duration_  = seconds; }

    /// Register a physics vehicle.  Allocates per-wheel GPU and CPU state.
    /// Safe to call with a vehicle that is already registered (no-op).
    void RegisterVehicle(const physics::PhysicsVehicle* vehicle);

    /// Unregister a physics vehicle and release its per-wheel resources.
    /// Safe to call with nullptr or an unregistered vehicle.
    void UnregisterVehicle(const physics::PhysicsVehicle* vehicle);

    /// Advance track state by dt seconds.
    /// Queries wheel contacts, emits new quads, and ages existing ones.
    /// Must be called after PhysicsSystem::Step() and before Render().
    void Update(float dt);

    /// Render all live track quads into the currently bound framebuffer.
    /// The caller must have set SrcAlpha blend, disabled depth write, and enabled depth test.
    void Render(const core::Camera& camera);

 private:
    /// Per-wheel CPU and GPU state.
    struct WheelState {
        // Ring buffer storage.
        // cppcheck-suppress unusedStructMember
        std::array<TrackQuad, kMaxQuadsPerWheel> quads;
        // cppcheck-suppress unusedStructMember
        int          head         = 0;    ///< Index of the oldest live quad.
        // cppcheck-suppress unusedStructMember
        int          count        = 0;    ///< Number of live quads in the ring.
        // cppcheck-suppress unusedStructMember
        bool         has_last_pos = false;
        // cppcheck-suppress unusedStructMember
        core::Vec3f  last_pos;
        // cppcheck-suppress unusedStructMember
        float        half_width   = 0.25f;  ///< Half the physical wheel width (metres).

        // GPU resources — one VBO per wheel, pre-allocated.
        // cppcheck-suppress unusedStructMember
        std::unique_ptr<abstract::VertexBuffer> vbo;
    };

    /// All wheels tracked by this system (flattened across all registered vehicles).
    struct VehicleEntry {
        // cppcheck-suppress unusedStructMember
        const physics::PhysicsVehicle*  vehicle;
        // cppcheck-suppress unusedStructMember
        std::vector<WheelState>         wheels;  // one entry per GetWheelCount()
    };

    /// Rebuild vertex data and issue draw calls for one wheel.
    void RenderWheel(WheelState& ws);

    // cppcheck-suppress unusedStructMember
    abstract::VideoDevice*  video_;
    // cppcheck-suppress unusedStructMember
    abstract::Shader*       shader_       = nullptr;
    // cppcheck-suppress unusedStructMember
    std::unique_ptr<abstract::IndexBuffer> ibo_;  ///< Shared static IBO for all wheels.

    // cppcheck-suppress unusedStructMember
    std::array<abstract::Texture*, physics::kSurfaceTypeCount> textures_ = {};

    // cppcheck-suppress unusedStructMember
    float emit_interval_ = 0.10f;
    // cppcheck-suppress unusedStructMember
    float fade_duration_ = 8.0f;

    // cppcheck-suppress unusedStructMember
    std::vector<VehicleEntry> entries_;
};

}  // namespace track
