#include "renderer/Renderer.h"

#include "abstract/BlendFactor.h"
#include "abstract/BufferUsage.h"
#include "abstract/CompareFunc.h"
#include "core/BBox3.h"
#include "core/Color.h"
#include "core/ViewFrustum.h"
#include "environment/SkyRenderer.h"
#include "renderer/CSMInfos.h"
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
constexpr int kCSMInfosSlot           = 5;
constexpr int kCSMInfosFloat4s        = sizeof(CSMInfos) / 16;          // 272 / 16 = 17
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
  csm_infos_cb_ = video_->CreateConstantBuffer(
      kCSMInfosFloat4s, kCSMInfosSlot, abstract::BufferUsage::kDynamic);

  render_w_ = video_->GetWidth();
  render_h_ = video_->GetHeight();
  gbuffer_.Create(video_, render_w_, render_h_);
  emissive_fbo_.Create(video_, render_w_, render_h_, gbuffer_.GetDepthRT());

  new MeshRenderer(video_);
  new LightRenderer(video_);
  new ShadowRenderer(video_);

  shadow_debug_renderer_ = std::make_unique<ShadowDebugRenderer>(video_);

  InitVisibilitySystems(1000.f);
}

Renderer::~Renderer() {
  composite_shader_->Release();
  debug_shader_->Release();
  ShadowRenderer::Shutdown();
  LightRenderer::Shutdown();
  MeshRenderer::Shutdown();
}

void Renderer::InitVisibilitySystems(float world_size) {
  const float half = world_size * 0.5f;
  const core::BBox3 bounds(-half, -half, -half, half, half, half);
  const int depth = OctreeVisibilitySystem::ComputeNumLevels(world_size);
  no_culling_system_ = std::make_unique<NoCullingVisibilitySystem>(bounds);
  octree_system_     = std::make_unique<OctreeVisibilitySystem>(bounds, depth);
}

void Renderer::ClearVisibilitySystems() {
  no_culling_system_->Clear();
  octree_system_->Clear();
  ShadowRenderer::Instance().ClearShadowMaps();
}

void Renderer::AddRenderable(Renderable* r) {
  if (r->IsAlwaysVisible())
    no_culling_system_->AddRenderable(r);
  else
    octree_system_->AddRenderable(r);
}

void Renderer::RemoveRenderable(Renderable* r) {
  if (r->IsAlwaysVisible())
    no_culling_system_->RemoveRenderable(r);
  else
    octree_system_->RemoveRenderable(r);
}

void Renderer::SetCamera(const core::Camera* camera) {
  camera_ = camera;
  if (camera_) FillSceneInfos();
}

void Renderer::Update(float time, const core::Camera* camera,
                      abstract::RenderTargetGroup* output_fbo) {
  // Clear per-frame renderer lists before re-enqueuing so that the previous
  // frame's snapshots remain available during event callbacks (e.g. Tab →
  // CycleShadowDebug) that fire before this Update() is entered.
  LightRenderer::Instance().EndRender();
  MeshRenderer::Instance().EndRender();

  time_   = time;
  camera_ = camera;
  renderable_infos_cb_->Bind();
  scene_infos_cb_->Bind();
  material_infos_cb_->Bind();
  csm_infos_cb_->Bind();
  if (camera_) FillSceneInfos();

  if (camera_) {
    const core::ViewFrustum frustum(camera_->GetViewProjectionMatrix());
    no_culling_system_->CullAndEnqueue(frustum);
    octree_system_->CullAndEnqueue(frustum);
  }

  // 0. Shadow pass — render depth maps for spot lights and CSM cascades.
  if (camera_) {
    ShadowRenderer::Instance().RenderShadowMaps(
        LightRenderer::Instance().GetLights(),
        no_culling_system_.get(),
        octree_system_.get(),
        *camera_);
    if (ShadowRenderer::Instance().HasCSM())
      csm_infos_cb_->Fill(&ShadowRenderer::Instance().GetCSMInfos());
  }

  // 1. Geometry pass — fill albedo, normal, specular MRTs and depth+stencil.
  // Shadow passes set their own viewport; restore to the current render target size.
  video_->SetViewport(0, 0, render_w_, render_h_);
  gbuffer_.BindForWriting();
  video_->SetDepthTestEnabled(true);
  video_->SetDepthWriteEnabled(true);
  video_->ClearRenderTargets(core::Color::kBlack);
  MeshRenderer::Instance().PrepareRender();
  MeshRenderer::Instance().Render();
  if (terrain_renderer_ && terrain_renderer_->IsReady() && camera_)
    terrain_renderer_->Render(*camera_);
  if (foliage_enabled_ && FoliageRenderer::Instance().IsReady() && camera_)
    FoliageRenderer::Instance().Render(*camera_);
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
    if (output_fbo) output_fbo->BindForWriting();
    video_->RenderIndexed(composite_quad_->GetNumIndices());
    if (output_fbo) output_fbo->UnbindForWriting();
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
  video_->SetBlendEnabled(false);
  emissive_fbo_.UnbindForWriting();

  // 3. Sky pass — render procedural sky into the HDR RT before emissive geometry.
  //    Depth is read-only (LEQUAL) so the sky fills only background pixels
  //    (those whose G-buffer depth equals the far-plane value 1.0).
  if (sky_renderer_ && sky_renderer_->IsReady() && camera_) {
    emissive_fbo_.BindForWriting();
    sky_renderer_->Render(*camera_, sky_world_time_);
    emissive_fbo_.UnbindForWriting();
    video_->SetDepthTestEnabled(false);
    video_->SetDepthWriteEnabled(true);
  }

  // 4. Emissive pass — additively draw emissive/ambient surfaces into the HDR RT.
  //    Depth is read-only (test on, write off) so emissive objects are occluded
  //    correctly by opaque geometry without corrupting G-buffer depth.
  //    GL_LEQUAL is required: emissive geometry was already drawn at the same depth
  //    during the geometry pass, so GL_LESS would reject every fragment.
  emissive_fbo_.BindForWriting();
  video_->SetDepthTestEnabled(true);
  video_->SetDepthFunc(abstract::CompareFunc::kLessEqual);
  video_->SetBlendEnabled(true, abstract::BlendFactor::kOne, abstract::BlendFactor::kOne);
  MeshRenderer::Instance().RenderEmissive();
  MeshRenderer::Instance().EndRender();
  if (foliage_enabled_ && FoliageRenderer::Instance().IsReady() && camera_)
    FoliageRenderer::Instance().RenderBillboards(*camera_);
  video_->SetBlendEnabled(false);
  video_->SetDepthFunc(abstract::CompareFunc::kLess);
  video_->SetDepthWriteEnabled(true);
  video_->SetDepthTestEnabled(false);
  emissive_fbo_.UnbindForWriting();

  // 5. Composite pass — gamma-correct the HDR RT to the default framebuffer.
  emissive_fbo_.GetHDRRT()->BindAsSampler(0);
  composite_shader_->Activate();
  composite_quad_->Set();
  if (output_fbo) output_fbo->BindForWriting();
  video_->RenderIndexed(composite_quad_->GetNumIndices());
  if (output_fbo) output_fbo->UnbindForWriting();

  // 6. Shadow debug overlay — renders thumbnails on the left side of screen.
  shadow_debug_renderer_->Render(LightRenderer::Instance().GetLights());

  // Restore default depth state for the next BeginFrame.
  video_->SetDepthTestEnabled(true);
  video_->SetDepthWriteEnabled(true);
}

void Renderer::SetRenderableInfos(const core::Mat4f& world_matrix) {
  RenderableInfos ri;
  ri.world = world_matrix;
  renderable_infos_cb_->Fill(&ri);
}

void Renderer::CycleShadowDebug() {
  shadow_debug_renderer_->CycleNext(LightRenderer::Instance().GetLights());
}

void Renderer::OnResize(int w, int h) {
  render_w_ = w;
  render_h_ = h;
  gbuffer_.Resize(video_, w, h);
  emissive_fbo_.Resize(video_, w, h, gbuffer_.GetDepthRT());
}

void Renderer::ResizeTargets(int w, int h) {
  render_w_ = w;
  render_h_ = h;
  gbuffer_.Resize(video_, w, h);
  emissive_fbo_.Resize(video_, w, h, gbuffer_.GetDepthRT());
}

void Renderer::RenderTerrainWireframe(const core::Camera& camera,
                                       abstract::RenderTargetGroup* fbo) {
  if (terrain_renderer_ && terrain_renderer_->IsReady())
    terrain_renderer_->RenderWireframe(video_, camera, fbo);
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
