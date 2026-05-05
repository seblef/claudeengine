// ClaudeEngine application entrypoint.
// Responsibilities (src/CLAUDE.md): load configuration, run the engine.

#include "abstract/ConstantBuffer.h"
#include "abstract/PrimitiveType.h"
#include "abstract/Shader.h"
#include "abstract/VideoDevice.h"
#include "core/Color.h"
#include "core/AppConfig.h"
#include "core/Config.h"
#include "core/EventManager.h"
#include "core/EventType.h"
#include "core/Logger.h"
#include "core/MouseButton.h"
#include "core/Vertex2D.h"
#include "gldevices/GLDevices.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <loguru.hpp>

int main(int argc, char* argv[]) {
  core::Logger::Init(argc, argv);
  core::Config::Init(argc, argv);
  LOG_F(INFO, "ClaudeEngine starting up");
  LOG_F(INFO, "Data folder: %s", core::Config::GetDataFolder().c_str());
  core::AppConfig::Init(core::Config::GetDataFolder() / "config.yaml");

  // TODO(#2): Instantiate and run the Engine

  const core::GraphicsConfig& gfx = core::AppConfig::GetGraphics();
  gldevices::GLDevices devices(gfx.GetWidth(), gfx.GetHeight(), !gfx.IsWindowed());
  abstract::VideoDevice* video = devices.GetVideoDevice();

  abstract::Shader* shader = video->CreateShader("passthrough_color_2d");

  // Constant buffer: 1 float4 at slot 0 — tint color for the fragment shader.
  auto cb = video->CreateConstantBuffer(1, 0);
  cb->Bind();

  const auto t_start = std::chrono::steady_clock::now();

  // Fullscreen quad: 4 unique vertices covering clip space [-1, 1].
  // Corner colours: top-left=Red, bottom-left=Blue, bottom-right=White, top-right=Green.
  const core::Vec2f uv0{0.f, 0.f};
  const core::Vertex2D verts[4] = {
      {{-1.f,  1.f, 0.f}, core::Color::kRed,   uv0},  // 0: TL
      {{-1.f, -1.f, 0.f}, core::Color::kBlue,  uv0},  // 1: BL
      {{ 1.f, -1.f, 0.f}, core::Color::kWhite, uv0},  // 2: BR
      {{ 1.f,  1.f, 0.f}, core::Color::kGreen, uv0},  // 3: TR
  };
  const uint16_t indices[6] = {0, 1, 2,  0, 2, 3};  // two CCW triangles

  auto gb = video->CreateGeometryBuffer(
      core::VertexType::k2D, 4,
      abstract::IndexType::kUInt16, 6,
      abstract::BufferUsage::kImmutable,
      verts, 0, indices, 0);

  gb->Bind();
  video->SetIndexType(abstract::IndexType::kUInt16);
  video->SetPrimitiveType(abstract::PrimitiveType::kTriangleList);

  bool running = true;
  while (running) {
    devices.Update();

    video->BeginFrame();
    video->ClearRenderTargets(core::Color::kBlack);

    // Animate tint: blue at t=0s → red at t=5s → blue at t=10s (sinusoidal).
    const float elapsed = std::chrono::duration<float>(
        std::chrono::steady_clock::now() - t_start).count();
    const float lerp_t = (1.0f - std::cos(elapsed * (3.14159265f / 5.0f))) * 0.5f;
    const core::Color tint = core::Color::kBlue.Lerp(core::Color::kRed, lerp_t);
    cb->Fill(&tint);

    shader->Activate();
    gb->Bind();
    video->RenderIndexed(6);

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

  if (shader) shader->Release();

  LOG_F(INFO, "ClaudeEngine shutting down");
  core::Logger::Shutdown();
  return 0;
}
