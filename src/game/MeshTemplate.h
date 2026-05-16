#pragma once

#include <map>
#include <memory>
#include <string>

#include "abstract/VideoDevice.h"
#include "core/BBox3.h"
#include "core/Resource.h"
#include "renderer/GeometryData.h"
#include "renderer/Material.h"
#include "renderer/Mesh.h"

namespace game {

// Reference-counted resource pairing GPU geometry with a surface material.
//
// Keyed by mesh file path so each file is loaded from disk at most once.
// Material assignment will move to game::GameMaterial (Issue #204); for now
// each template holds a default (untextured) Material.
// Always obtain file-backed instances via GetOrLoad(), never via direct construction.
//
// Lifecycle:
//   MeshTemplate* t = MeshTemplate::GetOrLoad(path, video);
//   // ... use t->GetMesh() ...
//   t->Release();   // destroys when ref-count reaches zero
//
// A GameMesh calls AddRef() in its constructor and Release() in its destructor,
// so manual ref management is only needed when holding a raw pointer outside
// of a GameMesh.
class MeshTemplate : public core::Resource<std::string, MeshTemplate> {
 public:
  MeshTemplate(const std::string& mesh_path, abstract::VideoDevice* video);

  // Constructor for procedurally generated meshes.
  // Takes ownership of geo and mat; bypasses file loading and the path-keyed registry.
  MeshTemplate(std::unique_ptr<renderer::GeometryData> geo,
               std::unique_ptr<renderer::Material> mat);

  // Returns the geometry+material pair, or nullptr if initialisation failed.
  [[nodiscard]] renderer::Mesh*        GetMesh()       const;
  [[nodiscard]] const core::BBox3&     GetLocalBBox()  const;

  // Returns the existing MeshTemplate for mesh_path (AddRef'd), or creates
  // a new one by loading the mesh from disk.
  [[nodiscard]] static MeshTemplate* GetOrLoad(const std::string& mesh_path,
                                               abstract::VideoDevice* video);

  // Returns all file-backed templates keyed by their path basename.
  // Procedurally generated templates (registered under "__proc_*") are excluded.
  [[nodiscard]] static std::map<std::string, MeshTemplate*> GetAll();

 private:
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<renderer::GeometryData> geometry_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<renderer::Material>     material_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<renderer::Mesh>         mesh_;
};

}  // namespace game
