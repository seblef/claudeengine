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
#include "environment/CloudShadowRenderer.h"
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
  video->SetIndexType(abstract::IndexType::kUInt32);

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
    game::MapData map_data = game::MapLoader::Load(map_path, video);
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

  // ---- Camera setup --------------------------------------------------------
  if (map_camera) {
    map_camera->SetMaxDepth(1000.f);
    map_camera->SetScreenCenter({gfx.GetWidth() * 0.5f, gfx.GetHeight() * 0.5f});
    game.SetCamera(map_camera);
  } else {
    camera.SetMaxDepth(1000.f);
    camera.SetScreenCenter({gfx.GetWidth() * 0.5f, gfx.GetHeight() * 0.5f});
    game.SetCamera(&camera);
    game.SetCameraController(&controller);

    if (map_player_start) {
      const core::Mat4f& t = map_player_start->GetWorldTransform();
      controller.SetPosition({t(0, 3), t(1, 3), t(2, 3)});
    } else {
      controller.SetPosition({0.f, 5.f, 30.f});
    }
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

  // Capture the cursor so mouse deltas are delivered even at window/screen edges.
  devices.SetCursorCapture(true);

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

  game::GameSystem::Shutdown();
  renderer::Renderer::Shutdown();
  core::EventManager::Shutdown();

  LOG_F(INFO, "ClaudeEngine shutting down");
  core::Logger::Shutdown();
  return 0;
}
