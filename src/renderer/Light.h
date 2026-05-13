#pragma once

#include <optional>

#include "core/BBox3.h"
#include "core/Color.h"
#include "core/Mat4f.h"
#include "renderer/Renderable.h"

namespace renderer {

// Discriminator returned by Light::GetType().
// Values match the `type` field in LightInfos (0–3).
enum class LightType : int {
  kGlobal     = 0,  // GlobalLight — directional + ambient, fullscreen quad.
  kOmni       = 1,  // OmniLight — point light, sphere volume.
  kCircleSpot = 2,  // CircleSpotLight — cone spot, cone volume.
  kRectSpot   = 3,  // RectangleSpotLight — rectangular beam, pyramid volume.
};

// Abstract base for all light types.
//
// Inherits Renderable for spatial placement (world matrix, bounding box,
// visibility). Subclasses implement GetType() and GetVolumeMatrix() so that
// LightRenderer can select the correct geometry and shader, and size/orient
// the volume mesh for each draw call.
class Light : public Renderable {
 public:
  // color: RGB light color; intensity: brightness multiplier.
  // local_bbox and world_matrix are forwarded to Renderable.
  Light(LightType type, const core::Color& color, float intensity,
        const core::BBox3& local_bbox,
        const core::Mat4f& world_matrix,
        bool always_visible);

  ~Light() override = default;

  // Enqueues this light to LightRenderer for the current frame.
  void Enqueue() override;

  // Returns the discriminator identifying this light's concrete type.
  [[nodiscard]] LightType GetType() const { return type_; }

  // Returns the transform that positions, orients, and scales the light volume
  // mesh.  Called by LightRenderer before each draw call.
  [[nodiscard]] virtual core::Mat4f GetVolumeMatrix() const = 0;

  // Returns the light-space view-projection matrix used to render the shadow
  // map for this light, or nullopt if this light type does not support 2D
  // shadow maps (e.g. GlobalLight uses CSM; OmniLight uses a cube map).
  // ShadowRenderer calls this without switching on light type.
  [[nodiscard]] virtual std::optional<core::Mat4f> ComputeShadowVP() const {
    return std::nullopt;
  }

  [[nodiscard]] const core::Color& GetColor()    const { return color_; }
  [[nodiscard]] float              GetIntensity() const { return intensity_; }

  void SetColor(const core::Color& color) { color_ = color; }
  void SetIntensity(float intensity)      { intensity_ = intensity; }

  // When false the light never renders a shadow map (useful for fill lights).
  [[nodiscard]] bool GetCastShadow() const        { return cast_shadow_; }
  void               SetCastShadow(bool b)        { cast_shadow_ = b; }

  // Maximum shadow map resolution cap for this light (default 1024).
  // The pool may assign a lower resolution based on screen-space size (#147).
  [[nodiscard]] int  GetShadowResolution() const  { return shadow_resolution_; }
  void               SetShadowResolution(int r)   { shadow_resolution_ = r; }

 private:
  // cppcheck-suppress unusedStructMember
  LightType   type_;
  // cppcheck-suppress unusedStructMember
  core::Color color_;
  // cppcheck-suppress unusedStructMember
  float       intensity_;
  // cppcheck-suppress unusedStructMember
  bool        cast_shadow_       = true;
  // cppcheck-suppress unusedStructMember
  int         shadow_resolution_ = 1024;
};

}  // namespace renderer
