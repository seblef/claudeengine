#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "abstract/ConstantBuffer.h"
#include "abstract/RenderTarget.h"
#include "abstract/RenderTargetGroup.h"
#include "abstract/VideoDevice.h"
#include "core/Camera.h"
#include "editor/IResourcePanel.h"
#include "editor/SoundEmitterSelectionModal.h"
#include "particles/ParticleEmitter.h"
#include "particles/ParticleRenderer.h"
#include "particles/ParticleSubSystemDesc.h"
#include "vfx/VFXDesc.h"

namespace particles { class ParticleSystemTemplate; }

namespace editor {

// IResourcePanel for .vfx.yaml: edits the parameter block for whichever
// vfx::VFXEffectType the file authors (explosion / electricity / fire /
// scrape) with a live, embedded particle preview.
//
// The preview is deliberately isolated from the live edited scene: it owns
// its own particles::ParticleRenderer, offscreen render target, and camera
// (same pattern as ParticleEditorWindow) rather than instantiating the real
// vfx::VFXExplosion / VFXElectricity / VFXFire / VFXScrape classes. Those
// classes reach into game::GameSystem (transient light), physics::PhysicsSystem
// (shockwave impulse), and the active camera controller (screen shake) —
// appropriate for real gameplay triggers, but not for a panel preview, which
// would otherwise leak a real light/impulse/shake into the scene being
// edited. The preview therefore approximates the effect visually (particles,
// plus a flat colour-flash overlay standing in for the explosion's light)
// and does not reproduce shockwave or screen shake.
class VFXPanel : public IResourcePanel {
 public:
  VFXPanel(std::filesystem::path path, abstract::VideoDevice* video);
  ~VFXPanel() override;

  VFXPanel(const VFXPanel&)            = delete;
  VFXPanel& operator=(const VFXPanel&) = delete;

  void Draw() override;
  [[nodiscard]] bool IsDirty() const override { return dirty_; }
  void Save() override;

 private:
  void LoadFromYaml();
  void SaveToYaml();

  void DrawEffectTypeSelector();
  void DrawExplosionParams();
  void DrawElectricityParams();
  void DrawFireParams();
  void DrawScrapeParams();
  void DrawSoundSection();
  void DrawPreviewSection();
  void DrawActionsBar();

  // Starts (or restarts) the preview for the current effect_type.
  void PlayPreview();
  // Stops the preview immediately and releases its emitters. Used both for
  // the explicit Stop button (fire/scrape loop until stopped) and for
  // one-shot effects once their authored duration has elapsed.
  void StopPreview();
  // Advances the active preview by dt: ticks emitters, drives the synthetic
  // scrape sliding motion, decays the explosion flash overlay, and
  // auto-stops one-shot effects once their duration elapses.
  void UpdatePreview(float dt);
  // Renders preview_emitters_ into preview_color_rt_ via preview_renderer_.
  void RenderPreviewFrame(float time);
  // Recomputes preview_camera_ position/target from the orbit state.
  void UpdatePreviewCamera();

  // Loads (ref-counted) the particle template backing the current
  // effect_type, releasing any previously-held template first. No-op if the
  // right template is already loaded.
  void EnsurePreviewTemplateLoaded();

  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice* video_;
  // cppcheck-suppress unusedStructMember
  std::filesystem::path  path_;
  bool                    dirty_ = false;

  // cppcheck-suppress unusedStructMember
  vfx::VFXDesc desc_;

  SoundEmitterSelectionModal sound_modal_{"Select VFX Sound"};

  // ---- Isolated preview (mirrors editor::ParticleEditorWindow) --------------

  // Non-owning; ref-counted via core::Resource. Released whenever the active
  // effect_type changes and in the destructor.
  particles::ParticleSystemTemplate* preview_template_ = nullptr;
  // Basename of the currently loaded preview_template_, so
  // EnsurePreviewTemplateLoaded() can tell when a reload is needed.
  // cppcheck-suppress unusedStructMember
  std::string preview_template_name_;

  // Stable storage for the descs backing preview_emitters_ (ParticleEmitter
  // stores a reference to its desc, so this vector must outlive — and never
  // reallocate out from under — the emitters built from it).
  // cppcheck-suppress unusedStructMember
  std::vector<particles::ParticleSubSystemDesc>            preview_descs_;
  // cppcheck-suppress unusedStructMember
  std::vector<std::unique_ptr<particles::ParticleEmitter>> preview_emitters_;
  std::unique_ptr<particles::ParticleRenderer>              preview_renderer_;

  std::unique_ptr<abstract::RenderTarget>      preview_color_rt_;
  std::unique_ptr<abstract::RenderTarget>      preview_depth_rt_;
  std::unique_ptr<abstract::RenderTargetGroup> preview_rtg_;
  std::unique_ptr<abstract::ConstantBuffer>    preview_scene_infos_cb_;
  core::Camera                                 preview_camera_;

  // cppcheck-suppress unusedStructMember
  float preview_orbit_azimuth_deg_   = 30.f;
  // cppcheck-suppress unusedStructMember
  float preview_orbit_elevation_deg_ = 15.f;
  // cppcheck-suppress unusedStructMember
  float preview_orbit_distance_      = 6.f;

  bool preview_playing_ = false;
  // True while the active preview is a looping effect (fire/scrape) that only
  // ends via StopPreview(); false for one-shots (explosion/electricity),
  // which self-stop once preview_elapsed_ passes their authored duration.
  bool preview_looping_ = false;
  // cppcheck-suppress unusedStructMember
  float preview_elapsed_ = 0.f;

  // Synthetic sliding-contact phase driving the scrape preview's spark
  // rate/direction — there is no real physics contact in an isolated preview.
  // cppcheck-suppress unusedStructMember
  float preview_scrape_phase_ = 0.f;

  // Explosion flash overlay: 1 at Play(), decays to 0 over
  // desc_.explosion.light_lifetime. Drawn as a flat colour tint over the
  // preview image; see the class comment for why there is no real light.
  // cppcheck-suppress unusedStructMember
  float preview_flash_ = 0.f;

  // cppcheck-suppress unusedStructMember
  static constexpr int kPreviewW = 256;
  // cppcheck-suppress unusedStructMember
  static constexpr int kPreviewH = 256;
};

}  // namespace editor
