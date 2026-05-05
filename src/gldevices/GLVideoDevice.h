#pragma once

#include <memory>
#include <string>

#include <GL/gl.h>

#include "abstract/VideoDevice.h"

struct GLFWwindow;

namespace gldevices {

// Concrete VideoDevice implementation backed by OpenGL.
//
// The constructor takes the GLFW window handle (created by GLDevices) so it
// can call glfwSwapBuffers at the start of each frame.
class GLVideoDevice : public abstract::VideoDevice {
 public:
  GLVideoDevice(int width, int height, bool windowed, GLFWwindow* window);
  ~GLVideoDevice() override = default;

  // Presents the previous frame and resets the viewport for the new one.
  void BeginFrame() override;

  // Clears colour and depth buffers to the given background colour.
  void ClearRenderTargets(const core::Color& color) override;

  // Updates stored resolution and adjusts the OpenGL viewport.
  void OnResize(int width, int height) override;

  // Stores the GL primitive topology for subsequent Render() and RenderIndexed() calls.
  void SetPrimitiveType(abstract::PrimitiveType type) override;

  // Stores the GL index element type for subsequent RenderIndexed() calls.
  void SetIndexType(abstract::IndexType type) override;

  // Issues a glDrawArrays call using the current primitive type.
  void Render(int num_vertices, int first_vertex = 0) override;

  // Issues a glDrawElementsBaseVertex call using the current primitive and index types.
  void RenderIndexed(int num_indices, int first_index = 0,
                     int vertex_offset = 0) override;

  // Creates a GLVertexBuffer with the given layout and fills it if data is non-null.
  [[nodiscard]] std::unique_ptr<abstract::VertexBuffer> CreateVertexBuffer(
      core::VertexType vertex_type, int num_vertices, abstract::BufferUsage usage,
      const void* data = nullptr, int offset = 0) override;

  // Creates a GLIndexBuffer with the given type and fills it if data is non-null.
  [[nodiscard]] std::unique_ptr<abstract::IndexBuffer> CreateIndexBuffer(
      abstract::IndexType type, int num_indices, abstract::BufferUsage usage,
      const void* data = nullptr, int offset = 0) override;

  // Creates a GLGeometryBuffer and fills it if vertex/index data are non-null.
  [[nodiscard]] std::unique_ptr<abstract::GeometryBuffer> CreateGeometryBuffer(
      core::VertexType vertex_type, int num_vertices,
      abstract::IndexType index_type, int num_indices,
      abstract::BufferUsage usage,
      const void* vertex_data = nullptr, int vertex_offset = 0,
      const void* index_data = nullptr, int index_offset = 0) override;

  // Creates a GLConstantBuffer of size float4 elements at the given slot.
  [[nodiscard]] std::unique_ptr<abstract::ConstantBuffer> CreateConstantBuffer(
      int size, int slot,
      abstract::BufferUsage usage = abstract::BufferUsage::kDynamic,
      const void* data = nullptr) override;

  // Creates (or retrieves from the registry) a GLShader by name.
  // Loads <name>_vs.glsl and <name>_ps.glsl from the configured data folder.
  [[nodiscard]] abstract::Shader* CreateShader(const std::string& name) override;

 private:
  GLFWwindow* window_;
  GLenum primitive_type_    = GL_TRIANGLES;
  GLenum current_index_type_ = GL_UNSIGNED_INT;
};

}  // namespace gldevices
