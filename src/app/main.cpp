// Wreckoning application entrypoint.
// Responsibilities (src/CLAUDE.md): load configuration, run the engine.

#include "abstract/PrimitiveType.h"
#include "abstract/VideoDevice.h"
#include "audio/ResourceManager.h"
#include "audio/SoundManager.h"
#include "audio/SoundSystemFactory.h"
#include "core/AppConfig.h"
#include "core/Color.h"
#include "core/Config.h"
#include "core/CoordinateSystem.h"
#include "core/EventManager.h"
#include "core/EventType.h"
#include "core/Key.h"
#include "core/Logger.h"
#include "core/Profiler.h"
#include "core/Mat4f.h"
#include "core/ProjectionType.h"
#include "core/Vec3f.h"
#include "environment/CloudRenderer.h"
#include "environment/CloudShadowRenderer.h"
#include "environment/SkyRenderer.h"
#include "environment/WaterRenderer.h"
#include "environment/WindSystem.h"
#include "environment/WorldTime.h"
#include "game/ChaseCameraController.h"
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
#include "game/GameVehicle.h"
#include "terrain/TerrainData.h"
#include "game/MapLoader.h"
#include "game/MeshTemplate.h"
#include "game/PlayerVehicleController.h"
#include "game/VehicleTemplate.h"
#include "gldevices/GLDevices.h"
#include "physics/PhysicsSystem.h"
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
  if (core::Config::IsProfilingEnabled())
    new core::Profiler(core::Config::GetProfileInterval());
  LOG_F(INFO, "Wreckoning starting up");
  LOG_F(INFO, "Data folder: %s", core::Config::GetDataFolder().c_str());
  core::AppConfig::Init(core::Config::GetDataFolder() / "config.yaml");
  new core::EventManager();

  // ---- Parse --map <path> and --vehicle <desc_path> -----------------------
  std::string map_path;
  std::string vehicle_path;
  for (int i = 1; i < argc; ++i) {
    const std::string arg(argv[i]);
    if (arg == "--map" && i + 1 < argc) {
      map_path = argv[++i];
    } else if (arg == "--vehicle" && i + 1 < argc) {
      vehicle_path = argv[++i];
    } else if (!arg.empty() && arg[0] != '-' && map_path.empty()) {
      map_path = arg;
    }
  }

  const core::GraphicsConfig& gfx = core::AppConfig::GetGraphics();
  gldevices::GLDevices devices(gfx.GetWidth(), gfx.GetHeight(), !gfx.IsWindowed());
  abstract::VideoDevice* video = devices.GetVideoDevice();

  video->SetDepthTestEnabled(true);
  video->SetPrimitiveType(abstract::PrimitiveType::kTriangleList);
  video->SetIndexType(abstract::IndexType::kUInt32);

  new renderer::Renderer(video);
  renderer::Renderer::Instance().InitVisibilitySystems(200.f);

  new physics::PhysicsSystem();
  physics::PhysicsSystem::Instance().Init();

  new game::GameSystem(&devices);
  game::GameSystem& game = game::GameSystem::Instance();

  // ---- Audio ---------------------------------------------------------------
  auto sound_system = audio::SoundSystemFactory::Create();
  std::unique_ptr<audio::SoundManager>    sound_manager;
  std::unique_ptr<audio::ResourceManager> sound_resources;
  if (sound_system && sound_system->Initialize()) {
    sound_manager    = std::make_unique<audio::SoundManager>(sound_system.get());
    sound_resources  = std::make_unique<audio::ResourceManager>(sound_system.get());
    game.SetSoundManager(sound_manager.get());
    LOG_F(INFO, "Audio system initialised");
  } else {
    LOG_F(WARNING, "Audio system unavailable — sound emitters will be silent");
    sound_system.reset();
  }

  // ---- Objects whose lifetime spans the main loop -------------------------
  // Camera + controller for the default (no map / no map camera) path.
  game::GameCamera        camera(core::ProjectionType::kPerspective,
                                 core::CoordinateSystem::kRightHanded);
  game::FPSCameraController controller;

  // Map-loaded scene objects.
  std::unique_ptr<game::GameLight>          map_global_light;
  std::unique_ptr<environment::WorldTime>   map_world_time;
  std::unique_ptr<environment::WindSystem>  map_wind_system;
  // cppcheck-suppress variableScope ; must outlive the main loop
  std::vector<std::unique_ptr<game::GameObject>> map_objects;
  game::GameCamera*      map_camera       = nullptr;
  game::GamePlayerStart* map_player_start = nullptr;
  game::GameTerrain*     map_terrain      = nullptr;

  // ---- Attempt map load ----------------------------------------------------
  if (!map_path.empty()) {
    game::MapData map_data = game::MapLoader::Load(
        map_path, video, sound_manager.get(), sound_resources.get());
    if (map_data.name.empty() && map_data.objects.empty()) {
      LOG_F(ERROR, "Failed to load map '%s', falling back to demo scene",
            map_path.c_str());
    } else {
      LOG_F(INFO, "Loaded map '%s'", map_data.name.c_str());
      renderer::Renderer::Instance().InitVisibilitySystems(map_data.map_size);

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
        environment::SkyRenderer::Instance().SetTurbidity(env.turbidity);
        environment::SkyRenderer::Instance().SetMoonTexture(env.moon_texture);
        environment::SkyRenderer::Instance().SetNightSkyTexture(env.night_sky_texture);
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
        environment::WaterRenderer& wr = environment::WaterRenderer::Instance();
        wr.Build(video, env.water_level, terrain_w, terrain_h);
        wr.SetWaterColor(env.water_color_r, env.water_color_g, env.water_color_b);
        wr.SetRoughness(env.roughness);
        wr.SetSunIntensity(env.sun_intensity);
        wr.SetRefractionStrength(env.refraction_strength);
        wr.SetAbsorptionScale(env.absorption_scale);
        wr.SetFoamParams(env.foam_height_thresh, env.foam_shoreline_depth,
                         env.foam_steepness_thresh, env.foam_speed);
        wr.SetFoamTexParams(env.foam_scale1, env.foam_scale2,
                            env.foam_scroll_speed1, env.foam_scroll_speed2);
        wr.SetNormalMapParams(env.normal_scale1, env.normal_scale2,
                              env.normal_scroll_speed1, env.normal_scroll_speed2);
        renderer::Renderer::Instance().SetWaterRenderer(&wr);
      }

      if (env.cloud_enabled) {
        new environment::CloudRenderer();
        environment::CloudRenderer::Instance().Build(video);
        renderer::Renderer::Instance().SetCloudRenderer(
            &environment::CloudRenderer::Instance());
        renderer::Renderer::Instance().SetCloudDensity(env.cloud_density);

        new environment::CloudShadowRenderer();
        environment::CloudShadowRenderer::Instance().Build(video);
        renderer::Renderer::Instance().SetCloudShadowRenderer(
            &environment::CloudShadowRenderer::Instance());
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

  // ---- Vehicle spawn (optional) --------------------------------------------
  game::VehicleTemplate*                         vehicle_tmpl = nullptr;
  game::GameVehicle*                             vehicle_ptr  = nullptr;
  std::unique_ptr<game::PlayerVehicleController> player_vehicle_ctrl;
  std::unique_ptr<game::ChaseCameraController>   chase_controller;

  if (!vehicle_path.empty() && map_player_start) {
    vehicle_tmpl = game::VehicleTemplate::GetOrLoad(vehicle_path, video);
    if (!vehicle_tmpl) {
      LOG_F(ERROR, "Failed to load vehicle '%s', falling back to FPS camera",
            vehicle_path.c_str());
    } else {
      core::Mat4f world(map_player_start->GetWorldTransform());
      world(1, 3) = world(1, 3) + 1.f;
      map_player_start->SetWorldTransform(world);
      auto vehicle_owned = std::make_unique<game::GameVehicle>(vehicle_tmpl);
      vehicle_ptr = vehicle_owned.get();
      vehicle_ptr->SetWorldTransform(map_player_start->GetWorldTransform());
      game.AddObject(vehicle_ptr);
      map_objects.push_back(std::move(vehicle_owned));

      player_vehicle_ctrl = std::make_unique<game::PlayerVehicleController>();
      vehicle_ptr->SetVehicleController(player_vehicle_ctrl.get());
      vehicle_ptr->Activate();

      chase_controller = std::make_unique<game::ChaseCameraController>();
      chase_controller->SetCamera(&camera);
      chase_controller->SetTarget(vehicle_ptr);
    }
  }

  // ---- Camera setup --------------------------------------------------------
  if (map_camera) {
    map_camera->SetMaxDepth(1000.f);
    map_camera->SetScreenCenter({gfx.GetWidth() * 0.5f, gfx.GetHeight() * 0.5f});
    game.SetCamera(map_camera);
  } else {
    camera.SetMaxDepth(1000.f);
    camera.SetScreenCenter({gfx.GetWidth() * 0.5f, gfx.GetHeight() * 0.5f});
    game.SetCamera(&camera);
    if (vehicle_ptr) {
      game.SetCameraController(chase_controller.get());
    } else {
      game.SetCameraController(&controller);
      if (map_player_start) {
        const core::Mat4f& t = map_player_start->GetWorldTransform();
        controller.SetPosition({t(0, 3), t(1, 3), t(2, 3)});
      } else {
        controller.SetPosition({0.f, 5.f, 30.f});
      }
    }
  }

  // ---- Debug mode -----------------------------------------------------------
  // Keys 0/Esc = full pipeline, 1-4 = G-buffer channels.
  // Tab = cycle shadow-map debug overlay (one entry per light with shadows).
  // P   = toggle physics body wireframe overlay.
  renderer::DebugMode debug_mode = renderer::DebugMode::kNone;
  bool physics_debug_enabled = false;
  game.SetEventCallback([&debug_mode, &physics_debug_enabled,
                         &player_vehicle_ctrl](const core::Event& e) {
    if (player_vehicle_ctrl) player_vehicle_ctrl->OnEvent(e);
    if (e.type != core::EventType::kKeyDown) return;
    if (e.key == core::Key::kEscape || e.key == core::Key::k0)
      debug_mode = renderer::DebugMode::kNone;
    if (e.key == core::Key::k1) debug_mode = renderer::DebugMode::kAlbedo;
    if (e.key == core::Key::k2) debug_mode = renderer::DebugMode::kNormal;
    if (e.key == core::Key::k3) debug_mode = renderer::DebugMode::kSpecular;
    if (e.key == core::Key::k4) debug_mode = renderer::DebugMode::kDepth;
    if (e.key == core::Key::kTab)
      renderer::Renderer::Instance().CycleShadowDebug();
    if (e.key == core::Key::kP)
      physics_debug_enabled = !physics_debug_enabled;
  });

  // Capture the cursor so mouse deltas are delivered even at window/screen edges.
  devices.SetCursorCapture(true);

  // Reset the frame timer so the first Update() dt is near zero; without this,
  // map loading time inflates the first dt and the physics character overshoots
  // the player-start position.
  game.ResetTimer();

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
    if (physics_debug_enabled)
      physics::PhysicsSystem::Instance().DrawDebug({.drawShapes = true});
    game.Update();
    if (core::Profiler::IsInstanced())
      core::Profiler::Instance().MarkFrame();
  }

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
  if (environment::CloudShadowRenderer::IsInstanced()) {
    environment::CloudShadowRenderer::Instance().Reset();
    environment::CloudShadowRenderer::Shutdown();
  }

  if (vehicle_tmpl) vehicle_tmpl->Release();

  physics::PhysicsSystem::Shutdown();
  game::GameSystem::Shutdown();

  // Destroy SoundManager and ResourceManager before shutting down the backend.
  sound_resources.reset();
  sound_manager.reset();
  if (sound_system) sound_system->Shutdown();

  renderer::Renderer::Shutdown();
  core::EventManager::Shutdown();

  if (core::Profiler::IsInstanced())
    core::Profiler::Shutdown();

  LOG_F(INFO, "Wreckoning shutting down");
  core::Logger::Shutdown();
  return 0;
}
