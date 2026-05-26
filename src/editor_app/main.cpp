#include "abstract/IndexType.h"
#include "abstract/PrimitiveType.h"
#include "abstract/VideoDevice.h"
#include "core/AppConfig.h"
#include "core/Config.h"
#include "core/EventManager.h"
#include "core/Logger.h"
#include "editor/EditorSystem.h"
#include "game/GameSystem.h"
#include "gldevices/GLDevices.h"
#include "renderer/Renderer.h"
#include "terrain/TerrainRenderer.h"

#include <loguru.hpp>

int main(int argc, char* argv[]) {
  core::Logger::Init(argc, argv);
  core::Config::Init(argc, argv);
  LOG_F(INFO, "ClaudeEditor starting up");
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

  new terrain::TerrainRenderer();
  renderer::Renderer::Instance().SetTerrainRenderer(
      &terrain::TerrainRenderer::Instance());

  new game::GameSystem(&devices);

  new editor::EditorSystem(&devices);
  editor::EditorSystem::Instance().Run();

  editor::EditorSystem::Shutdown();
  game::GameSystem::Shutdown();
  terrain::TerrainRenderer::Shutdown();
  renderer::Renderer::Shutdown();
  core::EventManager::Shutdown();

  LOG_F(INFO, "ClaudeEditor shutting down");
  core::Logger::Shutdown();
  return 0;
}
