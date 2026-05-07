#pragma once

#include "core/Singleton.h"
#include "renderer/MeshInstance.h"
#include "renderer/ObjectRenderer.h"

namespace renderer {

// Singleton batch renderer for MeshInstance objects.
//
// Lifecycle: created by Renderer constructor; destroyed by Renderer destructor.
// PrepareRender() sorts by material (primary) then by mesh (secondary) so that
// the most expensive state change (texture bind) happens as rarely as possible.
class MeshRenderer : public core::Singleton<MeshRenderer>,
                     public ObjectRenderer<MeshInstance> {
 public:
  explicit MeshRenderer(abstract::VideoDevice* video);

  // Renders all enqueued instances with minimal material and geometry binds.
  void Render() override;
};

}  // namespace renderer
