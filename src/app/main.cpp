// ClaudeEngine application entrypoint.
// Responsibilities (src/CLAUDE.md): load configuration, run the engine.

#include "abstract/PrimitiveType.h"
#include "abstract/Shader.h"
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
#include "renderer/Material.h"
#include "renderer/Renderer.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <loguru.hpp>

namespace {

constexpr float kHalf        = 5.f;     // cube half-side
constexpr float kCameraSpeed = 20.f;    // units/s
constexpr float kMouseSens   = 0.002f;  // rad/px
constexpr float kRotSpeed    = 20.f * (3.14159265f / 180.f);  // 20°/s in rad/s

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

  abstract::Shader* shader = video->CreateShader("textured_mesh");
  renderer::Material material("demo.yaml", video);

  // ---- Cube geometry ---------------------------------------------------------
  // 24 unique vertices (4 per face × 6 faces), CCW winding viewed from outside.
  const float h = kHalf;
  const core::Vertex3D verts[24] = {
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

  // 6 faces × 2 triangles × 3 vertices = 36 indices
  const uint16_t indices[36] = {
     0, 1, 2,  0, 2, 3,   // +Z
     4, 5, 6,  4, 6, 7,   // -Z
     8, 9,10,  8,10,11,   // +X
    12,13,14, 12,14,15,   // -X
    16,17,18, 16,18,19,   // +Y
    20,21,22, 20,22,23,   // -Y
  };

  renderer::GeometryData geo(video, 24, verts, 12, indices);

  new renderer::Renderer(video);

  // ---- Camera ---------------------------------------------------------------
  core::Camera camera(core::ProjectionType::kPerspective,
                      core::CoordinateSystem::kRightHanded);
  camera.SetPosition({0.f, 0.f, 50.f});
  camera.SetTarget({0.f, 0.f, 0.f});
  camera.SetMinDepth(0.1f);
  camera.SetMaxDepth(1000.f);
  camera.SetFOV(1.0472f);  // 60 degrees
  camera.SetScreenCenter({gfx.GetWidth() * 0.5f, gfx.GetHeight() * 0.5f});

  // Camera orientation in spherical coords; yaw=0 → looking along -Z.
  float yaw = 0.f, pitch = 0.f;
  float prev_mx = -1.f, prev_my = -1.f;

  // Held-key flags
  bool k_fwd = false, k_bwd  = false;
  bool k_lft = false, k_rgt  = false;
  bool k_up  = false, k_down = false;

  // ---- Timing ---------------------------------------------------------------
  auto  t_prev       = std::chrono::steady_clock::now();
  float cube_angle   = 0.f;
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
    // Spherical look direction derived from yaw/pitch.
    const core::Vec3f look = {
         std::sin(yaw) * std::cos(pitch),
        -std::sin(pitch),
        -std::cos(yaw) * std::cos(pitch)
    };
    const core::Vec3f right = core::Vec3f::kAxisY.Cross(look).Normalized();

    const float spd = kCameraSpeed * dt;
    core::Vec3f pos = camera.GetPosition();
    if (k_fwd)  pos += look               * spd;
    if (k_bwd)  pos -= look               * spd;
    if (k_rgt)  pos += right              * spd;
    if (k_lft)  pos -= right              * spd;
    if (k_up)   pos += core::Vec3f::kAxisY * spd;
    if (k_down) pos -= core::Vec3f::kAxisY * spd;

    camera.SetPosition(pos);
    camera.SetTarget(pos + look);
    camera.UpdateMatrices();

    // ---- Cube rotation ------------------------------------------------------
    cube_angle   += kRotSpeed * dt;
    elapsed_time += dt;
    const core::Mat4f world = core::Mat4f::RotationY(cube_angle);

    // ---- Render -------------------------------------------------------------
    video->BeginFrame();
    video->ClearRenderTargets(core::Color::kBlack);

    renderer::Renderer::Instance().Update(elapsed_time, &camera);
    renderer::Renderer::Instance().SetRenderableInfos(world);

    if (shader) shader->Activate();
    material.Set();
    geo.Set();
    video->RenderIndexed(geo.GetNumIndices());
  }

  if (shader) shader->Release();
  renderer::Renderer::Shutdown();
  core::EventManager::Shutdown();

  LOG_F(INFO, "ClaudeEngine shutting down");
  core::Logger::Shutdown();
  return 0;
}
