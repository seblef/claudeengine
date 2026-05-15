#pragma once

#include <map>
#include <memory>
#include <string>

#include "abstract/VideoDevice.h"
#include "core/Resource.h"
#include "renderer/RenderableMesh.h"

namespace game {

// Reference-counted resource wrapping a GPU-ready RenderableMesh.
//
// Keyed by mesh file path so each file is loaded from disk at most once.
// Materials are loaded automatically from the material_slot names embedded in
// the mesh file (see renderer::MeshLoader). Always obtain instances via
// GetOrLoad(), never via direct construction.
//
// Lifecycle:
//   MeshTemplate* t = MeshTemplate::GetOrLoad(path, video);
//   // ... use t->GetRenderableMesh() ...
//   t->Release();   // destroys when ref-count reaches zero
//
// A GameMesh calls AddRef() in its constructor and Release() in its destructor,
// so manual ref management is only needed when holding a raw pointer outside
// of a GameMesh.
class MeshTemplate : public core::Resource<std::string, MeshTemplate> {
 public:
  MeshTemplate(const std::string& mesh_path, abstract::VideoDevice* video);

  // Constructor for procedurally generated meshes.
  // Takes ownership of `mesh`; bypasses file loading and the path-keyed registry.
  explicit MeshTemplate(renderer::RenderableMesh* mesh);

  // Returns the loaded mesh, or nullptr if initialisation failed.
  [[nodiscard]] renderer::RenderableMesh* GetRenderableMesh() const;

  // Returns the existing MeshTemplate for mesh_path (AddRef'd), or creates
  // a new one by loading the mesh from disk.
  [[nodiscard]] static MeshTemplate* GetOrLoad(const std::string& mesh_path,
                                               abstract::VideoDevice* video);

  // Returns all file-backed templates keyed by their path basename.
  // Procedurally generated templates (registered under "__proc_*") are excluded.
  [[nodiscard]] static std::map<std::string, MeshTemplate*> GetAll();

 private:
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<renderer::RenderableMesh> mesh_;
};

}  // namespace game
