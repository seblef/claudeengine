#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "abstract/Shader.h"
#include "abstract/VideoDevice.h"

namespace renderer {

// Templated batch renderer for a homogeneous set of renderable instances.
//
// T must provide GetModel() returning a pointer to a type that exposes
// GetMaterial(). Both are used as sort keys in PrepareRender() to minimise
// texture and geometry switching across draw calls.
//
// Depth rendering (shadow pass):
//   AddDepthInstance() queues an instance for depth-only rendering.
//   RenderDepth() (pure virtual) must sort by geometry, activate the
//   depth-only shader loaded by the subclass, render, and clear the queue.
//   Each concrete subclass sets depth_shader_ in its constructor.
template <typename T>
class ObjectRenderer {
 public:
  // Loads the named shader via video. video must outlive this renderer.
  ObjectRenderer(const std::string& shader_name, abstract::VideoDevice* video);
  virtual ~ObjectRenderer();

  ObjectRenderer(const ObjectRenderer&)            = delete;
  ObjectRenderer& operator=(const ObjectRenderer&) = delete;

  // Enqueues instance for rendering in the current frame.
  void AddInstance(T* instance);

  // Sorts enqueued instances: primary key = material pointer, secondary key =
  // model pointer. Groups objects that share textures and geometry together,
  // reducing GPU state-switch cost. Called by the global renderer before draw.
  void PrepareRender();

  // Issues draw calls for the sorted instance list. Implemented by concrete
  // subclasses (e.g. MeshRenderer), which bind geometry and call RenderIndexed
  // per instance.
  virtual void Render() = 0;

  // Additively draws emissive instances into the HDR render target.
  // Default is a no-op; override in subclasses that contribute to the emissive
  // pass. Must be called after Render() and before EndRender().
  virtual void RenderEmissive() {}

  // Enqueues instance for depth-only rendering in the current shadow pass.
  void AddDepthInstance(T* instance);

  // Renders all depth-queued instances using the depth-only shader, sorted by
  // geometry to minimise VAO switching.  Clears the depth queue when done.
  virtual void RenderDepth() = 0;

  // Clears the instance list. Called by the global renderer after draw.
  void EndRender();

 protected:
  abstract::VideoDevice* video_;
  abstract::Shader*      shader_;
  // Depth-only shader for the shadow pass; each concrete subclass sets this in
  // its constructor.  Released by ~ObjectRenderer if non-null.
  // cppcheck-suppress unusedStructMember
  abstract::Shader*      depth_shader_ = nullptr;
  std::vector<T*>        instances_;
  std::vector<T*>        depth_instances_;
};

template <typename T>
ObjectRenderer<T>::ObjectRenderer(const std::string& shader_name,
                                  abstract::VideoDevice* video)
    : video_(video), shader_(video->CreateShader(shader_name)) {}

template <typename T>
ObjectRenderer<T>::~ObjectRenderer() {
  if (depth_shader_) depth_shader_->Release();
  shader_->Release();
}

template <typename T>
void ObjectRenderer<T>::AddInstance(T* instance) {
  instances_.push_back(instance);
}

template <typename T>
void ObjectRenderer<T>::AddDepthInstance(T* instance) {
  depth_instances_.push_back(instance);
}

template <typename T>
void ObjectRenderer<T>::PrepareRender() {
  std::sort(instances_.begin(), instances_.end(), [](const T* a, const T* b) {
    const auto* mat_a = a->GetModel()->GetMaterial();
    const auto* mat_b = b->GetModel()->GetMaterial();
    if (mat_a != mat_b) return mat_a < mat_b;
    return a->GetModel() < b->GetModel();
  });
}

template <typename T>
void ObjectRenderer<T>::EndRender() {
  instances_.clear();
}

}  // namespace renderer
