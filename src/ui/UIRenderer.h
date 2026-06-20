#pragma once

#include <memory>
#include <vector>

#include "abstract/ConstantBuffer.h"
#include "abstract/IndexBuffer.h"
#include "abstract/Shader.h"
#include "abstract/VertexBuffer.h"
#include "abstract/VideoDevice.h"
#include "core/Singleton.h"
#include "ui/UISprite.h"
#include "ui/UIText.h"

namespace ui {

// Singleton 2D overlay renderer.
//
// Renders UISprite and UIText elements over the 3D scene at the end of each
// frame, after the composite pass. Follows the same lifecycle pattern as
// BloomRenderer and EyeAdaptationRenderer.
//
// Constant buffer slot 4 is reserved for the orthographic projection matrix
// (pixel (0,0) = top-left → NDC (-1,+1), y-axis flipped).
//
// Render order: sprites sorted by layer_, then texts sorted by layer_.
// Depth test is off; alpha blending is SrcAlpha / OneMinusSrcAlpha.
class UIRenderer : public core::Singleton<UIRenderer> {
 public:
  // Allocates sprite and text shaders, the orthographic projection CB (slot 4),
  // and a shared dynamic VBO / static IBO for quad geometry.
  // video must outlive this UIRenderer.
  void Create(abstract::VideoDevice* video, int width, int height);

  // Releases all GPU resources (shaders, CB, VBO, IBO).
  void Destroy();

  // Recomputes and uploads the orthographic projection matrix for the new size.
  void OnResize(int width, int height);

  // Renders all sprites sorted by layer, then all texts sorted by layer.
  void Render(const std::vector<UISprite*>& sprites,
              const std::vector<UIText*>& texts);

 private:
  void UploadOrtho(int width, int height);
  void RenderSprites(const std::vector<UISprite*>& sprites);
  void RenderTexts(const std::vector<UIText*>& texts);

  // Maximum quads that fit in the shared VBO / IBO (4 verts, 6 indices each).
  // cppcheck-suppress unusedStructMember
  static constexpr int kMaxQuads = 1024;

  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice* video_ = nullptr;
  // cppcheck-suppress unusedStructMember
  abstract::Shader*      sprite_shader_ = nullptr;
  // cppcheck-suppress unusedStructMember
  abstract::Shader*      text_shader_   = nullptr;

  std::unique_ptr<abstract::ConstantBuffer> ortho_cb_;
  std::unique_ptr<abstract::VertexBuffer>   vertex_buf_;
  std::unique_ptr<abstract::IndexBuffer>    index_buf_;
};

}  // namespace ui
