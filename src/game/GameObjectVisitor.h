#pragma once

namespace game {

class GameMesh;
class GameLight;
class GameCamera;

// Abstract visitor for all concrete GameObject subclasses.
//
// Implement to dispatch per-type logic without casting or GetType() switches.
// Each concrete GameObject calls the matching Visit() overload from Accept().
class GameObjectVisitor {
 public:
  virtual ~GameObjectVisitor() = default;

  virtual void Visit(GameMesh& mesh)     = 0;
  virtual void Visit(GameLight& light)   = 0;
  virtual void Visit(GameCamera& camera) = 0;
};

}  // namespace game
