#include "renderer/Renderer.h"

#include <array>

#include "abstract/BufferUsage.h"
#include "abstract/TextureFormat.h"
#include "core/Color.h"
#include "renderer/MaterialInfos.h"
#include "renderer/MeshRenderer.h"
#include "renderer/RenderableInfos.h"
#include "renderer/SceneInfos.h"

namespace renderer {

namespace {
constexpr int kRenderableInfosSlot    = 1;
constexpr int kRenderableInfosFloat4s = sizeof(RenderableInfos) / 16;  // 64 / 16 = 4
constexpr int kSceneInfosSlot         = 2;
constexpr int kSceneInfosFloat4s      = sizeof(SceneInfos) / 16;        // 352 / 16 = 22
constexpr int kMaterialInfosSlot      = 3;
constexpr int kMaterialInfosFloat4s   = sizeof(MaterialInfos) / 16;     // 64 / 16 = 4
}  // namespace

Renderer::Renderer(abstract::VideoDevice* video) : video_(video) {
  renderable_infos_cb_ = video_->CreateConstantBuffer(
      kRenderableInfosFloat4s, kRenderableInfosSlot, abstract::BufferUsage::kDynamic);
  scene_infos_cb_ = video_->CreateConstantBuffer(
      kSceneInfosFloat4s, kSceneInfosSlot, abstract::BufferUsage::kDynamic);
  material_infos_cb_ = video_->CreateConstantBuffer(
      kMaterialInfosFloat4s, kMaterialInfosSlot, abstract::BufferUsage::kDynamic);

  CreateRenderTargets(video_->GetWidth(), video_->GetHeight());
  new MeshRenderer(video_);
}

Renderer::~Renderer() {
  MeshRenderer::Shutdown();
}

void Renderer::SetCamera(const core::Camera* camera) {
  camera_ = camera;
  if (camera_) FillSceneInfos();
}

void Renderer::Update(float time, const core::Camera* camera) {
  time_   = time;
  camera_ = camera;
  renderable_infos_cb_->Bind();
  scene_infos_cb_->Bind();
  material_infos_cb_->Bind();
  if (camera_) FillSceneInfos();

  // Geometry pass: fill G-buffer albedo, normal, specular MRTs.
  gbuffer_->BindForWriting();
  video_->ClearRenderTargets(core::Color::kBlack);
  video_->SetDepthTestEnabled(true);
  MeshRenderer::Instance().PrepareRender();
  MeshRenderer::Instance().Render();
  MeshRenderer::Instance().EndRender();
  gbuffer_->UnbindForWriting();
}

void Renderer::SetRenderableInfos(const core::Mat4f& world_matrix) {
  RenderableInfos ri;
  ri.world = world_matrix;
  renderable_infos_cb_->Fill(&ri);
}

void Renderer::OnResize(int w, int h) {
  // Destroy FBO bundles before their referenced RTs.
  emissive_fbo_.reset();
  gbuffer_.reset();
  gbuffer_albedo_rt_.reset();
  gbuffer_normal_rt_.reset();
  gbuffer_specular_rt_.reset();
  gbuffer_depth_rt_.reset();
  hdr_rt_.reset();
  CreateRenderTargets(w, h);
}

void Renderer::FillSceneInfos() {
  const core::Vec2f sc = camera_->GetScreenCenter();  // (half_w, half_h)

  SceneInfos si;
  si.view_proj       = camera_->GetViewProjectionMatrix();
  si.inv_view_proj   = camera_->GetViewProjectionMatrix().Inverse();
  si.inv_proj        = camera_->GetProjectionMatrix().Inverse();
  si.proj            = camera_->GetProjectionMatrix();
  si.view            = camera_->GetViewMatrix();
  si.eye_pos         = camera_->GetPosition();
  si.time            = time_;
  si.pad0_           = 0.f;
  si.inv_screen_size = {1.f / (2.f * sc.x), 1.f / (2.f * sc.y)};

  scene_infos_cb_->Fill(&si);
}

void Renderer::CreateRenderTargets(int w, int h) {
  using abstract::TextureFormat;

  gbuffer_albedo_rt_   = video_->CreateRenderTarget(w, h, TextureFormat::kRGBA8);
  gbuffer_normal_rt_   = video_->CreateRenderTarget(w, h, TextureFormat::kRGBA16F);
  gbuffer_specular_rt_ = video_->CreateRenderTarget(w, h, TextureFormat::kRGBA8);
  gbuffer_depth_rt_    = video_->CreateRenderTarget(w, h, TextureFormat::kDepth24Stencil8);
  hdr_rt_              = video_->CreateRenderTarget(w, h, TextureFormat::kRGBA16F);

  std::array<abstract::RenderTarget*, 3> gbuffer_colors = {
      gbuffer_albedo_rt_.get(),
      gbuffer_normal_rt_.get(),
      gbuffer_specular_rt_.get(),
  };
  gbuffer_ = video_->CreateRenderTargetGroup(gbuffer_colors, gbuffer_depth_rt_.get());

  // Emissive FBO reuses the G-buffer depth+stencil RT for depth testing.
  std::array<abstract::RenderTarget*, 1> emissive_colors = {hdr_rt_.get()};
  emissive_fbo_ = video_->CreateRenderTargetGroup(emissive_colors, gbuffer_depth_rt_.get());
}

}  // namespace renderer
