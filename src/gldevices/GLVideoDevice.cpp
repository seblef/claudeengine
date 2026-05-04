#include "gldevices/GLVideoDevice.h"

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

std::unique_ptr<abstract::VertexBuffer> GLVideoDevice::CreateVertexBuffer(
    core::VertexType vertex_type, int num_vertices,
    abstract::BufferUsage usage, const void* data, int offset) {
  auto vb = std::make_unique<GLVertexBuffer>(vertex_type, num_vertices, usage);
  if (data) vb->Fill(data, num_vertices, offset);
  return vb;
}

abstract::Shader* GLVideoDevice::CreateShader(const std::string& /*name*/) {
  return nullptr;
}

}  // namespace gldevices
