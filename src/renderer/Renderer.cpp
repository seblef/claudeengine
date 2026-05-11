#include "renderer/Renderer.h"

#include "abstract/BlendFactor.h"
#include "abstract/BufferUsage.h"
#include "core/Color.h"
#include "renderer/GeometryUtils.h"
#include "renderer/LightRenderer.h"
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

Renderer::Renderer(abstract::VideoDevice* video)
    : video_(video),
      composite_shader_(video->CreateShader("composite")),
      debug_shader_(video->CreateShader("debug_blit")),
      composite_quad_(CreateQuad(video)) {
  renderable_infos_cb_ = video_->CreateConstantBuffer(
      kRenderableInfosFloat4s, kRenderableInfosSlot, abstract::BufferUsage::kDynamic);
  scene_infos_cb_ = video_->CreateConstantBuffer(
      kSceneInfosFloat4s, kSceneInfosSlot, abstract::BufferUsage::kDynamic);
  material_infos_cb_ = video_->CreateConstantBuffer(
      kMaterialInfosFloat4s, kMaterialInfosSlot, abstract::BufferUsage::kDynamic);

  const int w = video_->GetWidth();
  const int h = video_->GetHeight();
  gbuffer_.Create(video_, w, h);
  emissive_fbo_.Create(video_, w, h, gbuffer_.GetDepthRT());

  new MeshRenderer(video_);
  new LightRenderer(video_);
}

Renderer::~Renderer() {
  composite_shader_->Release();
  debug_shader_->Release();
  LightRenderer::Shutdown();
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

  // 1. Geometry pass — fill albedo, normal, specular MRTs and depth+stencil.
  gbuffer_.BindForWriting();
  video_->SetDepthTestEnabled(true);
  video_->SetDepthWriteEnabled(true);
  video_->ClearRenderTargets(core::Color::kBlack);
  MeshRenderer::Instance().PrepareRender();
  MeshRenderer::Instance().Render();
  gbuffer_.UnbindForWriting();

  // Debug bypass — blit a chosen G-buffer RT to the default framebuffer and skip
  // the lighting, emissive, and composite passes entirely.
  if (debug_mode_ != DebugMode::kNone) {
    abstract::RenderTarget* rt = nullptr;
    switch (debug_mode_) {
      case DebugMode::kAlbedo:   rt = gbuffer_.GetAlbedoRT();   break;
      case DebugMode::kNormal:   rt = gbuffer_.GetNormalRT();   break;
      case DebugMode::kSpecular: rt = gbuffer_.GetSpecularRT(); break;
      case DebugMode::kDepth:    rt = gbuffer_.GetDepthRT();    break;
      default: break;
    }
    if (rt) rt->BindAsSampler(0);
    debug_shader_->Activate();
    debug_shader_->SetUniformInt("u_debug_mode", static_cast<int>(debug_mode_));
    composite_quad_->Set();
    video_->RenderIndexed(composite_quad_->GetNumIndices());
    MeshRenderer::Instance().EndRender();
    LightRenderer::Instance().EndRender();
    return;
  }

  // 2. Lighting pass — shade into the HDR RT; G-buffer RTs as samplers.
  //    Depth write is disabled so glClear only clears the HDR color attachment,
  //    preserving G-buffer depth for stencil sub-passes and position reconstruction.
  emissive_fbo_.BindForWriting();
  video_->SetDepthWriteEnabled(false);
  video_->ClearRenderTargets(core::Color::kBlack);
  gbuffer_.BindForReading(5);                    // albedo=5, normal=6, specular=7
  gbuffer_.GetDepthRT()->BindAsSampler(8);       // depth=8 (position reconstruction)
  video_->SetBlendEnabled(true, abstract::BlendFactor::kOne, abstract::BlendFactor::kOne);
  LightRenderer::Instance().Render();
  LightRenderer::Instance().EndRender();
  video_->SetBlendEnabled(false);
  emissive_fbo_.UnbindForWriting();

  // 3. Emissive pass — additively draw emissive/ambient surfaces into the HDR RT.
  //    Depth is read-only (test on, write off) so emissive objects are occluded
  //    correctly by opaque geometry without corrupting G-buffer depth.
  emissive_fbo_.BindForWriting();
  video_->SetDepthTestEnabled(true);
  video_->SetBlendEnabled(true, abstract::BlendFactor::kOne, abstract::BlendFactor::kOne);
  MeshRenderer::Instance().RenderEmissive();
  MeshRenderer::Instance().EndRender();
  video_->SetBlendEnabled(false);
  video_->SetDepthWriteEnabled(true);
  video_->SetDepthTestEnabled(false);
  emissive_fbo_.UnbindForWriting();

  // 4. Composite pass — gamma-correct the HDR RT to the default framebuffer.
  emissive_fbo_.GetHDRRT()->BindAsSampler(0);
  composite_shader_->Activate();
  composite_quad_->Set();
  video_->RenderIndexed(composite_quad_->GetNumIndices());

  // Restore default depth state for the next BeginFrame.
  video_->SetDepthTestEnabled(true);
  video_->SetDepthWriteEnabled(true);
}

void Renderer::SetRenderableInfos(const core::Mat4f& world_matrix) {
  RenderableInfos ri;
  ri.world = world_matrix;
  renderable_infos_cb_->Fill(&ri);
}

void Renderer::OnResize(int w, int h) {
  gbuffer_.Resize(video_, w, h);
  emissive_fbo_.Resize(video_, w, h, gbuffer_.GetDepthRT());
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
  si.z_near_         = camera_->GetMinDepth();
  si.z_far_          = camera_->GetMaxDepth();

  scene_infos_cb_->Fill(&si);
}

}  // namespace renderer
