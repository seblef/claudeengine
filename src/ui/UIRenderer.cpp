#include "ui/UIRenderer.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "abstract/BlendFactor.h"
#include "abstract/BufferUsage.h"
#include "abstract/IndexType.h"
#include "core/Color.h"
#include "core/Mat4f.h"
#include "core/Vec2f.h"
#include "core/Vec3f.h"
#include "core/Vertex2D.h"
#include "core/VertexType.h"

namespace ui {

namespace {

// Orthographic matrix: pixel (0,0) = top-left → NDC (-1,+1).
//   x_ndc = 2*px/W - 1
//   y_ndc = 1 - 2*py/H  (y flipped so y=0 maps to NDC top)
core::Mat4f MakeOrtho(int width, int height) {
  const float w = static_cast<float>(width);
  const float h = static_cast<float>(height);
  return core::Mat4f(
       2.f / w,      0.f,  0.f, -1.f,
          0.f, -2.f / h,  0.f,  1.f,
          0.f,      0.f,  1.f,  0.f,
          0.f,      0.f,  0.f,  1.f);
}

// GPU struct uploaded into CB slot 4 (must match ui_projection_infos.glsl).
struct UIProjectionInfos {
  core::Mat4f ortho;
};
constexpr int kUIProjectionFloat4s =
    static_cast<int>(sizeof(UIProjectionInfos) / 16);  // 4

}  // namespace

void UIRenderer::Create(abstract::VideoDevice* video, int width, int height) {
  video_         = video;
  sprite_shader_ = video_->CreateShader("ui_sprite");
  text_shader_   = video_->CreateShader("ui_text");

  constexpr int kOrthoSlot = 4;
  ortho_cb_ = video_->CreateConstantBuffer(
      kUIProjectionFloat4s, kOrthoSlot, abstract::BufferUsage::kDynamic);

  // Dynamic VBO: kMaxQuads quads × 4 vertices each.
  vertex_buf_ = video_->CreateVertexBuffer(
      core::VertexType::k2D, kMaxQuads * 4, abstract::BufferUsage::kDynamic);

  // Static IBO: pre-fill with repeating quad index pattern (0,1,2, 0,2,3 per quad).
  std::vector<uint16_t> indices;
  indices.reserve(kMaxQuads * 6);
  for (int q = 0; q < kMaxQuads; ++q) {
    const uint16_t base = static_cast<uint16_t>(q * 4);
    indices.push_back(base);
    indices.push_back(static_cast<uint16_t>(base + 1));
    indices.push_back(static_cast<uint16_t>(base + 2));
    indices.push_back(base);
    indices.push_back(static_cast<uint16_t>(base + 2));
    indices.push_back(static_cast<uint16_t>(base + 3));
  }
  index_buf_ = video_->CreateIndexBuffer(
      abstract::IndexType::kUInt16, kMaxQuads * 6,
      abstract::BufferUsage::kImmutable, indices.data());

  UploadOrtho(width, height);
}

void UIRenderer::Destroy() {
  index_buf_.reset();
  vertex_buf_.reset();
  ortho_cb_.reset();
  if (text_shader_) {
    text_shader_->Release();
    text_shader_ = nullptr;
  }
  if (sprite_shader_) {
    sprite_shader_->Release();
    sprite_shader_ = nullptr;
  }
  video_ = nullptr;
}

void UIRenderer::OnResize(int width, int height) {
  UploadOrtho(width, height);
}

void UIRenderer::RemoveUISprite(UISprite* s) {
  sprites_.erase(std::remove(sprites_.begin(), sprites_.end(), s),
                 sprites_.end());
}

void UIRenderer::RemoveUIText(UIText* t) {
  texts_.erase(std::remove(texts_.begin(), texts_.end(), t),
               texts_.end());
}

void UIRenderer::Render() {
  if (sprites_.empty() && texts_.empty()) return;

  video_->SetDepthTestEnabled(false);
  video_->SetDepthWriteEnabled(false);
  video_->SetBlendEnabled(true,
                          abstract::BlendFactor::kSrcAlpha,
                          abstract::BlendFactor::kOneMinusSrcAlpha);
  ortho_cb_->Bind();
  video_->SetIndexType(abstract::IndexType::kUInt16);

  RenderSprites();
  RenderTexts();

  video_->SetBlendEnabled(false);
  video_->SetDepthTestEnabled(true);
  video_->SetDepthWriteEnabled(true);
}

void UIRenderer::UploadOrtho(int width, int height) {
  UIProjectionInfos info;
  info.ortho = MakeOrtho(width, height);
  ortho_cb_->Bind();
  ortho_cb_->Fill(&info);
}

void UIRenderer::RenderSprites() {
  if (sprites_.empty()) return;

  std::vector<UISprite*> sorted = sprites_;
  std::sort(sorted.begin(), sorted.end(),
            [](const UISprite* a, const UISprite* b) {
              return a->GetLayer() < b->GetLayer();
            });

  sprite_shader_->Activate();
  vertex_buf_->Bind();
  index_buf_->Bind();

  for (const UISprite* s : sorted) {
    const core::Rect& r = s->GetRect();
    const core::Vec4f& t = s->GetTint();
    const core::Color  tint(t.x, t.y, t.z, t.w);

    const float x0 = r.GetX();
    const float y0 = r.GetY();
    const float x1 = x0 + r.GetWidth();
    const float y1 = y0 + r.GetHeight();

    // TL, TR, BR, BL — indices 0,1,2 and 0,2,3 form two triangles.
    const core::Vertex2D verts[4] = {
      core::Vertex2D({x0, y0, 0.f}, tint, {0.f, 0.f}),
      core::Vertex2D({x1, y0, 0.f}, tint, {1.f, 0.f}),
      core::Vertex2D({x1, y1, 0.f}, tint, {1.f, 1.f}),
      core::Vertex2D({x0, y1, 0.f}, tint, {0.f, 1.f}),
    };
    vertex_buf_->Fill(verts, 4);
    if (s->GetTexture()) s->GetTexture()->Bind(0);
    else if (s->GetRawTexture()) s->GetRawTexture()->Bind(0);
    video_->RenderIndexed(6);
  }
}

void UIRenderer::RenderTexts() {
  if (texts_.empty()) return;

  std::vector<UIText*> sorted = texts_;
  std::sort(sorted.begin(), sorted.end(),
            [](const UIText* a, const UIText* b) {
              return a->GetLayer() < b->GetLayer();
            });

  text_shader_->Activate();
  vertex_buf_->Bind();
  index_buf_->Bind();

  std::vector<core::Vertex2D> verts;
  verts.reserve(kMaxQuads * 4);

  for (const UIText* t : sorted) {
    const FontAtlas* atlas = t->GetAtlas();
    if (!atlas || !atlas->GetTexture()) continue;

    const char*        str   = t->GetText();
    const core::Vec2f  pos   = t->GetPosition();
    const float        scale = t->GetSize();
    const core::Vec4f& c     = t->GetColor();
    const core::Color  color(c.x, c.y, c.z, c.w);
    const float        line_h =
        static_cast<float>(atlas->GetLineHeight()) * scale;

    verts.clear();
    float pen_x = pos.x;
    float pen_y = pos.y;

    for (const char* p = str; *p != '\0'; ++p) {
      if (*p == '\n') {
        pen_x = pos.x;
        pen_y += line_h;
        continue;
      }
      const GlyphInfo* g = atlas->GetGlyph(*p);
      if (!g) continue;
      if (static_cast<int>(verts.size()) / 4 >= kMaxQuads) break;

      // Glyph quad in screen pixels (x_bearing/y_bearing from stbtt).
      const float gx0 = pen_x + static_cast<float>(g->x_bearing) * scale;
      const float gy0 = pen_y + static_cast<float>(g->y_bearing) * scale;
      const float gx1 = gx0 + static_cast<float>(g->width)       * scale;
      const float gy1 = gy0 + static_cast<float>(g->height)      * scale;

      verts.push_back(core::Vertex2D({gx0, gy0, 0.f}, color, {g->uv_x0, g->uv_y0}));
      verts.push_back(core::Vertex2D({gx1, gy0, 0.f}, color, {g->uv_x1, g->uv_y0}));
      verts.push_back(core::Vertex2D({gx1, gy1, 0.f}, color, {g->uv_x1, g->uv_y1}));
      verts.push_back(core::Vertex2D({gx0, gy1, 0.f}, color, {g->uv_x0, g->uv_y1}));

      pen_x += static_cast<float>(g->advance) * scale;
    }

    const int n_quads = static_cast<int>(verts.size()) / 4;
    if (n_quads == 0) continue;

    vertex_buf_->Fill(verts.data(), n_quads * 4);
    atlas->GetTexture()->Bind(0);
    video_->RenderIndexed(n_quads * 6);
  }
}

}  // namespace ui
