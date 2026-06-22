#pragma once

namespace game {

class GameCamera;
class GameLight;
class GameMesh;
class GameParticleSystem;
class GamePivot;
class GamePlayerStart;
class GameRoad;
class GameSoundEmitter;
class GameTerrain;
class GameVehicle;

// Abstract visitor for all concrete GameObject subclasses.
//
// Implement to dispatch per-type logic without casting or GetType() switches.
// Each concrete GameObject calls the matching Visit() overload from Accept().
class GameObjectVisitor {
 public:
  virtual ~GameObjectVisitor() = default;

  virtual void Visit(GameCamera& camera)                  = 0;
  virtual void Visit(GameLight& light)                    = 0;
  virtual void Visit(GameMesh& mesh)                      = 0;
  virtual void Visit(GameParticleSystem& particle_system) = 0;
  virtual void Visit(GamePivot& pivot)                    = 0;
  virtual void Visit(GamePlayerStart& player_start)       = 0;
  virtual void Visit(GameRoad& road)                      = 0;
  virtual void Visit(GameSoundEmitter& sound_emitter)     = 0;
  virtual void Visit(GameTerrain& terrain)                = 0;
  virtual void Visit(GameVehicle& vehicle)                = 0;
};

}  // namespace game
