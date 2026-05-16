#include "renderer/PreviewRenderer.h"

#include "abstract/BlendFactor.h"
#include "abstract/BufferUsage.h"
#include "abstract/CompareFunc.h"
#include "core/Color.h"
#include "renderer/GeometryUtils.h"
#include "renderer/GlobalLight.h"
#include "renderer/LightRenderer.h"
#include "renderer/MeshInstance.h"
#include "renderer/MeshRenderer.h"
#include "renderer/Renderer.h"
#include "renderer/SceneInfos.h"

namespace renderer {

namespace {
constexpr int kSceneInfosSlot    = 2;
constexpr int kSceneInfosFloat4s = sizeof(SceneInfos) / 16;
}  // namespace

PreviewRenderer::PreviewRenderer(abstract::VideoDevice* video, int width, int height)
    : video_(video),
      w_(width),
      h_(height),
      composite_shader_(video->CreateShader("composite")),
      composite_quad_(CreateQuad(video)) {
  scene_infos_cb_ = video_->CreateConstantBuffer(
      kSceneInfosFloat4s, kSceneInfosSlot, abstract::BufferUsage::kDynamic);
  gbuffer_.Create(video_, w_, h_);
  emissive_fbo_.Create(video_, w_, h_, gbuffer_.GetDepthRT());
}

PreviewRenderer::~PreviewRenderer() {
  composite_shader_->Release();
}

void PreviewRenderer::Render(float time, const core::Camera& camera,
                              MeshInstance* mesh_instance, GlobalLight* light,
                              abstract::RenderTargetGroup* output_rtg) {
  // Clear any stale lists left from the previous main frame.
  MeshRenderer::Instance().EndRender();
  LightRenderer::Instance().EndRender();

  // Bind preview scene CB (slot 2) with preview camera matrices.
  FillSceneInfos(camera, time);
  scene_infos_cb_->Bind();

  video_->SetViewport(0, 0, w_, h_);

  // 1. Geometry pass — fill the preview G-buffer.
  gbuffer_.BindForWriting();
  video_->SetDepthTestEnabled(true);
  video_->SetDepthWriteEnabled(true);
  video_->ClearRenderTargets(core::Color::kBlack);
  mesh_instance->Enqueue();
  MeshRenderer::Instance().PrepareRender();
  MeshRenderer::Instance().Render();
  gbuffer_.UnbindForWriting();

  // 2. Lighting pass — shade into the HDR RT using G-buffer samplers.
  emissive_fbo_.BindForWriting();
  video_->SetDepthWriteEnabled(false);
  video_->ClearRenderTargets(core::Color::kBlack);
  gbuffer_.BindForReading(5);           // albedo=5, normal=6, specular=7
  gbuffer_.GetDepthRT()->BindAsSampler(8);
  video_->SetBlendEnabled(true, abstract::BlendFactor::kOne,
                          abstract::BlendFactor::kOne);
  light->Enqueue();
  LightRenderer::Instance().Render(/*disable_shadows=*/true);
  video_->SetBlendEnabled(false);
  emissive_fbo_.UnbindForWriting();

  // 3. Emissive pass — additively draw emissive surfaces into the HDR RT.
  emissive_fbo_.BindForWriting();
  video_->SetDepthTestEnabled(true);
  video_->SetDepthFunc(abstract::CompareFunc::kLessEqual);
  video_->SetBlendEnabled(true, abstract::BlendFactor::kOne,
                          abstract::BlendFactor::kOne);
  MeshRenderer::Instance().RenderEmissive();
  MeshRenderer::Instance().EndRender();
  video_->SetBlendEnabled(false);
  video_->SetDepthFunc(abstract::CompareFunc::kLess);
  video_->SetDepthWriteEnabled(true);
  video_->SetDepthTestEnabled(false);
  emissive_fbo_.UnbindForWriting();
  LightRenderer::Instance().EndRender();

  // 4. Composite pass — gamma-correct the HDR RT into the output RTG.
  emissive_fbo_.GetHDRRT()->BindAsSampler(0);
  composite_shader_->Activate();
  composite_quad_->Set();
  if (output_rtg) output_rtg->BindForWriting();
  video_->RenderIndexed(composite_quad_->GetNumIndices());
  if (output_rtg) output_rtg->UnbindForWriting();

  video_->SetDepthTestEnabled(true);
  video_->SetDepthWriteEnabled(true);
}

void PreviewRenderer::FillSceneInfos(const core::Camera& camera, float time) {
  const core::Vec2f sc = camera.GetScreenCenter();

  SceneInfos si;
  si.view_proj       = camera.GetViewProjectionMatrix();
  si.inv_view_proj   = camera.GetViewProjectionMatrix().Inverse();
  si.inv_proj        = camera.GetProjectionMatrix().Inverse();
  si.proj            = camera.GetProjectionMatrix();
  si.view            = camera.GetViewMatrix();
  si.eye_pos         = camera.GetPosition();
  si.time            = time;
  si.pad0_           = 0.f;
  si.inv_screen_size = {1.f / (2.f * sc.x), 1.f / (2.f * sc.y)};
  si.z_near_         = camera.GetMinDepth();
  si.z_far_          = camera.GetMaxDepth();

  scene_infos_cb_->Fill(&si);
}

}  // namespace renderer
