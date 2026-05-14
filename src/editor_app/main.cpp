#include "core/AppConfig.h"
#include "core/Config.h"
#include "core/EventManager.h"
#include "core/Logger.h"
#include "editor/EditorSystem.h"
#include "gldevices/GLDevices.h"
#include "renderer/Renderer.h"

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

  new renderer::Renderer(devices.GetVideoDevice());
  renderer::Renderer::Instance().InitVisibilitySystems(200.f);

  new editor::EditorSystem(&devices);
  editor::EditorSystem::Instance().Run();

  editor::EditorSystem::Shutdown();
  renderer::Renderer::Shutdown();
  core::EventManager::Shutdown();

  LOG_F(INFO, "ClaudeEditor shutting down");
  core::Logger::Shutdown();
  return 0;
}
