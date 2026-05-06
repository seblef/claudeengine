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
// Concrete subclasses implement Render() to issue GPU draw calls for the
// sorted instance list.
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

  // Clears the instance list. Called by the global renderer after draw.
  void EndRender();

 protected:
  abstract::VideoDevice* video_;
  abstract::Shader*      shader_;
  std::vector<T*>        instances_;
};

template <typename T>
ObjectRenderer<T>::ObjectRenderer(const std::string& shader_name,
                                  abstract::VideoDevice* video)
    : video_(video), shader_(video->CreateShader(shader_name)) {}

template <typename T>
ObjectRenderer<T>::~ObjectRenderer() {
  shader_->Release();
}

template <typename T>
void ObjectRenderer<T>::AddInstance(T* instance) {
  instances_.push_back(instance);
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
