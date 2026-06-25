#include "track/TireTrackSystem.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "abstract/BlendFactor.h"
#include "abstract/BufferUsage.h"
#include "abstract/CompareFunc.h"
#include "abstract/IndexBuffer.h"
#include "abstract/IndexType.h"
#include "abstract/PrimitiveType.h"
#include "abstract/Shader.h"
#include "abstract/Texture.h"
#include "abstract/VertexBuffer.h"
#include "abstract/VideoDevice.h"
#include "core/Camera.h"
#include "core/MathUtils.h"
#include "core/Vec3f.h"
#include "core/VertexTrack.h"
#include "core/VertexType.h"
#include "physics/PhysicsSystem.h"
#include "physics/PhysicsVehicle.h"

namespace track {

namespace {

constexpr float kZFightOffset = 0.005f;  // metres; applied in vertex shader via world-up

// Builds the index buffer once: for each quad slot k, emits triangle pair
// [4k, 4k+1, 4k+2,  4k+2, 4k+1, 4k+3].
std::vector<uint32_t> BuildIBOData() {
    std::vector<uint32_t> indices;
    indices.reserve(kMaxQuadsPerWheel * 6);
    for (int k = 0; k < kMaxQuadsPerWheel; ++k) {
        const uint32_t base = static_cast<uint32_t>(k * 4);
        indices.push_back(base);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 2);
        indices.push_back(base + 1);
        indices.push_back(base + 3);
    }
    return indices;
}

// Builds 4 world-space vertices for a single track quad.
// v0=back-left  v1=back-right  v2=front-left  v3=front-right
void BuildQuadVertices(const TrackQuad& q,
                       float half_width, float half_length,
                       core::VertexTrack out[4]) {
    const core::Vec3f world_up(0.f, 1.f, 0.f);
    // Derive the right vector from heading and world-up.
    core::Vec3f right = q.forward.Cross(world_up);
    const float rlen  = right.Length();
    if (rlen > 1e-6f) right = right * (1.f / rlen);
    else right = core::Vec3f(1.f, 0.f, 0.f);  // degenerate: heading = world-up

    const core::Vec3f back_center  = q.center - q.forward * half_length;
    const core::Vec3f front_center = q.center + q.forward * half_length;

    out[0] = core::VertexTrack(back_center  - right * half_width, core::Vec2f(0.f, 0.f), q.alpha);
    out[1] = core::VertexTrack(back_center  + right * half_width, core::Vec2f(1.f, 0.f), q.alpha);
    out[2] = core::VertexTrack(front_center - right * half_width, core::Vec2f(0.f, 1.f), q.alpha);
    out[3] = core::VertexTrack(front_center + right * half_width, core::Vec2f(1.f, 1.f), q.alpha);
}

}  // namespace

TireTrackSystem::TireTrackSystem(abstract::VideoDevice* video)
    : video_(video) {
    shader_ = video_->CreateShader("forward/tire_track");

    // Build the shared static IBO once.
    const std::vector<uint32_t> idx_data = BuildIBOData();
    ibo_ = video_->CreateIndexBuffer(
        abstract::IndexType::kUInt32,
        kMaxQuadsPerWheel * 6,
        abstract::BufferUsage::kImmutable,
        idx_data.data());
}

TireTrackSystem::~TireTrackSystem() {
    if (shader_) shader_->Release();
}

void TireTrackSystem::SetTrackTexture(physics::SurfaceType type,
                                       abstract::Texture* tex) {
    textures_[static_cast<int>(type)] = tex;
}

void TireTrackSystem::RegisterVehicle(const physics::PhysicsVehicle* vehicle) {
    if (!vehicle) return;
    // Avoid double-registration.
    const bool already_registered = std::any_of(
        entries_.begin(), entries_.end(),
        [vehicle](const VehicleEntry& e) { return e.vehicle == vehicle; });
    if (already_registered) return;

    VehicleEntry entry;
    entry.vehicle = vehicle;
    const int wheel_count = vehicle->GetWheelCount();
    entry.wheels.resize(wheel_count);

    for (int i = 0; i < wheel_count; ++i) {
        entry.wheels[i].half_width = vehicle->GetWheelWidth(i) * 0.5f;
        entry.wheels[i].vbo = video_->CreateVertexBuffer(
            core::VertexType::kTrack,
            kMaxQuadsPerWheel * 4,
            abstract::BufferUsage::kDynamic);
    }
    entries_.push_back(std::move(entry));
}

void TireTrackSystem::UnregisterVehicle(const physics::PhysicsVehicle* vehicle) {
    if (!vehicle) return;
    entries_.erase(
        std::remove_if(entries_.begin(), entries_.end(),
                       [vehicle](const VehicleEntry& e) {
                           return e.vehicle == vehicle;
                       }),
        entries_.end());
}

void TireTrackSystem::Update(float dt) {
    const float alpha_decay = (fade_duration_ > 0.f) ? dt / fade_duration_ : 1.f;
    const float emit_interval_sq = emit_interval_ * emit_interval_;

    for (auto& entry : entries_) {
        const physics::PhysicsVehicle* vehicle = entry.vehicle;
        for (int wi = 0; wi < static_cast<int>(entry.wheels.size()); ++wi) {
            WheelState& ws = entry.wheels[wi];

            // Age all live quads; prune dead ones from the ring head.
            for (int k = 0; k < ws.count; ++k) {
                const int idx = (ws.head + k) % kMaxQuadsPerWheel;
                ws.quads[idx].alpha -= alpha_decay;
            }
            while (ws.count > 0 && ws.quads[ws.head % kMaxQuadsPerWheel].alpha <= 0.f) {
                ws.head = (ws.head + 1) % kMaxQuadsPerWheel;
                --ws.count;
            }

            // Emit a new quad if the wheel is in contact and has moved enough.
            const physics::WheelContact contact = vehicle->GetWheelContact(wi);
            if (!contact.has_contact) {
                ws.has_last_pos = false;
                continue;
            }

            const bool should_emit =
                !ws.has_last_pos ||
                contact.position.SquaredDistance(ws.last_pos) >= emit_interval_sq;

            if (should_emit) {
                core::Vec3f heading;
                float       half_len = emit_interval_ * 0.5f;
                if (ws.has_last_pos) {
                    const core::Vec3f delta = contact.position - ws.last_pos;
                    const float dist = delta.Length();
                    if (dist > 1e-6f) {
                        heading  = delta * (1.f / dist);
                        half_len = dist * 0.5f;
                    } else {
                        heading = core::Vec3f(0.f, 0.f, 1.f);
                    }
                } else {
                    heading = core::Vec3f(0.f, 0.f, 1.f);
                }

                // Write at tail = (head + count) % kMaxQuadsPerWheel.
                // If the ring is full, overwrite the oldest entry (advance head).
                const int tail = (ws.head + ws.count) % kMaxQuadsPerWheel;
                ws.quads[tail].center      = contact.position;
                ws.quads[tail].forward     = heading;
                ws.quads[tail].alpha       = 1.f;
                ws.quads[tail].half_length = half_len;

                physics::SurfaceType surface = physics::SurfaceType::kGeneric;
                if (physics::PhysicsSystem::IsInstanced())
                    surface = physics::PhysicsSystem::Instance().GetSurfaceType(
                        contact.contact_body_id);
                ws.quads[tail].surface = surface;

                if (ws.count < kMaxQuadsPerWheel) {
                    ++ws.count;
                } else {
                    // Ring full: oldest is overwritten; advance head.
                    ws.head = (ws.head + 1) % kMaxQuadsPerWheel;
                }

                ws.last_pos     = contact.position;
                ws.has_last_pos = true;
            }
        }
    }
}

void TireTrackSystem::Render(const core::Camera& camera) {
    if (!shader_ || !ibo_) return;

    shader_->Activate();
    ibo_->Bind();
    video_->SetBlendEnabled(true, abstract::BlendFactor::kSrcAlpha,
                            abstract::BlendFactor::kOneMinusSrcAlpha);
    video_->SetPrimitiveType(abstract::PrimitiveType::kTriangleList);
    video_->SetIndexType(abstract::IndexType::kUInt32);

    for (auto& entry : entries_) {
        for (auto& ws : entry.wheels) {
            if (ws.count == 0) continue;
            RenderWheel(ws);
        }
    }
}

void TireTrackSystem::RenderWheel(WheelState& ws) {
    // Collect live quads bucketed by surface type.
    // Layout in the VBO: [generic...][terrain...][road...]
    static thread_local std::vector<core::VertexTrack> scratch;
    scratch.clear();

    const float half_width  = ws.half_width;

    // Count quads per surface type to compute offsets.
    int counts[physics::kSurfaceTypeCount] = {};
    for (int k = 0; k < ws.count; ++k) {
        const int idx = (ws.head + k) % kMaxQuadsPerWheel;
        const int st  = static_cast<int>(ws.quads[idx].surface);
        if (st >= 0 && st < physics::kSurfaceTypeCount) ++counts[st];
    }

    int offsets[physics::kSurfaceTypeCount] = {};
    offsets[0] = 0;
    for (int t = 1; t < physics::kSurfaceTypeCount; ++t)
        offsets[t] = offsets[t - 1] + counts[t - 1];

    const int total = counts[0] + counts[1] + counts[2];
    if (total == 0) return;

    // Write vertices into scratch in surface-type order.
    scratch.resize(static_cast<size_t>(total * 4));
    int cursor[physics::kSurfaceTypeCount] = {offsets[0], offsets[1], offsets[2]};

    for (int k = 0; k < ws.count; ++k) {
        const int idx = (ws.head + k) % kMaxQuadsPerWheel;
        const TrackQuad& q = ws.quads[idx];
        if (q.alpha <= 0.f) continue;
        const int st = static_cast<int>(q.surface);
        if (st < 0 || st >= physics::kSurfaceTypeCount) continue;

        core::VertexTrack verts[4];
        BuildQuadVertices(q, half_width, q.half_length, verts);
        const int dest = cursor[st] * 4;
        scratch[dest]     = verts[0];
        scratch[dest + 1] = verts[1];
        scratch[dest + 2] = verts[2];
        scratch[dest + 3] = verts[3];
        ++cursor[st];
    }

    // Upload to VBO and draw each surface type.
    ws.vbo->Bind();
    ws.vbo->Fill(scratch.data(), total * 4, 0);
    ibo_->Bind();

    for (int t = 0; t < physics::kSurfaceTypeCount; ++t) {
        if (counts[t] == 0) continue;
        abstract::Texture* tex = textures_[t];
        if (tex) tex->Bind(0);
        video_->RenderIndexed(counts[t] * 6, offsets[t] * 6);
    }
}

}  // namespace track
