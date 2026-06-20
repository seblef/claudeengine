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

// Singleton 2D overlay renderer — independent of the 3D Renderer.
//
// Owns its registered UISprite and UIText lists. The caller (e.g. the game
// system) is responsible for calling Render() after 3D rendering completes,
// rendering to whichever framebuffer is currently bound.
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

  // Registers a sprite. The caller retains ownership; the pointer must remain
  // valid on every subsequent Render() call.
  void AddUISprite(UISprite* s) { sprites_.push_back(s); }

  // Unregisters a sprite.
  void RemoveUISprite(UISprite* s);

  // Registers a text element. The caller retains ownership; the pointer must
  // remain valid on every subsequent Render() call.
  void AddUIText(UIText* t) { texts_.push_back(t); }

  // Unregisters a text element.
  void RemoveUIText(UIText* t);

  // Renders all registered sprites sorted by layer, then all texts sorted by layer.
  void Render();

 private:
  void UploadOrtho(int width, int height);
  void RenderSprites();
  void RenderTexts();

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

  // cppcheck-suppress unusedStructMember
  std::vector<UISprite*> sprites_;
  // cppcheck-suppress unusedStructMember
  std::vector<UIText*>   texts_;
};

}  // namespace ui
