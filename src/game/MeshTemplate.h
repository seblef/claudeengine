#pragma once

#include <map>
#include <memory>
#include <string>

#include "abstract/VideoDevice.h"
#include "core/BBox3.h"
#include "core/Resource.h"
#include "renderer/GeometryData.h"
#include "renderer/Mesh.h"

namespace game { class GameMaterial; }

namespace game {

// Reference-counted resource pairing GPU geometry with a GameMaterial.
//
// Keyed by mesh file path (file-backed) or an explicit id (procedural).
// Use an id starting with "__proc_" for procedural instances to exclude them
// from GetAll(). Always obtain file-backed instances via GetOrLoad().
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
  // File-backed: loads geometry from disk and assigns mat (AddRef'd).
  // If mat is null a default (untextured) GameMaterial is created internally.
  MeshTemplate(const std::string& mesh_path, abstract::VideoDevice* video,
               GameMaterial* mat = nullptr);

  // Procedural: takes explicit id, owned geometry, and an AddRef'd material.
  // Use an id starting with "__proc_" to exclude from GetAll().
  MeshTemplate(const std::string& id,
               std::unique_ptr<renderer::GeometryData> geo,
               GameMaterial* mat);

  ~MeshTemplate() override;

  MeshTemplate(const MeshTemplate&)            = delete;
  MeshTemplate& operator=(const MeshTemplate&) = delete;

  // Swaps the assigned material: releases old ref, AddRef's new,
  // then calls mesh_->SetMaterial() so all live MeshInstances
  // pick up the change automatically on the next frame.
  void SetMaterial(GameMaterial* mat);

  // Returns the geometry+material pair, or nullptr if initialisation failed.
  [[nodiscard]] renderer::Mesh*         GetMesh()     const;
  [[nodiscard]] renderer::GeometryData* GetGeometry() const;
  [[nodiscard]] GameMaterial*           GetMaterial() const;
  [[nodiscard]] const core::BBox3&      GetLocalBBox() const;

  // Returns the existing MeshTemplate for mesh_path (AddRef'd), or loads a new
  // one from disk with mat assigned. mat is only used when a new template is
  // created; on cache hits call SetMaterial() explicitly if needed.
  [[nodiscard]] static MeshTemplate* GetOrLoad(const std::string& mesh_path,
                                               abstract::VideoDevice* video,
                                               GameMaterial* mat = nullptr);

  // Returns all file-backed templates keyed by their path basename.
  // Procedural templates (ids starting with "__proc_") are excluded.
  [[nodiscard]] static std::map<std::string, MeshTemplate*> GetAll();

 private:
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<renderer::GeometryData> geometry_;
  // cppcheck-suppress unusedStructMember
  GameMaterial*                           material_ = nullptr;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<renderer::Mesh>         mesh_;
};

}  // namespace game
