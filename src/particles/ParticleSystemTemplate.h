#pragma once

#include <map>
#include <string>
#include <vector>

#include "abstract/VideoDevice.h"
#include "core/Resource.h"
#include "particles/EmbeddedLightDesc.h"
#include "particles/ParticleSubSystemDesc.h"

namespace particles {

// Reference-counted resource loading particle system definitions from
// data/particles/*.particles.yaml.
//
// The key is the file basename without extension (e.g. "fire" for
// fire.particles.yaml). Obtain instances exclusively through GetOrLoad().
//
// Lifecycle follows core::Resource: the initial ref-count is 1 (held by the
// caller of GetOrLoad). Call AddRef() when an additional owner keeps the
// pointer; call Release() to decrement — the object destroys itself when the
// count reaches zero.
class ParticleSystemTemplate
    : public core::Resource<std::string, ParticleSystemTemplate> {
 public:
  // Returns the existing template for name (AddRef'd), or loads
  // data/particles/<name>.particles.yaml and returns a new instance.
  // video is reserved for future GPU-side resource allocation.
  [[nodiscard]] static ParticleSystemTemplate* GetOrLoad(
      const std::string& name, abstract::VideoDevice* video);

  // Returns all loaded templates keyed by their basename (no extension).
  [[nodiscard]] static std::map<std::string, ParticleSystemTemplate*> GetAll();

  [[nodiscard]] const std::vector<ParticleSubSystemDesc>& GetSubSystems() const;
  [[nodiscard]] const std::vector<EmbeddedLightDesc>&    GetLights()     const;

 private:
  explicit ParticleSystemTemplate(const std::string& name,
                                  abstract::VideoDevice* video);

  // cppcheck-suppress unusedStructMember
  std::vector<ParticleSubSystemDesc> sub_systems_;
  // cppcheck-suppress unusedStructMember
  std::vector<EmbeddedLightDesc>    lights_;
};

}  // namespace particles
