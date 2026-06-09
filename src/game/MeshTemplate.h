#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "abstract/VideoDevice.h"
#include "core/BBox3.h"
#include "core/Resource.h"
#include "core/Vec3f.h"
#include "renderer/GeometryData.h"
#include "renderer/Mesh.h"

namespace game { class GameMaterial; }

namespace game {

// Reference-counted resource pairing GPU geometry with per-submesh GameMaterials.
//
// Keyed by mesh file path (file-backed) or an explicit id (procedural).
// Use an id starting with "__proc_" for procedural instances to exclude them
// from GetAll(). Always obtain file-backed instances via GetOrLoad().
//
// Single-material meshes (no submesh ranges in the geometry) keep one entry in
// materials_ at slot 0; this is the legacy fast-path and is backward compatible
// with all existing callers.
//
// Multi-material meshes carry one GameMaterial per submesh slot; materials are
// resolved from the submesh material_name stored in GeometryData.  Slot 0 can
// be overridden after construction with SetMaterial(0, mat) or SetMaterial(mat).
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
  // File-backed: loads geometry from disk and assigns mat to slot 0 (AddRef'd).
  // If mat is null a default (untextured) GameMaterial is created for slot 0.
  // For meshes with submesh ranges, each slot is loaded from the material_name
  // stored in the range; a default material is used when the name is empty.
  MeshTemplate(const std::string& mesh_path, abstract::VideoDevice* video,
               GameMaterial* mat = nullptr);

  // Procedural: takes explicit id, owned geometry, and an AddRef'd material
  // for slot 0.  Use an id starting with "__proc_" to exclude from GetAll().
  MeshTemplate(const std::string& id,
               std::unique_ptr<renderer::GeometryData> geo,
               GameMaterial* mat);

  ~MeshTemplate() override;

  MeshTemplate(const MeshTemplate&)            = delete;
  MeshTemplate& operator=(const MeshTemplate&) = delete;

  // Swaps the material at slot 0: releases old ref, AddRef's new,
  // then calls mesh_->SetMaterial() so all live MeshInstances pick up the
  // change automatically on the next frame.  Backward-compatible shorthand.
  void SetMaterial(GameMaterial* mat);

  // Swaps the material at the given slot: releases old ref, AddRef's new,
  // then propagates the change to the underlying Mesh.
  void SetMaterial(int slot, GameMaterial* mat);

  // Returns the geometry+material(s), or nullptr if initialisation failed.
  [[nodiscard]] renderer::Mesh*         GetMesh()     const;
  [[nodiscard]] renderer::GeometryData* GetGeometry() const;

  // Slot-0 accessor — backward-compatible shorthand.
  [[nodiscard]] GameMaterial* GetMaterial() const;

  // Per-slot accessor for multi-material meshes.
  [[nodiscard]] GameMaterial* GetMaterial(int slot) const;

  // Returns the number of material slots (1 for single-material meshes).
  [[nodiscard]] int GetMaterialCount() const;

  [[nodiscard]] const core::BBox3& GetLocalBBox() const;

  // CPU-side positions and indices retained for ray-triangle picking.
  // Empty for procedural templates (cube/sphere previews); check before use.
  [[nodiscard]] const std::vector<core::Vec3f>& GetCPUPositions() const;
  [[nodiscard]] const std::vector<uint32_t>&    GetCPUIndices()   const;

  // Returns the existing MeshTemplate for mesh_path (AddRef'd), or loads a new
  // one from disk with mat assigned to slot 0. mat is only used when a new
  // template is created; on cache hits call SetMaterial() explicitly if needed.
  [[nodiscard]] static MeshTemplate* GetOrLoad(const std::string& mesh_path,
                                               abstract::VideoDevice* video,
                                               GameMaterial* mat = nullptr);

  // Returns all file-backed templates keyed by their path basename.
  // Procedural templates (ids starting with "__proc_") are excluded.
  [[nodiscard]] static std::map<std::string, MeshTemplate*> GetAll();

 private:
  // Builds the materials_ vector for a file-backed mesh.  If sub_count > 0,
  // allocates one GameMaterial per submesh (loaded by name or default).
  // override_mat, when non-null, replaces slot 0 after population.
  void InitMaterials(int sub_count, const std::string& mesh_path,
                     abstract::VideoDevice* video, GameMaterial* override_mat);

  // cppcheck-suppress unusedStructMember
  std::unique_ptr<renderer::GeometryData> geometry_;
  // cppcheck-suppress unusedStructMember
  std::vector<GameMaterial*>              materials_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<renderer::Mesh>         mesh_;
  // cppcheck-suppress unusedStructMember
  std::vector<core::Vec3f>                cpu_positions_;
  // cppcheck-suppress unusedStructMember
  std::vector<uint32_t>                   cpu_indices_;
};

}  // namespace game
