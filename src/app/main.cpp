// ClaudeEngine application entrypoint.
// Responsibilities (src/CLAUDE.md): load configuration, run the engine.

#include "abstract/PrimitiveType.h"
#include "abstract/VideoDevice.h"
#include "core/AppConfig.h"
#include "core/Camera.h"
#include "core/Color.h"
#include "core/Config.h"
#include "core/CoordinateSystem.h"
#include "core/EventManager.h"
#include "core/EventType.h"
#include "core/Key.h"
#include "core/Logger.h"
#include "core/Mat4f.h"
#include "core/ProjectionType.h"
#include "core/Vec3f.h"
#include "core/Vertex3D.h"
#include "gldevices/GLDevices.h"
#include "renderer/GeometryData.h"
#include "renderer/GlobalLight.h"
#include "renderer/Material.h"
#include "renderer/MaterialDesc.h"
#include "renderer/Mesh.h"
#include "renderer/MeshInstance.h"
#include "renderer/OmniLight.h"
#include "renderer/Renderer.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <random>
#include <vector>
#include <loguru.hpp>

namespace {

constexpr float kHalf        = 1.5f;    // cube half-side
constexpr float kWorldHalf   = 500.f;   // world extends [-500, 500]³
constexpr float kCameraSpeed = 10.f;    // units/s
constexpr float kMouseSens   = 0.002f;  // rad/px
constexpr float kPi          = 3.14159265f;

constexpr int kNumCubes      = 300;
constexpr int kNumSpheres    = 300;
constexpr int kNumOmniLights = 40;

// Builds a UV sphere GeometryData with normals, tangents, binormals and UVs.
// stacks: latitude bands (top to bottom); rings: longitude bands around Y.
std::unique_ptr<renderer::GeometryData> BuildSphere(
    abstract::VideoDevice* video, int stacks, int rings) {
  const int num_verts = (stacks + 1) * (rings + 1);
  const int num_tris  = stacks * rings * 2;

  std::vector<core::Vertex3D> verts;
  verts.reserve(num_verts);
  std::vector<uint16_t> idx;
  idx.reserve(num_tris * 3);

  for (int i = 0; i <= stacks; ++i) {
    const float phi     = kPi * static_cast<float>(i) /
                          static_cast<float>(stacks);
    const float sin_phi = std::sin(phi);
    const float cos_phi = std::cos(phi);

    for (int j = 0; j <= rings; ++j) {
      const float theta = 2.f * kPi * static_cast<float>(j) /
                          static_cast<float>(rings);
      const float sin_t = std::sin(theta);
      const float cos_t = std::cos(theta);

      // Unit sphere, Y-up; phi=0 at top (+Y), phi=pi at bottom (-Y).
      const core::Vec3f pos    = {sin_phi * cos_t, cos_phi, sin_phi * sin_t};
      const core::Vec3f normal = pos;  // outward normal for unit sphere

      // Tangent: d(pos)/d(theta) normalized — direction of increasing U.
      // d(pos)/d(theta) = (-sin_phi*sin_t, 0, sin_phi*cos_t); sin_phi cancels.
      const core::Vec3f tangent = (sin_phi > 1e-5f)
          ? core::Vec3f{-sin_t, 0.f, cos_t}
          : core::Vec3f{1.f,    0.f, 0.f};  // degenerate at poles

      // Binormal: d(pos)/d(phi) — direction of increasing V.
      // d(pos)/d(phi) = (cos_phi*cos_t, -sin_phi, cos_phi*sin_t).
      const core::Vec3f binormal =
          {cos_phi * cos_t, -sin_phi, cos_phi * sin_t};

      const float u = static_cast<float>(j) / static_cast<float>(rings);
      const float v = static_cast<float>(i) / static_cast<float>(stacks);
      verts.push_back({pos, normal, binormal, tangent, {u, v}});
    }
  }

  // Two CCW triangles per quad viewed from outside the sphere.
  for (int i = 0; i < stacks; ++i) {
    for (int j = 0; j < rings; ++j) {
      const auto k0 = static_cast<uint16_t>(i       * (rings + 1) + j);
      const auto k1 = static_cast<uint16_t>((i + 1) * (rings + 1) + j);
      idx.push_back(k0);     idx.push_back(k0 + 1); idx.push_back(k1);
      idx.push_back(k0 + 1); idx.push_back(k1 + 1); idx.push_back(k1);
    }
  }

  return std::make_unique<renderer::GeometryData>(
      video, num_verts, verts.data(), num_tris, idx.data());
}

}  // namespace

int main(int argc, char* argv[]) {
  core::Logger::Init(argc, argv);
  core::Config::Init(argc, argv);
  LOG_F(INFO, "ClaudeEngine starting up");
  LOG_F(INFO, "Data folder: %s", core::Config::GetDataFolder().c_str());
  core::AppConfig::Init(core::Config::GetDataFolder() / "config.yaml");
  new core::EventManager();

  // TODO(#2): Instantiate and run the Engine

  const core::GraphicsConfig& gfx = core::AppConfig::GetGraphics();
  gldevices::GLDevices devices(gfx.GetWidth(), gfx.GetHeight(), !gfx.IsWindowed());
  abstract::VideoDevice* video = devices.GetVideoDevice();

  video->SetDepthTestEnabled(true);
  video->SetPrimitiveType(abstract::PrimitiveType::kTriangleList);
  video->SetIndexType(abstract::IndexType::kUInt16);

  // ---- Materials ------------------------------------------------------------
  renderer::Material cube_material(
      renderer::MaterialDesc()
          .SetDiffuse("demo.png")
          .SetDiffuseColor(core::Color(1.f, 1.f, 1.f))
          .SetShininess(64.f),
      video);

  // Sphere: ambient_color > 0 so IsEmissive() == true → emissive pass.
  renderer::Material sphere_material(
      renderer::MaterialDesc()
          .SetDiffuse("demo.png")
          .SetEmissive("demo.png")
          .SetDiffuseColor(core::Color(0.8f, 0.9f, 1.f))
          .SetAmbientColor(core::Color(0.05f, 0.05f, 0.1f))
          .SetShininess(128.f)
          .SetEmissiveColor(core::Color(1.1f, 0.05f, 0.f)),
      video);

  // ---- Cube geometry --------------------------------------------------------
  // 24 unique vertices (4 per face × 6 faces), CCW winding viewed from outside.
  const float h = kHalf;
  const core::Vertex3D cube_verts[24] = {
    // +Z (front): normal (0,0,1), tangent (1,0,0), binormal (0,1,0)
    {{-h,+h,+h}, {0,0,1}, {0,1,0}, {1,0,0}, {0.f,1.f}},
    {{-h,-h,+h}, {0,0,1}, {0,1,0}, {1,0,0}, {0.f,0.f}},
    {{+h,-h,+h}, {0,0,1}, {0,1,0}, {1,0,0}, {1.f,0.f}},
    {{+h,+h,+h}, {0,0,1}, {0,1,0}, {1,0,0}, {1.f,1.f}},
    // -Z (back): normal (0,0,-1), tangent (-1,0,0), binormal (0,1,0)
    {{+h,+h,-h}, {0,0,-1}, {0,1,0}, {-1,0,0}, {0.f,1.f}},
    {{+h,-h,-h}, {0,0,-1}, {0,1,0}, {-1,0,0}, {0.f,0.f}},
    {{-h,-h,-h}, {0,0,-1}, {0,1,0}, {-1,0,0}, {1.f,0.f}},
    {{-h,+h,-h}, {0,0,-1}, {0,1,0}, {-1,0,0}, {1.f,1.f}},
    // +X (right): normal (1,0,0), tangent (0,0,-1), binormal (0,1,0)
    {{+h,+h,+h}, {1,0,0}, {0,1,0}, {0,0,-1}, {0.f,1.f}},
    {{+h,-h,+h}, {1,0,0}, {0,1,0}, {0,0,-1}, {0.f,0.f}},
    {{+h,-h,-h}, {1,0,0}, {0,1,0}, {0,0,-1}, {1.f,0.f}},
    {{+h,+h,-h}, {1,0,0}, {0,1,0}, {0,0,-1}, {1.f,1.f}},
    // -X (left): normal (-1,0,0), tangent (0,0,1), binormal (0,1,0)
    {{-h,+h,-h}, {-1,0,0}, {0,1,0}, {0,0,1}, {0.f,1.f}},
    {{-h,-h,-h}, {-1,0,0}, {0,1,0}, {0,0,1}, {0.f,0.f}},
    {{-h,-h,+h}, {-1,0,0}, {0,1,0}, {0,0,1}, {1.f,0.f}},
    {{-h,+h,+h}, {-1,0,0}, {0,1,0}, {0,0,1}, {1.f,1.f}},
    // +Y (top): normal (0,1,0), tangent (1,0,0), binormal (0,0,-1)
    {{-h,+h,+h}, {0,1,0}, {0,0,-1}, {1,0,0}, {0.f,1.f}},
    {{-h,+h,-h}, {0,1,0}, {0,0,-1}, {1,0,0}, {0.f,0.f}},
    {{+h,+h,-h}, {0,1,0}, {0,0,-1}, {1,0,0}, {1.f,0.f}},
    {{+h,+h,+h}, {0,1,0}, {0,0,-1}, {1,0,0}, {1.f,1.f}},
    // -Y (bottom): normal (0,-1,0), tangent (1,0,0), binormal (0,0,1)
    {{-h,-h,-h}, {0,-1,0}, {0,0,1}, {1,0,0}, {0.f,1.f}},
    {{-h,-h,+h}, {0,-1,0}, {0,0,1}, {1,0,0}, {0.f,0.f}},
    {{+h,-h,+h}, {0,-1,0}, {0,0,1}, {1,0,0}, {1.f,0.f}},
    {{+h,-h,-h}, {0,-1,0}, {0,0,1}, {1,0,0}, {1.f,1.f}},
  };
  const uint16_t cube_indices[36] = {
     0, 1, 2,  0, 2, 3,   // +Z
     4, 5, 6,  4, 6, 7,   // -Z
     8, 9,10,  8,10,11,   // +X
    12,13,14, 12,14,15,   // -X
    16,19,18, 16,18,17,   // +Y
    20,22,21, 20,23,22,   // -Y
  };
  renderer::GeometryData cube_geo(video, 24, cube_verts, 12, cube_indices);

  // ---- Sphere geometry (32 stacks × 64 rings, full TBN + UVs) ---------------
  auto sphere_geo = BuildSphere(video, 32, 64);

  new renderer::Renderer(video);
  renderer::Renderer::Instance().InitVisibilitySystems(1000.f);

  // ---- Meshes ---------------------------------------------------------------
  renderer::Mesh cube_mesh(&cube_geo, &cube_material);
  renderer::Mesh sphere_mesh(sphere_geo.get(), &sphere_material);

  // ---- Random scene population (seed=42 for reproducibility) ----------------
  std::mt19937 rng(42);
  std::uniform_real_distribution<float> pos_dist(-kWorldHalf, kWorldHalf);
  std::uniform_real_distribution<float> scale_dist(0.5f, 4.0f);
  std::uniform_real_distribution<float> color_dist(0.f, 1.f);
  std::uniform_real_distribution<float> radius_dist(20.f, 60.f);

  // 300 cube instances at random positions and uniform scales.
  std::vector<std::unique_ptr<renderer::MeshInstance>> cube_instances;
  cube_instances.reserve(kNumCubes);
  for (int i = 0; i < kNumCubes; ++i) {
    const float px = pos_dist(rng);
    const float py = pos_dist(rng);
    const float pz = pos_dist(rng);
    const float s  = scale_dist(rng);
    const core::Mat4f world = core::Mat4f::Translation({px, py, pz})
                            * core::Mat4f::Scale3D({s, s, s});
    cube_instances.push_back(
        std::make_unique<renderer::MeshInstance>(&cube_mesh, world, false));
  }

  // 300 sphere instances at random positions and uniform scales.
  std::vector<std::unique_ptr<renderer::MeshInstance>> sphere_instances;
  sphere_instances.reserve(kNumSpheres);
  for (int i = 0; i < kNumSpheres; ++i) {
    const float px = pos_dist(rng);
    const float py = pos_dist(rng);
    const float pz = pos_dist(rng);
    const float s  = scale_dist(rng);
    const core::Mat4f world = core::Mat4f::Translation({px, py, pz})
                            * core::Mat4f::Scale3D({s, s, s});
    sphere_instances.push_back(
        std::make_unique<renderer::MeshInstance>(&sphere_mesh, world, false));
  }

  // Warm sun-like directional light (always visible).
  renderer::GlobalLight global_light(
      core::Color(0.9f, 0.85f, 0.7f), 1.2f,
      core::Vec3f{0.05f, 0.05f, 0.08f},
      core::Vec3f{-1.f, -1.f, -0.5f}.Normalized());

  // 40 omni lights scattered through the world.
  std::vector<std::unique_ptr<renderer::OmniLight>> omni_lights;
  omni_lights.reserve(kNumOmniLights);
  for (int i = 0; i < kNumOmniLights; ++i) {
    const float px = pos_dist(rng);
    const float py = pos_dist(rng);
    const float pz = pos_dist(rng);
    const core::Color color(color_dist(rng), color_dist(rng), color_dist(rng));
    const float radius = radius_dist(rng);
    omni_lights.push_back(std::make_unique<renderer::OmniLight>(
        color, 2.f, radius, core::Mat4f::Translation({px, py, pz})));
  }

  // ---- Register with visibility systems ------------------------------------
  for (auto& inst  : cube_instances)   renderer::Renderer::Instance().AddRenderable(inst.get());
  for (auto& inst  : sphere_instances) renderer::Renderer::Instance().AddRenderable(inst.get());
  for (auto& light : omni_lights)      renderer::Renderer::Instance().AddRenderable(light.get());
  renderer::Renderer::Instance().AddRenderable(&global_light);

  // ---- Camera ---------------------------------------------------------------
  core::Camera camera(core::ProjectionType::kPerspective,
                      core::CoordinateSystem::kRightHanded);
  camera.SetPosition({0.f, 200.f, 500.f});
  camera.SetTarget({0.f, 0.f, 0.f});
  camera.SetMinDepth(1.f);
  camera.SetMaxDepth(1500.f);
  camera.SetFOV(1.0472f);  // 60 degrees
  camera.SetScreenCenter({gfx.GetWidth() * 0.5f, gfx.GetHeight() * 0.5f});

  // Camera orientation in spherical coords; yaw=0 → looking along -Z.
  float yaw = 0.f, pitch = 0.f;
  float prev_mx = -1.f, prev_my = -1.f;

  // Held-key flags
  bool k_fwd = false, k_bwd  = false;
  bool k_lft = false, k_rgt  = false;
  bool k_up  = false, k_down = false;

  // G-buffer debug mode: keys 0/Esc = full pipeline, 1-4 = channels.
  renderer::DebugMode debug_mode = renderer::DebugMode::kNone;

  // ---- Timing ---------------------------------------------------------------
  auto  t_prev       = std::chrono::steady_clock::now();
  float elapsed_time = 0.f;

  bool running = true;
  while (running) {
    const auto  t_now = std::chrono::steady_clock::now();
    const float dt    = std::chrono::duration<float>(t_now - t_prev).count();
    t_prev = t_now;

    devices.Update();

    // ---- Events -------------------------------------------------------------
    while (core::EventManager::Instance().HasEvents()) {
      core::Event e = core::EventManager::Instance().Consume();
      switch (e.type) {
        case core::EventType::kWindowClose:
          LOG_F(INFO, "Event: WindowClose");
          running = false;
          break;
        case core::EventType::kKeyDown:
          if (e.key == core::Key::kUp)    k_fwd  = true;
          if (e.key == core::Key::kDown)  k_bwd  = true;
          if (e.key == core::Key::kLeft)  k_lft  = true;
          if (e.key == core::Key::kRight) k_rgt  = true;
          if (e.key == core::Key::kA)     k_up   = true;
          if (e.key == core::Key::kZ)     k_down = true;
          if (e.key == core::Key::kEscape || e.key == core::Key::k0)
            debug_mode = renderer::DebugMode::kNone;
          if (e.key == core::Key::k1) debug_mode = renderer::DebugMode::kAlbedo;
          if (e.key == core::Key::k2) debug_mode = renderer::DebugMode::kNormal;
          if (e.key == core::Key::k3) debug_mode = renderer::DebugMode::kSpecular;
          if (e.key == core::Key::k4) debug_mode = renderer::DebugMode::kDepth;
          break;
        case core::EventType::kKeyUp:
          if (e.key == core::Key::kUp)    k_fwd  = false;
          if (e.key == core::Key::kDown)  k_bwd  = false;
          if (e.key == core::Key::kLeft)  k_lft  = false;
          if (e.key == core::Key::kRight) k_rgt  = false;
          if (e.key == core::Key::kA)     k_up   = false;
          if (e.key == core::Key::kZ)     k_down = false;
          break;
        case core::EventType::kMouseMoved:
          if (prev_mx >= 0.f) {
            yaw   += (e.mouse_x - prev_mx) * kMouseSens;
            pitch += (e.mouse_y - prev_my) * kMouseSens;
            pitch  = std::max(-1.5f, std::min(1.5f, pitch));
          }
          prev_mx = e.mouse_x;
          prev_my = e.mouse_y;
          break;
        default:
          break;
      }
    }

    // ---- Camera movement ----------------------------------------------------
    const core::Vec3f look = {
         std::sin(yaw) * std::cos(pitch),
        -std::sin(pitch),
        -std::cos(yaw) * std::cos(pitch)
    };
    const core::Vec3f right = core::Vec3f::kAxisY.Cross(look).Normalized();

    const float spd = kCameraSpeed * dt;
    core::Vec3f pos = camera.GetPosition();
    if (k_fwd)  pos += look                * spd;
    if (k_bwd)  pos -= look                * spd;
    if (k_rgt)  pos += right               * spd;
    if (k_lft)  pos -= right               * spd;
    if (k_up)   pos += core::Vec3f::kAxisY * spd;
    if (k_down) pos -= core::Vec3f::kAxisY * spd;

    camera.SetPosition(pos);
    camera.SetTarget(pos + look);
    camera.UpdateMatrices();

    // ---- Render -------------------------------------------------------------
    elapsed_time += dt;

    video->BeginFrame();
    video->ClearRenderTargets(core::Color::kBlack);

    renderer::Renderer::Instance().SetDebugMode(debug_mode);
    renderer::Renderer::Instance().Update(elapsed_time, &camera);
  }

  renderer::Renderer::Shutdown();
  core::EventManager::Shutdown();

  LOG_F(INFO, "ClaudeEngine shutting down");
  core::Logger::Shutdown();
  return 0;
}
