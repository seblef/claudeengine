#pragma once

#include <cstdint>
#include <string>

#include "core/Vec3f.h"

namespace editor {

// Parameters controlling one procedural damage generation pass.
struct DamageGenerationParams {
  float severity = 0.5f;                          ///< Overall damage intensity in [0,1].
  uint32_t noise_seed = 0;                         ///< Drives the displacement/mask noise field; change to reroll.
  core::Vec3f half_extents = core::Vec3f::kOne;    ///< Vehicle body half-extents, used for zone-weighted falloff.
};

// Result of a generation pass.
struct DamageGenerationResult {
  bool success = false;
  // cppcheck-suppress unusedStructMember
  std::string mesh_path;  ///< Data-relative path to the written .emesh. Valid only when success is true.
};

// Procedurally generates a damaged mesh — and, when an uncompressed diffuse
// source texture is available next to the source material's .dds, a matching
// damaged material/texture pair — from a vehicle's pristine body mesh.
//
// Standalone one-shot utility (see editor/CLAUDE.md "GUI vs. edition logic
// separation"): VehicleEditorWindow's Damage tab calls Generate() from its
// "Generate Variant"/"Reroll" buttons and holds no generation logic itself.
//
// Output layout: files are written under a generated/ subfolder alongside
// the corresponding source asset (e.g. data/meshes/vehicles/generated/,
// data/textures/vehicles/generated/, data/materials/generated/) so they are
// clearly distinguishable from hand-authored source art. `variant_id` is a
// short stable identifier chosen once by the caller when a row is first
// generated and kept unchanged across reroll, so re-generating overwrites
// the same output files in place instead of accumulating orphans.
class DamageMeshGenerator {
 public:
  // source_mesh_path is data-relative (typically the vehicle's pristine
  // body_mesh path). Displaces the CPU mesh geometry inward along vertex
  // normals using a seeded noise field weighted by a soft per-vertex "zone
  // extremity" factor (normalized by half_extents, dominant axis — the same
  // approach as game::VehicleDamage::ClassifyZone(), made continuous instead
  // of discrete), recomputes normals/tangents, and writes a new .emesh.
  //
  // When every submesh of the source mesh shares a single material and that
  // material's diffuse texture has an uncompressed PNG source alongside its
  // compressed .dds, also composites a damage mask (rasterized from the same
  // per-vertex zone weights, plus procedural grime/streak noise) onto it and
  // writes a new material + texture pair, rewriting the mesh's submesh
  // material references to point at it. Falls back to a mesh-only variant
  // (original material references untouched) when no usable texture source
  // is found — this is expected for most vehicles today, since hand-authored
  // materials only ship compressed .dds textures.
  //
  // Returns a result with success=false and an empty mesh_path on failure
  // (source mesh missing/unreadable, or a write failure); check the log for
  // the reason.
  [[nodiscard]] static DamageGenerationResult Generate(
      const std::string& source_mesh_path,
      const std::string& variant_id,
      const DamageGenerationParams& params);
};

}  // namespace editor
