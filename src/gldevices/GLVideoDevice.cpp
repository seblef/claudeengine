// Must be defined before any GL/gl.h include to expose extension prototypes.
#define GL_GLEXT_PROTOTYPES

#include "gldevices/GLVideoDevice.h"

#include "gldevices/GLConstantBuffer.h"
#include "gldevices/GLGeometryBuffer.h"
#include "gldevices/GLIndexBuffer.h"
#include "gldevices/GLRenderTarget.h"
#include "gldevices/GLRenderTargetCube.h"
#include "gldevices/GLRenderTargetGroup.h"
#include "gldevices/GLShader.h"
#include "gldevices/GLTexture.h"
#include "gldevices/GLVertexBuffer.h"

#include <GLFW/glfw3.h>

namespace gldevices {

GLVideoDevice::GLVideoDevice(int width, int height, bool windowed, GLFWwindow* window)
    : abstract::VideoDevice(width, height, windowed), window_(window) {}

void GLVideoDevice::BeginFrame() {
  glfwSwapBuffers(window_);
  glViewport(0, 0, width_, height_);
}

void GLVideoDevice::ClearRenderTargets(const core::Color& color) {
  glClearColor(color.r, color.g, color.b, color.a);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GLVideoDevice::OnResize(int width, int height) {
  width_  = width;
  height_ = height;
  glViewport(0, 0, width_, height_);
}

void GLVideoDevice::SetDepthTestEnabled(bool enabled) {
  if (enabled) {
    glEnable(GL_DEPTH_TEST);
  } else {
    glDisable(GL_DEPTH_TEST);
  }
}

void GLVideoDevice::SetPrimitiveType(abstract::PrimitiveType type) {
  switch (type) {
    case abstract::PrimitiveType::kPointList:     primitive_type_ = GL_POINTS;         break;
    case abstract::PrimitiveType::kLineList:      primitive_type_ = GL_LINES;          break;
    case abstract::PrimitiveType::kLineStrip:     primitive_type_ = GL_LINE_STRIP;     break;
    case abstract::PrimitiveType::kTriangleList:  primitive_type_ = GL_TRIANGLES;      break;
    case abstract::PrimitiveType::kTriangleStrip: primitive_type_ = GL_TRIANGLE_STRIP; break;
    case abstract::PrimitiveType::kTriangleFan:   primitive_type_ = GL_TRIANGLE_FAN;   break;
  }
}

void GLVideoDevice::Render(int num_vertices, int first_vertex) {
  glDrawArrays(primitive_type_, first_vertex, num_vertices);
}

void GLVideoDevice::SetIndexType(abstract::IndexType type) {
  current_index_type_ = (type == abstract::IndexType::kUInt16)
                        ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
}

void GLVideoDevice::RenderIndexed(int num_indices, int first_index, int vertex_offset) {
  const uintptr_t index_size = (current_index_type_ == GL_UNSIGNED_SHORT) ? 2 : 4;
  glDrawElementsBaseVertex(
      primitive_type_, num_indices, current_index_type_,
      reinterpret_cast<const void*>(first_index * index_size),
      vertex_offset);
}

std::unique_ptr<abstract::VertexBuffer> GLVideoDevice::CreateVertexBuffer(
    core::VertexType vertex_type, int num_vertices,
    abstract::BufferUsage usage, const void* data, int offset) {
  auto vb = std::make_unique<GLVertexBuffer>(vertex_type, num_vertices, usage);
  if (data) vb->Fill(data, num_vertices, offset);
  return vb;
}

std::unique_ptr<abstract::IndexBuffer> GLVideoDevice::CreateIndexBuffer(
    abstract::IndexType type, int num_indices,
    abstract::BufferUsage usage, const void* data, int offset) {
  auto ib = std::make_unique<GLIndexBuffer>(type, num_indices, usage);
  if (data) ib->Fill(data, num_indices, offset);
  return ib;
}

std::unique_ptr<abstract::GeometryBuffer> GLVideoDevice::CreateGeometryBuffer(
    core::VertexType vertex_type, int num_vertices,
    abstract::IndexType index_type, int num_indices,
    abstract::BufferUsage usage,
    const void* vertex_data, int vertex_offset,
    const void* index_data, int index_offset) {
  auto gb = std::make_unique<GLGeometryBuffer>(vertex_type, num_vertices,
                                               index_type, num_indices, usage);
  if (vertex_data) gb->FillVertices(vertex_data, num_vertices, vertex_offset);
  if (index_data)  gb->FillIndices(index_data, num_indices, index_offset);
  return gb;
}

std::unique_ptr<abstract::ConstantBuffer> GLVideoDevice::CreateConstantBuffer(
    int size, int slot, abstract::BufferUsage usage, const void* data) {
  auto cb = std::make_unique<GLConstantBuffer>(size, slot, usage);
  if (data) cb->Fill(data);
  return cb;
}

abstract::Shader* GLVideoDevice::CreateShader(const std::string& name) {
  auto* existing = abstract::Shader::Get(name);
  if (existing) {
    existing->AddRef();
    return existing;
  }
  auto* shader = new GLShader(name);
  if (!shader->IsInitialized()) {
    shader->Release();
    return nullptr;
  }
  return shader;
}

abstract::Texture* GLVideoDevice::CreateTexture(
    const std::string& name, abstract::BufferUsage usage) {
  auto* existing = abstract::Texture::Get(name);
  if (existing) {
    existing->AddRef();
    return existing;
  }
  auto* tex = new GLTexture(name, usage);
  if (!tex->IsInitialized()) {
    tex->Release();
    return nullptr;
  }
  return tex;
}

void GLVideoDevice::SetColorWriteEnabled(bool enabled) {
  const GLboolean m = enabled ? GL_TRUE : GL_FALSE;
  glColorMask(m, m, m, m);
}

void GLVideoDevice::SetFaceCulling(abstract::CullFace mode) {
  if (mode == abstract::CullFace::kNone) {
    glDisable(GL_CULL_FACE);
    return;
  }
  glEnable(GL_CULL_FACE);
  glCullFace(mode == abstract::CullFace::kFront ? GL_FRONT : GL_BACK);
}

void GLVideoDevice::SetStencilTestEnabled(bool enabled) {
  if (enabled) {
    glEnable(GL_STENCIL_TEST);
  } else {
    glDisable(GL_STENCIL_TEST);
  }
}

namespace {

GLenum ToGLCompareFunc(abstract::CompareFunc func) {
  switch (func) {
    case abstract::CompareFunc::kAlways:       return GL_ALWAYS;
    case abstract::CompareFunc::kNever:        return GL_NEVER;
    case abstract::CompareFunc::kEqual:        return GL_EQUAL;
    case abstract::CompareFunc::kNotEqual:     return GL_NOTEQUAL;
    case abstract::CompareFunc::kLess:         return GL_LESS;
    case abstract::CompareFunc::kGreater:      return GL_GREATER;
    case abstract::CompareFunc::kLessEqual:    return GL_LEQUAL;
    case abstract::CompareFunc::kGreaterEqual: return GL_GEQUAL;
  }
  return GL_ALWAYS;
}

GLenum ToGLStencilOp(abstract::StencilOp op) {
  switch (op) {
    case abstract::StencilOp::kKeep:     return GL_KEEP;
    case abstract::StencilOp::kZero:     return GL_ZERO;
    case abstract::StencilOp::kIncrWrap: return GL_INCR_WRAP;
    case abstract::StencilOp::kDecrWrap: return GL_DECR_WRAP;
  }
  return GL_KEEP;
}

}  // namespace

void GLVideoDevice::SetDepthFunc(abstract::CompareFunc func) {
  glDepthFunc(ToGLCompareFunc(func));
}

void GLVideoDevice::SetStencilFunc(abstract::CompareFunc func, int ref,
                                   unsigned mask) {
  glStencilFunc(ToGLCompareFunc(func), ref, mask);
}

void GLVideoDevice::SetStencilOp(abstract::Face face,
                                 abstract::StencilOp sfail,
                                 abstract::StencilOp dpfail,
                                 abstract::StencilOp dppass) {
  const GLenum gl_face = (face == abstract::Face::kFront) ? GL_FRONT : GL_BACK;
  glStencilOpSeparate(gl_face, ToGLStencilOp(sfail),
                      ToGLStencilOp(dpfail), ToGLStencilOp(dppass));
}

void GLVideoDevice::ClearStencil(int val) {
  glClearStencil(val);
  glClear(GL_STENCIL_BUFFER_BIT);
}

namespace {

GLenum ToGLBlendFactor(abstract::BlendFactor f) {
  switch (f) {
    case abstract::BlendFactor::kZero:             return GL_ZERO;
    case abstract::BlendFactor::kOne:              return GL_ONE;
    case abstract::BlendFactor::kSrcAlpha:         return GL_SRC_ALPHA;
    case abstract::BlendFactor::kOneMinusSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
  }
  return GL_ONE;
}

}  // namespace

void GLVideoDevice::SetBlendEnabled(bool enabled,
                                    abstract::BlendFactor src,
                                    abstract::BlendFactor dst) {
  if (enabled) {
    glEnable(GL_BLEND);
    glBlendFunc(ToGLBlendFactor(src), ToGLBlendFactor(dst));
  } else {
    glDisable(GL_BLEND);
  }
}

void GLVideoDevice::SetDepthWriteEnabled(bool enabled) {
  glDepthMask(enabled ? GL_TRUE : GL_FALSE);
}

void GLVideoDevice::SetViewport(int x, int y, int width, int height) {
  glViewport(x, y, width, height);
}

std::unique_ptr<abstract::RenderTarget> GLVideoDevice::CreateRenderTarget(
    int width, int height, abstract::TextureFormat format) {
  return std::make_unique<GLRenderTarget>(width, height, format);
}

std::unique_ptr<abstract::RenderTargetGroup>
GLVideoDevice::CreateRenderTargetGroup(
    std::span<abstract::RenderTarget*> color_targets,
    abstract::RenderTarget* depth_stencil_target) {
  return std::make_unique<GLRenderTargetGroup>(color_targets,
                                               depth_stencil_target);
}

std::unique_ptr<abstract::RenderTargetCube> GLVideoDevice::CreateRenderTargetCube(
    int size, abstract::TextureFormat /*format*/) {
  return std::make_unique<GLRenderTargetCube>(size);
}

}  // namespace gldevices
