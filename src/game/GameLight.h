#pragma once

#include <memory>

#include "core/Vec3f.h"
#include "game/GameObject.h"
#include "game/GameLightDesc.h"
#include "renderer/Light.h"

namespace game {

// Game object wrapping a renderer light.
//
// Internally creates and owns the appropriate renderer::Light subclass
// (GlobalLight, OmniLight, CircleSpotLight, or RectangleSpotLight) based on
// the provided LightType and GameLightDesc. The renderer light's world matrix
// is kept in sync with the GameObject world transform.
//
// The renderer light is registered with Renderer when this object is added to
// the scene and unregistered when it is removed.
class GameLight : public GameObject {
 public:
  // Creates the renderer light from `type` and `desc`.
  // The light starts at the identity world transform; call SetWorldTransform()
  // to place it before or after adding it to the scene.
  explicit GameLight(renderer::LightType type,
                     const GameLightDesc& desc = GameLightDesc{});

  ~GameLight() override = default;

  void Accept(GameObjectVisitor& visitor) override { visitor.Visit(*this); }

  void OnWorldTransformUpdated() override;
  void OnAddedToScene()          override;
  void OnRemovedFromScene()      override;

  [[nodiscard]] renderer::Light* GetLight() const;

  // Sets the spot light direction in world space and updates rest_direction_ to
  // match the current world rotation. Only meaningful for kCircleSpot /
  // kRectSpot; calling on other types is a no-op.
  void SetSpotDirection(const core::Vec3f& world_dir);

 private:
  // Instantiates the right renderer::Light subclass from type and desc.
  static std::unique_ptr<renderer::Light> CreateRendererLight(
      renderer::LightType type, const GameLightDesc& desc);

  // cppcheck-suppress unusedStructMember
  std::unique_ptr<renderer::Light> light_;
  // Local-space spot direction (direction with identity world rotation applied).
  // Kept in sync so OnWorldTransformUpdated() can derive the correct world-space
  // direction after any rotation. Only meaningful for kCircleSpot / kRectSpot.
  // cppcheck-suppress unusedStructMember
  core::Vec3f rest_direction_;
};

}  // namespace game
