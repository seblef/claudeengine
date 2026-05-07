#include "renderer/Renderer.h"

#include "abstract/BufferUsage.h"
#include "renderer/RenderableInfos.h"
#include "renderer/SceneInfos.h"

namespace renderer {

namespace {
constexpr int kRenderableInfosSlot    = 1;
constexpr int kRenderableInfosFloat4s = sizeof(RenderableInfos) / 16;  // 64 / 16 = 4
constexpr int kSceneInfosSlot         = 2;
constexpr int kSceneInfosFloat4s      = sizeof(SceneInfos) / 16;        // 352 / 16 = 22
}  // namespace

Renderer::Renderer(abstract::VideoDevice* video) : video_(video) {
  renderable_infos_cb_ = video_->CreateConstantBuffer(
      kRenderableInfosFloat4s, kRenderableInfosSlot, abstract::BufferUsage::kDynamic);
  scene_infos_cb_ = video_->CreateConstantBuffer(
      kSceneInfosFloat4s, kSceneInfosSlot, abstract::BufferUsage::kDynamic);
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
  if (camera_) FillSceneInfos();
}

void Renderer::SetRenderableInfos(const core::Mat4f& world_matrix) {
  RenderableInfos ri;
  ri.world = world_matrix;
  renderable_infos_cb_->Fill(&ri);
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

}  // namespace renderer
