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
#include "environment/CloudRenderer.h"
#include "environment/SkyRenderer.h"
#include "environment/WaterRenderer.h"
#include "environment/WindSystem.h"
#include "environment/WorldTime.h"
#include "game/EnvironmentLighting.h"
#include "game/FPSCameraController.h"
#include "game/GameCamera.h"
#include "game/GameLight.h"
#include "game/GameLightDesc.h"
#include "game/GameMaterial.h"
#include "game/GameMesh.h"
#include "game/GamePlayerStart.h"
#include "game/GameSystem.h"
#include "game/GameTerrain.h"
#include "terrain/TerrainData.h"
#include "game/MapLoader.h"
#include "game/MeshTemplate.h"
#include "gldevices/GLDevices.h"
#include "renderer/GeometryUtils.h"
#include "renderer/GlobalLight.h"
#include "renderer/MaterialDesc.h"
#include "renderer/Renderer.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <loguru.hpp>

int main(int argc, char* argv[]) {
  core::Logger::Init(argc, argv);
  core::Config::Init(argc, argv);
  LOG_F(INFO, "ClaudeEngine starting up");
  LOG_F(INFO, "Data folder: %s", core::Config::GetDataFolder().c_str());
  core::AppConfig::Init(core::Config::GetDataFolder() / "config.yaml");
  new core::EventManager();

  // ---- Parse --map <path> or bare positional path -------------------------
  std::string map_path;
  for (int i = 1; i < argc; ++i) {
    const std::string arg(argv[i]);
    if (arg == "--map" && i + 1 < argc) {
      map_path = argv[++i];
    } else if (!arg.empty() && arg[0] != '-' && map_path.empty()) {
      map_path = arg;
    }
  }

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

  // ---- Objects whose lifetime spans the main loop -------------------------
  // Camera + controller for the default (no map / no map camera) path.
  game::GameCamera        camera(core::ProjectionType::kPerspective,
                                 core::CoordinateSystem::kRightHanded);
  game::FPSCameraController controller;

  // Map-loaded scene objects.
  std::unique_ptr<game::GameMesh>           map_floor;
  std::unique_ptr<game::GameLight>          map_global_light;
  std::unique_ptr<environment::WorldTime>   map_world_time;
  std::unique_ptr<environment::WindSystem>  map_wind_system;
  // cppcheck-suppress variableScope ; must outlive the main loop
  std::vector<std::unique_ptr<game::GameObject>> map_objects;
  game::GameCamera*      map_camera       = nullptr;
  game::GamePlayerStart* map_player_start = nullptr;
  game::GameTerrain*     map_terrain      = nullptr;

  // Demo-scene objects (only constructed when no map is loaded).
  game::GameMaterial* demo_mat = nullptr;
  std::unique_ptr<game::GameMesh> demo_obj_mesh;
  std::unique_ptr<game::GameMesh> demo_fbx_mesh;
  std::unique_ptr<game::GameMesh> demo_floor;
  std::optional<game::GameLight>  demo_global_light;
  std::optional<game::GameLight>  demo_spot_light;
  std::optional<game::GameLight>  demo_light_obj;
  std::optional<game::GameLight>  demo_light_fbx;
  std::optional<game::GameLight>  demo_light_fill;

  // ---- Attempt map load ----------------------------------------------------
  bool map_loaded = false;
  if (!map_path.empty()) {
    game::MapData map_data = game::MapLoader::Load(map_path, video);
    if (map_data.name.empty() && map_data.objects.empty()) {
      LOG_F(ERROR, "Failed to load map '%s', falling back to demo scene",
            map_path.c_str());
    } else {
      map_loaded = true;
      LOG_F(INFO, "Loaded map '%s'", map_data.name.c_str());

      // Floor — size driven by map_size.
      const float half = map_data.map_size / 2.f;
      auto* floor_mat = new game::GameMaterial(
          "__proc_floor",
          renderer::MaterialDesc().SetDiffuseColor({0.55f, 0.55f, 0.55f}), video);
      auto* plane_tmpl = new game::MeshTemplate(
          "__proc_floor", renderer::CreatePlaneMesh(video, half), floor_mat);
      floor_mat->Release();
      map_floor = std::make_unique<game::GameMesh>(plane_tmpl);
      map_floor->SetWorldTransform(core::Mat4f::kIdentity);
      game.AddObject(map_floor.get());
      plane_tmpl->Release();

      // Global directional light from map descriptor.
      map_global_light = std::make_unique<game::GameLight>(
          renderer::LightType::kGlobal, map_data.global_light);
      game.AddObject(map_global_light.get());

      const environment::EnvironmentDesc& env = map_data.environment_desc;

      if (env.sky_enabled) {
        map_world_time = std::make_unique<environment::WorldTime>(
            env.time_scale, env.start_time_of_day);
        new environment::SkyRenderer();
        environment::SkyRenderer::Instance().Build(video);
        renderer::Renderer::Instance().SetSkyRenderer(
            &environment::SkyRenderer::Instance());
      }

      if (env.water_enabled) {
        // Pre-scan objects to obtain terrain world dimensions for alignment.
        float terrain_w = 0.f;
        float terrain_h = 0.f;
        const auto terrain_it = std::find_if(
            map_data.objects.begin(), map_data.objects.end(),
            [](const std::unique_ptr<game::GameObject>& o) {
              return o->GetType() == game::GameObjectType::kTerrain;
            });
        if (terrain_it != map_data.objects.end()) {
          const auto* gt =
              static_cast<const game::GameTerrain*>(terrain_it->get());
          terrain_w = gt->GetData().GetWorldWidth();
          terrain_h = gt->GetData().GetWorldHeight();
        }
        new environment::WaterRenderer();
        environment::WaterRenderer::Instance().Build(
            video, env.water_level, terrain_w, terrain_h);
        renderer::Renderer::Instance().SetWaterRenderer(
            &environment::WaterRenderer::Instance());
      }

      if (env.cloud_enabled) {
        new environment::CloudRenderer();
        environment::CloudRenderer::Instance().Build(video);
        renderer::Renderer::Instance().SetCloudRenderer(
            &environment::CloudRenderer::Instance());
        renderer::Renderer::Instance().SetCloudDensity(env.cloud_density);
      }

      if (env.wind_enabled) {
        map_wind_system = std::make_unique<environment::WindSystem>(env);
        renderer::Renderer::Instance().SetWindSystem(map_wind_system.get());
      }

      renderer::Renderer::Instance().SetTreeEnabled(env.trees_enabled);

      // Add all map objects; locate the first camera and first player start.
      map_objects = std::move(map_data.objects);
      int player_start_count = 0;
      for (auto& obj : map_objects) {
        if (!map_camera)
          map_camera = dynamic_cast<game::GameCamera*>(obj.get());
        if (obj->GetType() == game::GameObjectType::kPlayerStart) {
          ++player_start_count;
          if (!map_player_start)
            map_player_start =
                static_cast<game::GamePlayerStart*>(obj.get());
        }
        if (!map_terrain && obj->GetType() == game::GameObjectType::kTerrain)
          map_terrain = static_cast<game::GameTerrain*>(obj.get());
        game.AddObject(obj.get());
      }
      if (player_start_count > 1)
        LOG_F(WARNING, "Map has %d player starts; using the first one",
              player_start_count);
      if (!map_player_start)
        LOG_F(WARNING, "Map has no player start; camera stays at default position");
    }
  }

  // ---- Camera setup --------------------------------------------------------
  if (map_camera) {
    map_camera->SetMaxDepth(200.f);
    map_camera->SetScreenCenter({gfx.GetWidth() * 0.5f, gfx.GetHeight() * 0.5f});
    game.SetCamera(map_camera);
  } else {
    camera.SetMaxDepth(200.f);
    camera.SetScreenCenter({gfx.GetWidth() * 0.5f, gfx.GetHeight() * 0.5f});
    game.SetCamera(&camera);
    game.SetCameraController(&controller);
    controller.SetTerrain(map_terrain ? &map_terrain->GetData() : nullptr);

    if (map_player_start) {
      const core::Mat4f& t = map_player_start->GetWorldTransform();
      controller.SetPosition({t(0, 3), t(1, 3), t(2, 3)});
    } else {
      controller.SetPosition({0.f, 5.f, 30.f});
    }
  }

  // ---- Demo scene (when no map was loaded) ---------------------------------
  if (!map_loaded) {
    const std::string data_dir = core::Config::GetDataFolder().string();

    demo_mat = game::GameMaterial::GetOrLoad("demo", video);

    game::MeshTemplate* obj_tmpl = game::MeshTemplate::GetOrLoad(
        data_dir + "/meshes/demo.obj", video, demo_mat);
    game::MeshTemplate* fbx_tmpl = game::MeshTemplate::GetOrLoad(
        data_dir + "/meshes/demo.fbx", video, demo_mat);

    if (obj_tmpl && obj_tmpl->GetMesh()) {
      demo_obj_mesh = std::make_unique<game::GameMesh>(obj_tmpl);
      demo_obj_mesh->SetWorldTransform(
          core::Mat4f::Translation({-10.f, 3.f, 0.f}) *
          core::Mat4f::Scale3D({3.f, 3.f, 3.f}));
      game.AddObject(demo_obj_mesh.get());
    }
    if (fbx_tmpl && fbx_tmpl->GetMesh()) {
      demo_fbx_mesh = std::make_unique<game::GameMesh>(fbx_tmpl);
      demo_fbx_mesh->SetWorldTransform(
          core::Mat4f::Translation({10.f, 3.f, 0.f}) *
          core::Mat4f::Scale3D({3.f, 3.f, 3.f}));
      game.AddObject(demo_fbx_mesh.get());
    }
    if (obj_tmpl) obj_tmpl->Release();
    if (fbx_tmpl) fbx_tmpl->Release();

    // Floor
    auto* floor_mat = new game::GameMaterial(
        "__proc_floor",
        renderer::MaterialDesc().SetDiffuseColor({0.55f, 0.55f, 0.55f}), video);
    auto* plane_tmpl = new game::MeshTemplate(
        "__proc_floor", renderer::CreatePlaneMesh(video, 120.f), floor_mat);
    floor_mat->Release();
    demo_floor = std::make_unique<game::GameMesh>(plane_tmpl);
    demo_floor->SetWorldTransform(core::Mat4f::kIdentity);
    game.AddObject(demo_floor.get());
    plane_tmpl->Release();

    // Lights
    game::GameLightDesc global_desc;
    global_desc.color         = core::Color(0.9f, 0.85f, 0.7f);
    global_desc.intensity     = 1.2f;
    global_desc.ambient_color = core::Vec3f(0.05f, 0.05f, 0.08f);
    global_desc.direction     = core::Vec3f(-0.4f, -0.8f, -0.3f).Normalized();
    demo_global_light.emplace(renderer::LightType::kGlobal, global_desc);
    game.AddObject(&*demo_global_light);

    game::GameLightDesc spot_desc;
    spot_desc.color       = core::Color(1.f, 0.9f, 0.8f);
    spot_desc.intensity   = 3.f;
    spot_desc.direction   = core::Vec3f(0.f, -1.f, -0.3f).Normalized();
    spot_desc.outer_angle = 0.4f;
    spot_desc.range       = 50.f;
    demo_spot_light.emplace(renderer::LightType::kCircleSpot, spot_desc);
    demo_spot_light->SetWorldTransform(core::Mat4f::Translation({10.f, 20.f, 5.f}));
    game.AddObject(&*demo_spot_light);

    game::GameLightDesc omni_obj_desc;
    omni_obj_desc.color     = core::Color(1.f, 0.6f, 0.2f);
    omni_obj_desc.intensity = 2.f;
    omni_obj_desc.radius    = 40.f;
    demo_light_obj.emplace(renderer::LightType::kOmni, omni_obj_desc);
    demo_light_obj->SetWorldTransform(core::Mat4f::Translation({-10.f, 8.f, 5.f}));
    game.AddObject(&*demo_light_obj);

    game::GameLightDesc omni_fbx_desc;
    omni_fbx_desc.color     = core::Color(0.3f, 0.6f, 1.f);
    omni_fbx_desc.intensity = 2.f;
    omni_fbx_desc.radius    = 40.f;
    demo_light_fbx.emplace(renderer::LightType::kOmni, omni_fbx_desc);
    demo_light_fbx->SetWorldTransform(core::Mat4f::Translation({10.f, 8.f, 5.f}));
    game.AddObject(&*demo_light_fbx);

    game::GameLightDesc omni_fill_desc;
    omni_fill_desc.color     = core::Color(0.9f, 0.9f, 1.f);
    omni_fill_desc.intensity = 1.f;
    omni_fill_desc.radius    = 60.f;
    demo_light_fill.emplace(renderer::LightType::kOmni, omni_fill_desc);
    demo_light_fill->SetWorldTransform(core::Mat4f::Translation({0.f, 20.f, 10.f}));
    game.AddObject(&*demo_light_fill);
  }

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
  float prev_elapsed = 0.f;
  while (game.IsRunning()) {
    if (map_world_time || map_wind_system) {
      const float elapsed  = game.GetElapsedTime();
      const float frame_dt = elapsed - prev_elapsed;
      prev_elapsed = elapsed;

      if (map_world_time) {
        map_world_time->Advance(frame_dt);
        renderer::Renderer::Instance().SetSkyWorldTime(
            map_world_time->GetTimeOfDay());
        renderer::Renderer::Instance().SetWaterSkyWorldTime(
            map_world_time->GetTimeOfDay());
        if (map_global_light) {
          auto* gl = static_cast<renderer::GlobalLight*>(
              map_global_light->GetLight());
          game::UpdateGlobalLight(*gl, *map_world_time);
        }
      }

      if (map_wind_system) {
        map_wind_system->Update(frame_dt);
      }
    }
    video->BeginFrame();
    video->ClearRenderTargets(core::Color::kBlack);
    renderer::Renderer::Instance().SetDebugMode(debug_mode);
    game.Update();
  }

  if (demo_mat) demo_mat->Release();

  if (environment::SkyRenderer::IsInstanced()) {
    environment::SkyRenderer::Instance().Reset();
    environment::SkyRenderer::Shutdown();
  }
  if (environment::WaterRenderer::IsInstanced()) {
    environment::WaterRenderer::Instance().Reset();
    environment::WaterRenderer::Shutdown();
  }
  if (environment::CloudRenderer::IsInstanced()) {
    environment::CloudRenderer::Instance().Reset();
    environment::CloudRenderer::Shutdown();
  }

  game::GameSystem::Shutdown();
  renderer::Renderer::Shutdown();
  core::EventManager::Shutdown();

  LOG_F(INFO, "ClaudeEngine shutting down");
  core::Logger::Shutdown();
  return 0;
}
