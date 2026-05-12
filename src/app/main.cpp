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
#include "gldevices/GLDevices.h"
#include "renderer/GlobalLight.h"
#include "renderer/MaterialDesc.h"
#include "renderer/MeshLoader.h"
#include "renderer/OmniLight.h"
#include "renderer/RenderableMesh.h"
#include "renderer/RenderableMeshInstance.h"
#include "renderer/Renderer.h"

#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <loguru.hpp>

namespace {

constexpr float kCameraSpeed = 10.f;   // units/s
constexpr float kMouseSens   = 0.002f;  // rad/px

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

  new renderer::Renderer(video);
  renderer::Renderer::Instance().InitVisibilitySystems(100.f);

  // ---- Demo meshes ----------------------------------------------------------
  // Load demo.obj (orange) and demo.fbx (blue) from the data folder and place
  // them side by side near the origin.
  const std::string data_dir = core::Config::GetDataFolder().string();

  auto obj_mesh = renderer::MeshLoader::Load(
      data_dir + "/meshes/demo.obj", video,
      renderer::MaterialDesc()
          .SetDiffuse("demo.png")
          .SetDiffuseColor(core::Color(1.f, 0.5f, 0.f))
          .SetShininess(32.f));

  auto fbx_mesh = renderer::MeshLoader::Load(
      data_dir + "/meshes/demo.fbx", video,
      renderer::MaterialDesc()
          .SetDiffuse("demo.png")
          .SetDiffuseColor(core::Color(0.2f, 0.6f, 1.f))
          .SetShininess(64.f));

  const core::Mat4f obj_world =
      core::Mat4f::Translation({-10.f, 0.f, 0.f}) * core::Mat4f::Scale3D({3.f, 3.f, 3.f});
  const core::Mat4f fbx_world =
      core::Mat4f::Translation({ 10.f, 0.f, 0.f}) * core::Mat4f::Scale3D({3.f, 3.f, 3.f});

  std::unique_ptr<renderer::RenderableMeshInstance> obj_inst;
  std::unique_ptr<renderer::RenderableMeshInstance> fbx_inst;

  if (obj_mesh) {
    obj_inst = std::make_unique<renderer::RenderableMeshInstance>(
        obj_mesh.get(), obj_world, false);
    renderer::Renderer::Instance().AddRenderable(obj_inst.get());
  }
  if (fbx_mesh) {
    fbx_inst = std::make_unique<renderer::RenderableMeshInstance>(
        fbx_mesh.get(), fbx_world, false);
    renderer::Renderer::Instance().AddRenderable(fbx_inst.get());
  }

  // ---- Lights ---------------------------------------------------------------
  // Warm directional sun from upper-left.
  renderer::GlobalLight global_light(
      core::Color(0.9f, 0.85f, 0.7f), 1.2f,
      core::Vec3f{0.05f, 0.05f, 0.08f},
      core::Vec3f{-1.f, -1.f, -0.5f}.Normalized());

  // Accent omni lights: warm near the OBJ mesh, cool near the FBX mesh,
  // and a neutral fill above the scene centre.
  renderer::OmniLight light_obj(
      core::Color(1.f, 0.6f, 0.2f), 2.f, 40.f,
      core::Mat4f::Translation({-10.f, 8.f, 5.f}));
  renderer::OmniLight light_fbx(
      core::Color(0.3f, 0.6f, 1.f), 2.f, 40.f,
      core::Mat4f::Translation({ 10.f, 8.f, 5.f}));
  renderer::OmniLight light_fill(
      core::Color(0.9f, 0.9f, 1.f), 1.f, 60.f,
      core::Mat4f::Translation({  0.f, 20.f, 10.f}));

  renderer::Renderer::Instance().AddRenderable(&global_light);
  renderer::Renderer::Instance().AddRenderable(&light_obj);
  renderer::Renderer::Instance().AddRenderable(&light_fbx);
  renderer::Renderer::Instance().AddRenderable(&light_fill);

  // ---- Camera ---------------------------------------------------------------
  core::Camera camera(core::ProjectionType::kPerspective,
                      core::CoordinateSystem::kRightHanded);
  camera.SetPosition({0.f, 5.f, 30.f});
  camera.SetTarget({0.f, 0.f, 0.f});
  camera.SetMinDepth(0.1f);
  camera.SetMaxDepth(200.f);
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
