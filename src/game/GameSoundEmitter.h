#pragma once

#include <memory>
#include <string>

#include "core/BBox3.h"
#include "game/GameObject.h"

namespace audio {
class ResourceManager;
class Sound;
class SoundManager;
class VirtualSoundInstance;
}  // namespace audio

namespace game {

// Game object that loops a 3D positional sound at its world-space location.
//
// On scene entry the emitter registers a looping VirtualSoundInstance with
// the SoundManager. The hardware source assignment and distance culling are
// handled each frame by SoundManager::Update(). Position is forwarded to
// the VirtualSoundInstance whenever the world transform changes.
//
// Requires a live SoundManager and ResourceManager; if either is null the
// emitter is a silent no-op (safe for use in audio-less contexts).
class GameSoundEmitter : public GameObject {
 public:
  // sound_name: stem of the .sound.yaml asset (e.g. "ambient_water").
  // sound_manager and resource_manager may be null (emitter becomes silent).
  // volume_scale overrides the Sound's own volume field when > 0.
  GameSoundEmitter(const std::string& sound_name,
                   audio::SoundManager* sound_manager,
                   audio::ResourceManager* resource_manager,
                   float volume_scale = 1.f);

  // Releases the Sound resource reference.
  ~GameSoundEmitter() override;

  GameSoundEmitter(const GameSoundEmitter&)            = delete;
  GameSoundEmitter& operator=(const GameSoundEmitter&) = delete;
  GameSoundEmitter(GameSoundEmitter&&)                 = delete;
  GameSoundEmitter& operator=(GameSoundEmitter&&)      = delete;

  void Accept(GameObjectVisitor& visitor) override { visitor.Visit(*this); }

  // Returns a clone placed at position with the same sound and volume_scale.
  [[nodiscard]] std::unique_ptr<GameObject> Copy(
      const core::Vec3f& position) const override;

  // Registers a looping VirtualSoundInstance with the SoundManager.
  void OnAddedToScene() override;

  // Stops and unregisters the VirtualSoundInstance.
  void OnRemovedFromScene() override;

  // Forwards the emitter's world position to the VirtualSoundInstance.
  void OnWorldTransformUpdated() override;

  [[nodiscard]] const std::string& GetSoundName()  const { return sound_name_; }
  [[nodiscard]] float              GetVolumeScale() const { return volume_scale_; }

  // Updates the sound asset referenced by this emitter. The running instance
  // (if any) is stopped and the new sound is loaded on the next
  // OnAddedToScene() call. Safe to call with null managers (edit-time use).
  void SetSoundName(const std::string& name);

  // Updates the volume scale. Immediately applies to the live
  // VirtualSoundInstance when the emitter is in scene, otherwise deferred
  // until the next OnAddedToScene().
  void SetVolumeScale(float scale);

  // Returns the sound's reference distance (gain == 1 at this range).
  // Falls back to 1.0 when no sound is loaded.
  [[nodiscard]] float GetMinDistance() const;

  // Returns the sound's maximum attenuation distance (gain falls to 0).
  // Falls back to 100.0 when no sound is loaded.
  [[nodiscard]] float GetMaxDistance() const;

  // Swaps the audio managers. Stops any running instance, releases the current
  // sound, reloads from the new resource manager, and restarts playback if the
  // emitter is currently in the scene.
  // Pass null pointers to silence the emitter without removing it from the scene.
  void SetManagers(audio::SoundManager* sound_manager,
                   audio::ResourceManager* resource_manager);

 private:
  // cppcheck-suppress unusedStructMember
  std::string             sound_name_;
  // cppcheck-suppress unusedStructMember
  audio::SoundManager*    sound_manager_;
  // cppcheck-suppress unusedStructMember
  audio::ResourceManager* resource_manager_;
  // cppcheck-suppress unusedStructMember
  audio::Sound*           sound_   = nullptr;
  // cppcheck-suppress unusedStructMember
  float                   volume_scale_;
  // Non-owning handle; valid between OnAddedToScene() and OnRemovedFromScene().
  // cppcheck-suppress unusedStructMember
  audio::VirtualSoundInstance* instance_ = nullptr;
  // cppcheck-suppress unusedStructMember
  bool in_scene_ = false;
};

}  // namespace game
