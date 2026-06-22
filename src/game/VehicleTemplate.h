#pragma once

#include <map>
#include <string>

#include "core/Resource.h"
#include "physics/VehicleDesc.h"

namespace abstract { class VideoDevice; }

namespace game {

class MeshTemplate;

// Reference-counted resource that parses a vehicle descriptor YAML file and
// pre-loads the body and wheel MeshTemplates.
//
// Keyed by the descriptor path relative to core::Config::GetDataFolder()
// (e.g. "vehicles/car.vehicle.yaml").  Obtain instances exclusively via
// GetOrLoad(); the returned pointer has ref-count 1.  Shared instances are
// returned AddRef'd so all callers must pair every GetOrLoad() with a Release().
//
// Caching: if two map objects reference the same desc_path, only one YAML parse
// and one set of MeshTemplate loads happen.  GetAll() enumerates all cached
// templates — useful for vehicle selection screens.
class VehicleTemplate : public core::Resource<std::string, VehicleTemplate> {
 public:
  // Returns the existing template for desc_path (AddRef'd), or loads a new one.
  // desc_path is relative to core::Config::GetDataFolder().
  // Returns nullptr when the file cannot be parsed or required fields are absent;
  // no template is left in the cache in that case.
  [[nodiscard]] static VehicleTemplate* GetOrLoad(const std::string& desc_path,
                                                  abstract::VideoDevice* video);

  // Returns all loaded templates keyed by their desc_path.
  [[nodiscard]] static std::map<std::string, VehicleTemplate*> GetAll();

  ~VehicleTemplate() override;

  VehicleTemplate(const VehicleTemplate&)            = delete;
  VehicleTemplate& operator=(const VehicleTemplate&) = delete;

  // The descriptor path used to identify this template (equal to GetId()).
  [[nodiscard]] const std::string&          GetDescPath()           const;
  [[nodiscard]] const physics::VehicleDesc& GetVehicleDesc()        const;
  // Non-owning views; callers must not Release() these directly.
  [[nodiscard]] MeshTemplate*               GetBodyTemplate()        const;
  [[nodiscard]] MeshTemplate*               GetFrontWheelTemplate()  const;
  [[nodiscard]] MeshTemplate*               GetRearWheelTemplate()   const;

 private:
  explicit VehicleTemplate(const std::string& desc_path,
                            abstract::VideoDevice* video);

  // cppcheck-suppress unusedStructMember
  physics::VehicleDesc vehicle_desc_;
  // cppcheck-suppress unusedStructMember
  MeshTemplate*        body_tmpl_        = nullptr;
  // cppcheck-suppress unusedStructMember
  MeshTemplate*        front_wheel_tmpl_ = nullptr;
  // cppcheck-suppress unusedStructMember
  MeshTemplate*        rear_wheel_tmpl_  = nullptr;
};

}  // namespace game
