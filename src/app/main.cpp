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
#include "mesh/FbxImporter.h"
#include "mesh/LodData.h"
#include "mesh/MeshData.h"
#include "mesh/ObjImporter.h"
#include "renderer/GeometryData.h"
#include "renderer/GlobalLight.h"
#include "renderer/Material.h"
#include "renderer/MaterialDesc.h"
#include "renderer/Mesh.h"
#include "renderer/MeshInstance.h"
#include "renderer/OmniLight.h"
#include "renderer/Renderer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>
#include <loguru.hpp>

namespace {

constexpr float kCameraSpeed = 10.f;   // units/s
constexpr float kMouseSens   = 0.002f;  // rad/px

// Converts a LodData to a GPU GeometryData. Downcasts uint32_t indices to
// uint16_t — safe for meshes with fewer than 65 536 vertices.
// Returns nullptr and logs an error if the vertex count exceeds the limit.
std::unique_ptr<renderer::GeometryData> MakeGeometry(
    abstract::VideoDevice* video, const mesh::LodData& lod) {
  if (lod.vertices.size() > 65535u) {
    LOG_F(ERROR, "Imported mesh exceeds 16-bit index limit (%zu vertices)",
          lod.vertices.size());
    return nullptr;
  }
  std::vector<uint16_t> idx16(lod.indices.size());
  std::transform(lod.indices.begin(), lod.indices.end(), idx16.begin(),
                 [](uint32_t i) { return static_cast<uint16_t>(i); });
  return std::make_unique<renderer::GeometryData>(
      video,
      static_cast<int>(lod.vertices.size()), lod.vertices.data(),
      static_cast<int>(lod.indices.size()) / 3, idx16.data());
}

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

  // ---- Imported demo meshes ------------------------------------------------
  // demo.obj (orange) placed left of origin; demo.fbx (blue) placed right.
  // Each submesh gets its own GeometryData, Material, Mesh and MeshInstance.
  const std::string data_dir = core::Config::GetDataFolder().string();
  const std::string obj_path = data_dir + "/meshes/demo.obj";
  const std::string fbx_path = data_dir + "/meshes/demo.fbx";

  renderer::Material obj_mat(
      renderer::MaterialDesc()
          .SetDiffuse("demo.png")
          .SetDiffuseColor(core::Color(1.f, 0.5f, 0.f))
          .SetShininess(32.f),
      video);
  renderer::Material fbx_mat(
      renderer::MaterialDesc()
          .SetDiffuse("demo.png")
          .SetDiffuseColor(core::Color(0.2f, 0.6f, 1.f))
          .SetShininess(64.f),
      video);

  const core::Mat4f obj_world =
      core::Mat4f::Translation({-10.f, 0.f, 0.f}) * core::Mat4f::Scale3D({3.f, 3.f, 3.f});
  const core::Mat4f fbx_world =
      core::Mat4f::Translation({ 10.f, 0.f, 0.f}) * core::Mat4f::Scale3D({3.f, 3.f, 3.f});

  std::vector<std::unique_ptr<renderer::GeometryData>> demo_geos;
  std::vector<std::unique_ptr<renderer::Mesh>>         demo_meshes;
  std::vector<std::unique_ptr<renderer::MeshInstance>> demo_instances;

  // Uploads all LOD-0 submeshes of mesh_data to the GPU and registers them
  // with the renderer, pairing each submesh with mat and world.
  auto RegisterMeshData = [&](const mesh::MeshData& mesh_data,
                               renderer::Material* mat,
                               const core::Mat4f& world) {
    for (const auto& sub : mesh_data.submeshes) {
      if (sub.lods.empty()) continue;
      auto geo = MakeGeometry(video, sub.lods[0]);
      if (!geo) continue;
      auto mesh_obj = std::make_unique<renderer::Mesh>(geo.get(), mat);
      auto inst = std::make_unique<renderer::MeshInstance>(
          mesh_obj.get(), world, false);
      renderer::Renderer::Instance().AddRenderable(inst.get());
      demo_geos.push_back(std::move(geo));
      demo_meshes.push_back(std::move(mesh_obj));
      demo_instances.push_back(std::move(inst));
    }
  };

  mesh::MeshData obj_data;
  mesh::ObjImporter obj_importer;
  if (obj_importer.Import(obj_path, &obj_data))
    RegisterMeshData(obj_data, &obj_mat, obj_world);

  mesh::MeshData fbx_data;
  mesh::FbxImporter fbx_importer;
  if (fbx_importer.Import(fbx_path, &fbx_data))
    RegisterMeshData(fbx_data, &fbx_mat, fbx_world);

  // ---- Lights --------------------------------------------------------------
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
