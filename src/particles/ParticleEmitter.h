#pragma once

#include <memory>
#include <random>
#include <vector>

#include "abstract/VertexBuffer.h"
#include "abstract/VideoDevice.h"
#include "core/Mat4f.h"
#include "core/Vec3f.h"
#include "core/VertexParticle.h"
#include "particles/ParticleSubSystemDesc.h"

namespace particles {

// CPU particle simulation and GPU billboard upload for one sub-system.
//
// One instance is created per ParticleSubSystemDesc at runtime. The descriptor
// must outlive this emitter.
//
// Typical per-frame usage:
//   emitter.Update(dt);
//   emitter.UploadToGPU();
//   // bind VBO, issue draw call for GetParticleCount() * 4 vertices
class ParticleEmitter {
 public:
  // desc must outlive this emitter; video is used to create the dynamic VBO.
  ParticleEmitter(const ParticleSubSystemDesc& desc, abstract::VideoDevice* video);

  // Advances simulation by dt seconds: spawns new particles, integrates
  // positions/velocities, interpolates size/color, and expires dead particles.
  void Update(float dt);

  // Uploads current particle billboard vertices to the GPU VBO.
  // Call once per frame after Update(), before issuing the draw call.
  void UploadToGPU();

  // Sets the world-space origin used for particle spawn positions.
  void SetWorldTransform(const core::Mat4f& transform);

  [[nodiscard]] const ParticleSubSystemDesc& GetDesc()          const;
  [[nodiscard]] abstract::VertexBuffer*      GetVBO()           const;
  // Number of live particles; index count for the draw call = GetParticleCount() * 6.
  [[nodiscard]] int                          GetParticleCount() const;

 private:
  struct Particle {
    // cppcheck-suppress unusedStructMember
    core::Vec3f position;
    // cppcheck-suppress unusedStructMember
    core::Vec3f velocity;
    // cppcheck-suppress unusedStructMember
    float       age;
    // cppcheck-suppress unusedStructMember
    float       lifetime;
    // cppcheck-suppress unusedStructMember
    float       size;
    // cppcheck-suppress unusedStructMember
    core::Color color;
    // cppcheck-suppress unusedStructMember
    int         frame;
    // cppcheck-suppress unusedStructMember
    float       size_start;
    // cppcheck-suppress unusedStructMember
    float       size_end;
  };

  void SpawnParticle();

  [[nodiscard]] core::Vec3f RandomSpawnOffset();
  [[nodiscard]] core::Vec3f RandomDirection();

  // cppcheck-suppress unusedStructMember
  const ParticleSubSystemDesc&              desc_;
  // cppcheck-suppress unusedStructMember
  core::Vec3f                               origin_{};
  std::unique_ptr<abstract::VertexBuffer>   vbo_;
  // cppcheck-suppress unusedStructMember
  std::vector<Particle>                     particles_;
  // cppcheck-suppress unusedStructMember
  std::vector<core::VertexParticle>         vbo_vertices_;
  int                                       live_count_  = 0;
  float                                     spawn_accum_ = 0.f;
  float                                     elapsed_     = 0.f;
  bool                                      done_        = false;
  std::mt19937                              rng_;
};

}  // namespace particles
