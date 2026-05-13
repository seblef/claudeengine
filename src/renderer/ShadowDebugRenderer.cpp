#include "renderer/ShadowDebugRenderer.h"

#include "renderer/CSMInfos.h"
#include "renderer/GeometryUtils.h"
#include "renderer/Light.h"
#include "renderer/ShadowCubeMap.h"
#include "renderer/ShadowMap.h"
#include "renderer/ShadowRenderer.h"

namespace renderer {

ShadowDebugRenderer::ShadowDebugRenderer(abstract::VideoDevice* video)
    : video_(video),
      shader_2d_(video->CreateShader("shadow_debug_2d")),
      shader_cube_(video->CreateShader("shadow_debug_cube")),
      quad_(CreateQuad(video)) {}

ShadowDebugRenderer::~ShadowDebugRenderer() {
  shader_2d_->Release();
  shader_cube_->Release();
}

std::vector<ShadowDebugRenderer::Entry> ShadowDebugRenderer::CollectEntries(
    const std::vector<Light*>& lights) const {
  std::vector<Entry> entries;

  if (ShadowRenderer::Instance().HasCSM())
    entries.push_back({EntryType::kCSM, nullptr});

  for (const Light* l : lights) {
    if (l->GetType() == LightType::kGlobal) continue;
    if (l->GetType() == LightType::kOmni) {
      if (ShadowRenderer::Instance().GetShadowCubeMap(l))
        entries.push_back({EntryType::kOmni, l});
    } else {
      if (ShadowRenderer::Instance().GetShadowMap(l))
        entries.push_back({EntryType::kSpot, l});
    }
  }
  return entries;
}

void ShadowDebugRenderer::CycleNext(const std::vector<Light*>& lights) {
  const int total = static_cast<int>(CollectEntries(lights).size());
  current_index_  = (total == 0) ? 0 : (current_index_ + 1) % (total + 1);
}

void ShadowDebugRenderer::Render(const std::vector<Light*>& lights) {
  if (current_index_ == 0) return;

  const std::vector<Entry> entries = CollectEntries(lights);
  if (current_index_ > static_cast<int>(entries.size())) {
    current_index_ = 0;
    return;
  }
  const Entry& e = entries[current_index_ - 1];

  video_->SetDepthTestEnabled(false);
  video_->SetDepthWriteEnabled(false);
  quad_->Set();

  const float tw = TileW();
  const float th = TileH();

  switch (e.type) {
    case EntryType::kCSM:
      shader_2d_->Activate();
      for (int i = 0; i < kCSMCascadeCount; ++i) {
        const ShadowMap* sm = ShadowRenderer::Instance().GetCascadeMap(i);
        if (!sm) continue;
        const float y1 = 1.0f - static_cast<float>(i) * th;
        sm->GetDepthRT()->EnableCompareMode(false);
        sm->GetDepthRT()->BindAsSampler(0);
        shader_2d_->SetUniform2f("u_tile_min", -1.0f, y1 - th);
        shader_2d_->SetUniform2f("u_tile_max", -1.0f + tw, y1);
        video_->RenderIndexed(quad_->GetNumIndices());
        sm->GetDepthRT()->EnableCompareMode(true);
      }
      break;

    case EntryType::kSpot: {
      const ShadowMap* sm = ShadowRenderer::Instance().GetShadowMap(e.light);
      if (sm) {
        shader_2d_->Activate();
        sm->GetDepthRT()->EnableCompareMode(false);
        sm->GetDepthRT()->BindAsSampler(0);
        shader_2d_->SetUniform2f("u_tile_min", -1.0f, 1.0f - th);
        shader_2d_->SetUniform2f("u_tile_max", -1.0f + tw, 1.0f);
        video_->RenderIndexed(quad_->GetNumIndices());
        sm->GetDepthRT()->EnableCompareMode(true);
      }
      break;
    }

    case EntryType::kOmni: {
      const ShadowCubeMap* scm =
          ShadowRenderer::Instance().GetShadowCubeMap(e.light);
      if (scm) {
        shader_cube_->Activate();
        abstract::RenderTargetCube* rt = scm->GetCubeRT();
        rt->EnableCompareMode(false);
        rt->BindAsSampler(0);
        // 2-column × 3-row grid: face 0..5 left-to-right, top-to-bottom.
        for (int face = 0; face < 6; ++face) {
          const int   col = face % 2;
          const int   row = face / 2;
          const float x0  = -1.0f + static_cast<float>(col) * tw;
          const float y1  = 1.0f  - static_cast<float>(row) * th;
          shader_cube_->SetUniform2f("u_tile_min", x0, y1 - th);
          shader_cube_->SetUniform2f("u_tile_max", x0 + tw, y1);
          shader_cube_->SetUniformInt("u_face", face);
          video_->RenderIndexed(quad_->GetNumIndices());
        }
        rt->EnableCompareMode(true);
      }
      break;
    }
  }
}

float ShadowDebugRenderer::TileW() const {
  return 400.0f / static_cast<float>(video_->GetWidth());
}

float ShadowDebugRenderer::TileH() const {
  return 400.0f / static_cast<float>(video_->GetHeight());
}

void ShadowDebugRenderer::RenderTile2D(abstract::RenderTarget* rt,
                                        float x0, float y0, float x1, float y1) {
  rt->EnableCompareMode(false);
  rt->BindAsSampler(0);
  shader_2d_->SetUniform2f("u_tile_min", x0, y0);
  shader_2d_->SetUniform2f("u_tile_max", x1, y1);
  video_->RenderIndexed(quad_->GetNumIndices());
  rt->EnableCompareMode(true);
}

void ShadowDebugRenderer::RenderTileCube(abstract::RenderTargetCube* rt, int face,
                                          float x0, float y0, float x1, float y1) {
  rt->EnableCompareMode(false);
  rt->BindAsSampler(0);
  shader_cube_->SetUniform2f("u_tile_min", x0, y0);
  shader_cube_->SetUniform2f("u_tile_max", x1, y1);
  shader_cube_->SetUniformInt("u_face", face);
  video_->RenderIndexed(quad_->GetNumIndices());
  rt->EnableCompareMode(true);
}

}  // namespace renderer
