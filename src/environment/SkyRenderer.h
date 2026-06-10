#pragma once

#include <memory>
#include <string>

#include "abstract/ConstantBuffer.h"
#include "abstract/IndexBuffer.h"
#include "abstract/Shader.h"
#include "abstract/Texture.h"
#include "abstract/VertexBuffer.h"
#include "abstract/VideoDevice.h"
#include "core/Camera.h"
#include "core/Singleton.h"

namespace environment {

// Singleton procedural sky renderer using the Preetham atmospheric scattering
// model (Preetham et al., "A Practical Analytic Model for Daylight", 1999).
//
// Renders a full-screen quad into the HDR render target (emissive FBO) with
// depth writes OFF and depth test LEQUAL so the sky fills only background pixels
// (those not written by the geometry pass).
//
// The fragment shader computes:
//   - Rayleigh + Mie scattering for the daytime sky (blue sky, orange sunsets)
//   - A bright HDR sun disc with a soft halo
//   - A silver moon disc visible when the sun is below the horizon
//   - A dark-blue night sky with procedural star noise
//
// Constant buffer slot 8 (SkyInfos): sun_direction, time_of_day, turbidity.
//
// Usage:
//   new SkyRenderer();
//   SkyRenderer::Instance().Build(video);
//   // each frame (inside the emissive pass):
//   SkyRenderer::Instance().Render(camera, world_time_of_day);
//   SkyRenderer::Instance().Reset();  // release GPU resources
//   SkyRenderer::Shutdown();          // delete instance
class SkyRenderer : public core::Singleton<SkyRenderer> {
 public:
  SkyRenderer() = default;
  ~SkyRenderer() = default;

  SkyRenderer(const SkyRenderer&)            = delete;
  SkyRenderer& operator=(const SkyRenderer&) = delete;
  SkyRenderer(SkyRenderer&&)                 = delete;
  SkyRenderer& operator=(SkyRenderer&&)      = delete;

  // Creates the fullscreen quad, loads the sky shader, and allocates the
  // SkyInfos constant buffer at slot 8. video must outlive this renderer.
  void Build(abstract::VideoDevice* video);

  // Fills the SkyInfos CB, activates the sky shader, draws the fullscreen quad
  // with depth writes OFF and depth test LEQUAL so sky fills background only.
  // world_time: time of day in hours [0, 24).
  void Render(const core::Camera& camera, float world_time);

  // Releases the shader and all GPU buffers.
  void Reset();

  // Sets atmospheric turbidity: 1.7 = very clear, 2.0 = default, 10 = very hazy.
  void SetTurbidity(float t) { turbidity_ = t; }

  // Loads (or clears) the moon texture from a path relative to data/textures/.
  // Pass an empty string to clear and fall back to the plain circle.
  // No-op before Build() is called.
  void SetMoonTexture(const std::string& path);

  // Loads (or clears) the night sky texture (equirectangular) from a path
  // relative to data/textures/. Pass an empty string to clear and fall back
  // to the procedural star-noise implementation.
  // No-op before Build() is called.
  void SetNightSkyTexture(const std::string& path);

  // Geographic latitude in degrees.  Positive = north hemisphere.
  // Lower latitudes raise the noon sun, producing a bluer midday sky.
  // Must match WorldTime::SetLatitude() so sky and scene lighting agree.
  void SetLatitude(float degrees)    { latitude_deg_    = degrees; }

  // Solar declination in degrees.  0° = equinox, +23.45° = northern summer
  // solstice.  Higher values raise the noon sun for northern latitudes.
  // Must match WorldTime::SetDeclination() so sky and scene lighting agree.
  void SetDeclination(float degrees) { declination_deg_ = degrees; }

  [[nodiscard]] bool IsReady() const { return shader_ != nullptr; }

 private:
  // Derives the world-space sun direction from the time of day using the same
  // astronomical formula as WorldTime::GetSunDirection().
  [[nodiscard]] core::Vec3f ComputeSunDirection(float time_of_day) const;

  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice*                    video_         = nullptr;
  // cppcheck-suppress unusedStructMember
  abstract::Shader*                         shader_        = nullptr;
  // cppcheck-suppress unusedStructMember
  abstract::Texture*                        moon_tex_      = nullptr;
  // cppcheck-suppress unusedStructMember
  abstract::Texture*                        night_sky_tex_ = nullptr;
  std::unique_ptr<abstract::ConstantBuffer> sky_cb_;
  std::unique_ptr<abstract::VertexBuffer>   quad_vb_;
  std::unique_ptr<abstract::IndexBuffer>    quad_ib_;
  float turbidity_       = 2.f;
  float latitude_deg_    = 45.f;  // geographic latitude; lower = bluer noon sky
  float declination_deg_ = 0.f;   // solar declination; +23.45 = northern summer
};

}  // namespace environment
