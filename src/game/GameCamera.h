#pragma once

#include "core/Camera.h"
#include "core/CoordinateSystem.h"
#include "core/ProjectionType.h"
#include "core/Vec2f.h"
#include "game/GameObject.h"

namespace game {

// A game object that owns a core::Camera and derives its view from the world
// transform set via SetWorldTransform().
//
// Position is extracted from column 3, forward from −column 2, and up from
// column 1 of the world matrix (right-handed −Z convention). Any camera
// controller that computes a Mat4f and calls SetWorldTransform() will
// automatically drive this camera correctly.
//
// Screen centre must be set after construction (typically once the window
// dimensions are known) via SetScreenCenter().
class GameCamera : public GameObject {
 public:
  GameCamera(core::ProjectionType proj_type,
             core::CoordinateSystem coord_system);

  // ---- Convenience setters (each calls camera_.UpdateMatrices()) ------------

  void SetFOV(float fov_radians);
  void SetScreenCenter(core::Vec2f center);
  void SetMinDepth(float min_depth);
  void SetMaxDepth(float max_depth);

  // Returns the underlying camera for use by the renderer.
  [[nodiscard]] const core::Camera* GetCamera() const;

  // ---- GameObject overrides -------------------------------------------------

  // Extracts position, forward, and up from the world transform columns and
  // pushes them to camera_.
  void OnWorldTransformUpdated() override;

  // No-op: camera registration with the Renderer is handled by GameSystem.
  void OnAddedToScene() override {}

  // No-op: camera registration with the Renderer is handled by GameSystem.
  void OnRemovedFromScene() override {}

 private:
  core::Camera camera_;
};

}  // namespace game
