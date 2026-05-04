// Must be defined before any GL/gl.h include to expose extension prototypes.
#define GL_GLEXT_PROTOTYPES

#include "gldevices/GLVideoDevice.h"

#include "gldevices/GLGeometryBuffer.h"
#include "gldevices/GLIndexBuffer.h"
#include "gldevices/GLShader.h"
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

}  // namespace gldevices
