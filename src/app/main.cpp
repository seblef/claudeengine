// ClaudeEngine application entrypoint.
// Responsibilities (src/CLAUDE.md): load configuration, run the engine.

#include "abstract/PrimitiveType.h"
#include "abstract/VideoDevice.h"
#include "core/AppConfig.h"
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
#include "game/FPSCameraController.h"
#include "game/GameCamera.h"
#include "game/GameLight.h"
#include "game/GameLightDesc.h"
#include "game/GameMesh.h"
#include "game/GameSystem.h"
#include "game/MeshTemplate.h"
#include "gldevices/GLDevices.h"
#include "renderer/GeometryUtils.h"
#include "renderer/Material.h"
#include "renderer/MaterialDesc.h"
#include "renderer/Renderer.h"

#include <memory>
#include <string>
#include <loguru.hpp>

int main(int argc, char* argv[]) {
  core::Logger::Init(argc, argv);
  core::Config::Init(argc, argv);
  LOG_F(INFO, "ClaudeEngine starting up");
  LOG_F(INFO, "Data folder: %s", core::Config::GetDataFolder().c_str());
  core::AppConfig::Init(core::Config::GetDataFolder() / "config.yaml");
  new core::EventManager();

  const core::GraphicsConfig& gfx = core::AppConfig::GetGraphics();
  gldevices::GLDevices devices(gfx.GetWidth(), gfx.GetHeight(), !gfx.IsWindowed());
  abstract::VideoDevice* video = devices.GetVideoDevice();

  video->SetDepthTestEnabled(true);
  video->SetPrimitiveType(abstract::PrimitiveType::kTriangleList);
  video->SetIndexType(abstract::IndexType::kUInt16);

  new renderer::Renderer(video);
  renderer::Renderer::Instance().InitVisibilitySystems(200.f);

  new game::GameSystem(&devices);
  game::GameSystem& game = game::GameSystem::Instance();

  // ---- Camera ---------------------------------------------------------------
  game::GameCamera camera(core::ProjectionType::kPerspective,
                          core::CoordinateSystem::kRightHanded);
  camera.SetMaxDepth(200.f);
  camera.SetScreenCenter({gfx.GetWidth() * 0.5f, gfx.GetHeight() * 0.5f});
  game.SetCamera(&camera);

  game::FPSCameraController controller;
  controller.SetPosition({0.f, 5.f, 30.f});
  game.SetCameraController(&controller);

  // ---- Demo meshes ----------------------------------------------------------
  const std::string data_dir = core::Config::GetDataFolder().string();

  game::MeshTemplate* obj_tmpl = game::MeshTemplate::GetOrLoad(
      data_dir + "/meshes/demo.obj", video);
  game::MeshTemplate* fbx_tmpl = game::MeshTemplate::GetOrLoad(
      data_dir + "/meshes/demo.fbx", video);

  std::unique_ptr<game::GameMesh> obj_mesh;
  std::unique_ptr<game::GameMesh> fbx_mesh;

  if (obj_tmpl && obj_tmpl->GetMesh()) {
    obj_mesh = std::make_unique<game::GameMesh>(obj_tmpl);
    obj_mesh->SetWorldTransform(
        core::Mat4f::Translation({-10.f, 3.f, 0.f}) *
        core::Mat4f::Scale3D({3.f, 3.f, 3.f}));
    game.AddObject(obj_mesh.get());
  }
  if (fbx_tmpl && fbx_tmpl->GetMesh()) {
    fbx_mesh = std::make_unique<game::GameMesh>(fbx_tmpl);
    fbx_mesh->SetWorldTransform(
        core::Mat4f::Translation({10.f, 3.f, 0.f}) *
        core::Mat4f::Scale3D({3.f, 3.f, 3.f}));
    game.AddObject(fbx_mesh.get());
  }

  if (obj_tmpl) obj_tmpl->Release();
  if (fbx_tmpl) fbx_tmpl->Release();

  // ---- Floor plane ----------------------------------------------------------
  auto plane_mat = std::make_unique<renderer::Material>(
      renderer::MaterialDesc().SetDiffuseColor({0.55f, 0.55f, 0.55f}), video);
  auto* plane_tmpl = new game::MeshTemplate(
      renderer::CreatePlaneMesh(video, 120.f), std::move(plane_mat));
  auto floor = std::make_unique<game::GameMesh>(plane_tmpl);
  floor->SetWorldTransform(core::Mat4f::kIdentity);
  game.AddObject(floor.get());
  plane_tmpl->Release();

  // ---- Lights ---------------------------------------------------------------
  game::GameLightDesc global_desc;
  global_desc.color         = core::Color(0.9f, 0.85f, 0.7f);
  global_desc.intensity     = 1.2f;
  global_desc.ambient_color = core::Vec3f(0.05f, 0.05f, 0.08f);
  global_desc.direction     = core::Vec3f(-0.4f, -0.8f, -0.3f).Normalized();
  game::GameLight global_light(renderer::LightType::kGlobal, global_desc);
  game.AddObject(&global_light);

  game::GameLightDesc spot_desc;
  spot_desc.color       = core::Color(1.f, 0.9f, 0.8f);
  spot_desc.intensity   = 3.f;
  spot_desc.direction   = core::Vec3f(0.f, -1.f, -0.3f).Normalized();
  spot_desc.outer_angle = 0.4f;
  spot_desc.range       = 50.f;
  game::GameLight spot_light(renderer::LightType::kCircleSpot, spot_desc);
  spot_light.SetWorldTransform(core::Mat4f::Translation({10.f, 20.f, 5.f}));
  game.AddObject(&spot_light);

  game::GameLightDesc omni_obj_desc;
  omni_obj_desc.color     = core::Color(1.f, 0.6f, 0.2f);
  omni_obj_desc.intensity = 2.f;
  omni_obj_desc.radius    = 40.f;
  game::GameLight light_obj(renderer::LightType::kOmni, omni_obj_desc);
  light_obj.SetWorldTransform(core::Mat4f::Translation({-10.f, 8.f, 5.f}));
  game.AddObject(&light_obj);

  game::GameLightDesc omni_fbx_desc;
  omni_fbx_desc.color     = core::Color(0.3f, 0.6f, 1.f);
  omni_fbx_desc.intensity = 2.f;
  omni_fbx_desc.radius    = 40.f;
  game::GameLight light_fbx(renderer::LightType::kOmni, omni_fbx_desc);
  light_fbx.SetWorldTransform(core::Mat4f::Translation({10.f, 8.f, 5.f}));
  game.AddObject(&light_fbx);

  game::GameLightDesc omni_fill_desc;
  omni_fill_desc.color     = core::Color(0.9f, 0.9f, 1.f);
  omni_fill_desc.intensity = 1.f;
  omni_fill_desc.radius    = 60.f;
  game::GameLight light_fill(renderer::LightType::kOmni, omni_fill_desc);
  light_fill.SetWorldTransform(core::Mat4f::Translation({0.f, 20.f, 10.f}));
  game.AddObject(&light_fill);

  // ---- Debug mode -----------------------------------------------------------
  // Keys 0/Esc = full pipeline, 1-4 = G-buffer channels.
  // Tab = cycle shadow-map debug overlay (one entry per light with shadows).
  renderer::DebugMode debug_mode = renderer::DebugMode::kNone;
  game.SetEventCallback([&debug_mode](const core::Event& e) {
    if (e.type != core::EventType::kKeyDown) return;
    if (e.key == core::Key::kEscape || e.key == core::Key::k0)
      debug_mode = renderer::DebugMode::kNone;
    if (e.key == core::Key::k1) debug_mode = renderer::DebugMode::kAlbedo;
    if (e.key == core::Key::k2) debug_mode = renderer::DebugMode::kNormal;
    if (e.key == core::Key::k3) debug_mode = renderer::DebugMode::kSpecular;
    if (e.key == core::Key::k4) debug_mode = renderer::DebugMode::kDepth;
    if (e.key == core::Key::kTab)
      renderer::Renderer::Instance().CycleShadowDebug();
  });

  // ---- Main loop ------------------------------------------------------------
  while (game.IsRunning()) {
    video->BeginFrame();
    video->ClearRenderTargets(core::Color::kBlack);
    renderer::Renderer::Instance().SetDebugMode(debug_mode);
    game.Update();
  }

  game::GameSystem::Shutdown();
  renderer::Renderer::Shutdown();
  core::EventManager::Shutdown();

  LOG_F(INFO, "ClaudeEngine shutting down");
  core::Logger::Shutdown();
  return 0;
}
