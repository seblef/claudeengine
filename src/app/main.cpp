// ClaudeEngine application entrypoint.
// Responsibilities (src/CLAUDE.md): load configuration, run the engine.

#include "core/EventManager.h"
#include "core/EventType.h"
#include "core/Logger.h"
#include "core/MouseButton.h"
#include "gldevices/GLDevices.h"

#include <loguru.hpp>

int main(int argc, char* argv[]) {
  core::Logger::Init(argc, argv);
  LOG_F(INFO, "ClaudeEngine starting up");

  // TODO(#1): Load engine configuration from data/config.yaml
  // TODO(#2): Instantiate and run the Engine

  gldevices::GLDevices devices(640, 480, /*fullscreen=*/false);

  bool running = true;
  while (running) {
    devices.Update();
    while (core::EventManager::Instance().HasEvents()) {
      core::Event e = core::EventManager::Instance().Consume();
      switch (e.type) {
        case core::EventType::kWindowClose:
          LOG_F(INFO, "Event: WindowClose");
          running = false;
          break;
        case core::EventType::kWindowLostFocus:
          LOG_F(INFO, "Event: WindowLostFocus");
          break;
        case core::EventType::kWindowGetFocus:
          LOG_F(INFO, "Event: WindowGetFocus");
          break;
        case core::EventType::kWindowMinimized:
          LOG_F(INFO, "Event: WindowMinimized");
          break;
        case core::EventType::kWindowRestored:
          LOG_F(INFO, "Event: WindowRestored");
          break;
        case core::EventType::kMouseMoved:
          LOG_F(INFO, "Event: MouseMoved (%.1f, %.1f)", e.mouse_x, e.mouse_y);
          break;
        case core::EventType::kMouseWheelChanged:
          LOG_F(INFO, "Event: MouseWheel (%.2f)", e.wheel_delta);
          break;
        case core::EventType::kMouseButtonDown:
          LOG_F(INFO, "Event: MouseButtonDown (%s)",
                e.mouse_button == core::MouseButton::kLeft ? "Left" : "Right");
          break;
        case core::EventType::kMouseButtonUp:
          LOG_F(INFO, "Event: MouseButtonUp (%s)",
                e.mouse_button == core::MouseButton::kLeft ? "Left" : "Right");
          break;
        case core::EventType::kKeyDown:
          LOG_F(INFO, "Event: KeyDown (%d)", static_cast<int>(e.key));
          break;
        case core::EventType::kKeyUp:
          LOG_F(INFO, "Event: KeyUp (%d)", static_cast<int>(e.key));
          break;
      }
    }
  }

  LOG_F(INFO, "ClaudeEngine shutting down");
  core::Logger::Shutdown();
  return 0;
}
