#pragma once

#include <memory>
#include <string>

#include "abstract/VideoDevice.h"
#include "core/Resource.h"
#include "renderer/MaterialDesc.h"
#include "renderer/RenderableMesh.h"

namespace game {

// Reference-counted resource wrapping a GPU-ready RenderableMesh.
//
// Keyed by mesh file path so each file is loaded from disk at most once.
// Always obtain instances via GetOrLoad(), never via direct construction.
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
  // Constructs from an explicit material descriptor.
  MeshTemplate(const std::string& mesh_path, abstract::VideoDevice* video,
               const renderer::MaterialDesc& desc);

  // Constructs by loading a MaterialDesc from a YAML material file.
  MeshTemplate(const std::string& mesh_path, abstract::VideoDevice* video,
               const std::string& material_file_path);

  // Returns the loaded mesh, or nullptr if initialisation failed.
  [[nodiscard]] renderer::RenderableMesh* GetRenderableMesh() const;

  // ---- Static factories ----------------------------------------------------

  // Returns the existing MeshTemplate for mesh_path (AddRef'd), or creates a
  // new one from the provided MaterialDesc.
  [[nodiscard]] static MeshTemplate* GetOrLoad(const std::string& mesh_path,
                                               abstract::VideoDevice* video,
                                               const renderer::MaterialDesc& desc = {});

  // Returns the existing MeshTemplate for mesh_path (AddRef'd), or creates a
  // new one whose MaterialDesc is loaded from material_file_path.
  [[nodiscard]] static MeshTemplate* GetOrLoad(const std::string& mesh_path,
                                               abstract::VideoDevice* video,
                                               const std::string& material_file_path);

 private:
  // Parses a YAML material file and returns the corresponding MaterialDesc.
  // Returns a default MaterialDesc on parse failure.
  static renderer::MaterialDesc LoadMaterialDesc(
      const std::string& material_file_path);

  // cppcheck-suppress unusedStructMember
  std::unique_ptr<renderer::RenderableMesh> mesh_;
};

}  // namespace game
