#include "particles/ParticleEmitter.h"

#include <algorithm>
#include <cmath>
#include <random>

#include "abstract/BufferUsage.h"
#include "core/VertexType.h"
#include "particles/EmitterShape.h"
#include "particles/ParticleAnimationMode.h"

namespace particles {

namespace {

constexpr float kPi        = 3.14159265358979323846f;
constexpr float kTwoPi     = 2.f * kPi;
constexpr float kDegToRad  = kPi / 180.f;

// Sample the colour gradient at normalised age t ∈ [0, 1].
// Falls back to white-opaque → white-transparent when the gradient is empty.
core::Color SampleGradient(const ParticleSubSystemDesc& desc, float t) {
  const int count = desc.color_gradient_count;
  if (count <= 0) {
    // Legacy two-stop fallback: white-opaque → white-transparent.
    static const core::Color kDefault0{1.f, 1.f, 1.f, 1.f};
    static const core::Color kDefault1{1.f, 1.f, 1.f, 0.f};
    return kDefault0.Lerp(kDefault1, t);
  }
  if (count == 1) return desc.color_gradient[0].color;

  if (t <= desc.color_gradient[0].key) return desc.color_gradient[0].color;

  const ColorStop& last = desc.color_gradient[count - 1];
  if (t >= last.key) return last.color;

  for (int i = 0; i < count - 1; ++i) {
    const ColorStop& a = desc.color_gradient[i];
    const ColorStop& b = desc.color_gradient[i + 1];
    if (t <= b.key) {
      const float span = b.key - a.key;
      const float local_t = (span > 1e-6f) ? (t - a.key) / span : 0.f;
      return a.color.Lerp(b.color, local_t);
    }
  }
  return last.color;
}

}  // namespace

ParticleEmitter::ParticleEmitter(const ParticleSubSystemDesc& desc,
                                 abstract::VideoDevice* video)
    : desc_(desc),
      rng_(std::random_device{}()) {
  particles_.resize(desc_.max_particles);
  vbo_vertices_.resize(desc_.max_particles * 4);
  vbo_ = video->CreateVertexBuffer(
      core::VertexType::kParticle,
      desc_.max_particles * 4,
      abstract::BufferUsage::kDynamic);
  if (!desc_.texture.empty())
    texture_ = video->CreateTexture(desc_.texture);
}

ParticleEmitter::~ParticleEmitter() {
  if (texture_) texture_->Release();
}

void ParticleEmitter::Update(float dt) {
  elapsed_ += dt;

  // Spawn phase — stopped after duration for one-shot emitters, or when disabled.
  bool emitting = desc_.enabled && (desc_.looping || elapsed_ <= desc_.duration);
  if (emitting) {
    spawn_accum_ += desc_.emission_rate * dt;
    int n = static_cast<int>(spawn_accum_);
    spawn_accum_ -= static_cast<float>(n);
    for (int i = 0; i < n; ++i) SpawnParticle();
  }

  // Integration and interpolation.
  const core::Vec3f gravity_vec{0.f, -desc_.gravity, 0.f};
  const float half_dt2      = 0.5f * dt * dt;
  const int   frame_count   = desc_.sprite_cols * desc_.sprite_rows;
  const float drag_factor   = 1.f - std::min(desc_.drag * dt, 1.f);

  for (int i = 0; i < live_count_; ++i) {
    Particle& p = particles_[i];
    p.age += dt;

    // Velocity drag.
    if (desc_.drag > 0.f)
      p.velocity = p.velocity * drag_factor;

    // Lateral turbulence: sinusoidal XZ perturbation based on particle age.
    if (desc_.turbulence_strength > 0.f) {
      const float phase = desc_.turbulence_frequency * kTwoPi * p.age + p.turb_phase;
      p.position.x += std::sin(phase) * desc_.turbulence_strength * dt;
      p.position.z += std::cos(phase) * desc_.turbulence_strength * dt;
    }

    p.position += p.velocity * dt + gravity_vec * half_dt2;

    // Per-particle rotation.
    p.angle += p.angular_vel * dt;

    const float t = p.age / p.lifetime;
    p.size  = p.size_start + (p.size_end - p.size_start) * t;
    p.color = SampleGradient(desc_, t);

    if (desc_.animation_mode == ParticleAnimationMode::kSequential &&
        frame_count > 1 && desc_.animation_fps > 0.f) {
      p.frame = static_cast<int>(p.age * desc_.animation_fps) % frame_count;
    }
  }

  // Expire dead particles using swap-with-last to keep the live prefix packed.
  for (int i = live_count_ - 1; i >= 0; --i) {
    if (particles_[i].age >= particles_[i].lifetime) {
      particles_[i] = particles_[--live_count_];
    }
  }

  // One-shot: mark done when all particles have died and emission has stopped.
  if (!desc_.looping && elapsed_ > desc_.duration && live_count_ == 0) {
    done_ = true;
  }
}

void ParticleEmitter::UploadToGPU() {
  const int frame_count = desc_.sprite_cols * desc_.sprite_rows;

  for (int i = 0; i < live_count_; ++i) {
    const Particle& p = particles_[i];

    int frame = 0;
    if (frame_count > 0) {
      frame = p.frame % frame_count;
    }
    const int col = (desc_.sprite_cols > 0) ? frame % desc_.sprite_cols : 0;
    const int row = (desc_.sprite_cols > 0) ? frame / desc_.sprite_cols : 0;
    // Rows are counted top-to-bottom in the image but the texture is loaded with
    // stbi_set_flip_vertically_on_load(1), so V=0 is the image bottom. Invert the
    // row index so that sprite row 0 maps to the top of the texture (high V).
    const int rows = std::max(1, desc_.sprite_rows);
    const core::Vec2f uv_offset{
        static_cast<float>(col) / static_cast<float>(std::max(1, desc_.sprite_cols)),
        static_cast<float>(rows - 1 - row) / static_cast<float>(rows)};

    const core::VertexParticle v{p.position, p.size, p.color, uv_offset, p.angle};
    const int base = i * 4;
    vbo_vertices_[base + 0] = v;
    vbo_vertices_[base + 1] = v;
    vbo_vertices_[base + 2] = v;
    vbo_vertices_[base + 3] = v;
  }

  if (live_count_ > 0) {
    vbo_->Fill(vbo_vertices_.data(), live_count_ * 4);
  }
}

void ParticleEmitter::SetWorldTransform(const core::Mat4f& transform) {
  // Extract the translation column from the row-major matrix.
  origin_ = core::Vec3f(transform(0, 3), transform(1, 3), transform(2, 3));
}

const ParticleSubSystemDesc& ParticleEmitter::GetDesc()    const { return desc_; }
abstract::Texture*           ParticleEmitter::GetTexture() const { return texture_; }
abstract::VertexBuffer*      ParticleEmitter::GetVBO()     const { return vbo_.get(); }
int                          ParticleEmitter::GetParticleCount() const { return live_count_; }

// ---------------------------------------------------------------------------

void ParticleEmitter::SpawnParticle() {
  if (live_count_ >= static_cast<int>(particles_.size())) return;

  std::uniform_real_distribution<float> u01(0.f, 1.f);

  Particle& p = particles_[live_count_++];
  p.age      = 0.f;
  p.lifetime = desc_.lifetime_min +
               u01(rng_) * (desc_.lifetime_max - desc_.lifetime_min);

  const float speed = desc_.speed_min +
                      u01(rng_) * (desc_.speed_max - desc_.speed_min);
  p.velocity = RandomDirection() * speed;
  p.position = origin_ + RandomSpawnOffset();

  p.size_start = desc_.size_start_min +
                 u01(rng_) * (desc_.size_start_max - desc_.size_start_min);
  p.size_end   = desc_.size_end_min +
                 u01(rng_) * (desc_.size_end_max - desc_.size_end_min);
  p.size  = p.size_start;
  p.color = SampleGradient(desc_, 0.f);

  // Per-particle rotation.
  p.angle = (desc_.rotation_start_min +
             u01(rng_) * (desc_.rotation_start_max - desc_.rotation_start_min))
            * kDegToRad;
  p.angular_vel = (desc_.angular_velocity_min +
                   u01(rng_) * (desc_.angular_velocity_max - desc_.angular_velocity_min))
                  * kDegToRad;

  // Random turbulence phase so adjacent particles don't oscillate in sync.
  p.turb_phase = u01(rng_) * kTwoPi;

  const int frame_count = desc_.sprite_cols * desc_.sprite_rows;
  if (desc_.animation_mode == ParticleAnimationMode::kRandom && frame_count > 0) {
    std::uniform_int_distribution<int> u_frame(0, frame_count - 1);
    p.frame = u_frame(rng_);
  } else {
    p.frame = 0;
  }
}

core::Vec3f ParticleEmitter::RandomSpawnOffset() {
  const float r = desc_.emitter_radius;
  if (r <= 0.f) return core::Vec3f::kZero;

  std::uniform_real_distribution<float> u01(0.f, 1.f);

  switch (desc_.emitter_shape) {
    case EmitterShape::kPoint:
      return core::Vec3f::kZero;

    case EmitterShape::kSphere: {
      // Uniform point on sphere surface via rejection sampling.
      std::uniform_real_distribution<float> u11(-1.f, 1.f);
      core::Vec3f v;
      float len;
      do {
        v = {u11(rng_), u11(rng_), u11(rng_)};
        len = v.Length();
      } while (len < 1e-6f);
      return v * (r / len);
    }

    case EmitterShape::kBox: {
      std::uniform_real_distribution<float> u11(-r, r);
      return {u11(rng_), u11(rng_), u11(rng_)};
    }

    case EmitterShape::kCone: {
      // Uniform point in a disc perpendicular to the emission direction.
      const core::Vec3f dir = desc_.direction.Normalized();
      core::Vec3f perp;
      if (std::fabs(dir.x) < 0.9f)
        perp = dir.Cross(core::Vec3f::kAxisX).Normalized();
      else
        perp = dir.Cross(core::Vec3f::kAxisY).Normalized();
      const core::Vec3f perp2 = dir.Cross(perp).Normalized();

      const float angle = u01(rng_) * kTwoPi;
      const float rad   = std::sqrt(u01(rng_)) * r;
      return perp * (rad * std::cos(angle)) + perp2 * (rad * std::sin(angle));
    }
  }
  return core::Vec3f::kZero;
}

core::Vec3f ParticleEmitter::RandomDirection() {
  const float spread_rad  = desc_.spread * kDegToRad;
  const float cos_spread  = std::cos(spread_rad);

  std::uniform_real_distribution<float> u01(0.f, 1.f);

  // Uniform sampling on a spherical cap (Archimedes' hat-box theorem).
  const float cos_theta = 1.f - u01(rng_) * (1.f - cos_spread);
  const float sin_theta = std::sqrt(std::max(0.f, 1.f - cos_theta * cos_theta));
  const float phi       = u01(rng_) * kTwoPi;

  // Local direction around the canonical Y axis.
  const core::Vec3f local{sin_theta * std::cos(phi), cos_theta,
                           sin_theta * std::sin(phi)};

  // Rotate local direction to align canonical Y with desc_.direction.
  const core::Vec3f n   = desc_.direction.Normalized();
  const float       ny  = n.y;
  // Rotation axis = (0,1,0) × n = (nz, 0, -nx).
  const core::Vec3f k{n.z, 0.f, -n.x};
  const float       k_len = k.Length();

  if (k_len < 1e-6f) {
    // n is (nearly) parallel or anti-parallel to Y.
    return (ny > 0.f) ? local : core::Vec3f{local.x, -local.y, local.z};
  }

  const core::Vec3f kn     = k * (1.f / k_len);
  const float       cos_a  = ny;
  const float       sin_a  = k_len;
  const float       k_dot  = kn.Dot(local);
  return local * cos_a + kn.Cross(local) * sin_a + kn * (k_dot * (1.f - cos_a));
}

}  // namespace particles
