# Procedural Damage Mesh & Texture Generator for Vehicle Damage Tab

Implements #846: a "Generate Variant" / "Reroll" workflow in the Vehicle
Editor's Damage tab that procedurally produces a damaged body mesh (and,
when possible, a matching damaged texture) from a vehicle's pristine body
mesh, so `mesh_variants` rows (schema added in #834) can be populated without
hand-authoring in a DCC tool.

## What changed

- **New `editor::DamageMeshGenerator`** (`src/editor/tools/DamageMeshGenerator.h/.cpp`):
  a standalone one-shot utility class (not an `EditorToolBase` subclass — it
  has no viewport/per-frame interaction, so per `editor/CLAUDE.md`'s "GUI vs.
  edition logic separation" it belongs in `editor/tools/` as a plain
  command/utility class instead).
  - Reads the source `.emesh` via `mesh::EmeshReader`, displaces every
    vertex inward along its normal by `severity * zone_weight * noise`,
    where `zone_weight` is a **continuous** analogue of
    `game::VehicleDamage::ClassifyZone()`'s per-axis-normalized
    (by `half_extents`), dominant-axis logic — instead of collapsing to one
    of five discrete zones, it returns a soft `[0,1]` "extremity" scalar, so
    damage naturally clusters near corners/edges of the body without the
    user picking a specific zone (see Decisions below).
  - Noise is a 3-octave FBM built on `stb_perlin_noise3_seed` (mirrors
    `terrain::TerrainGenerator::GenerateFbm`'s approach, applied to
    mesh-local positions instead of a heightfield).
  - Recomputes normals/tangents (`mesh::ComputeNormals`/`ComputeTangents`)
    and the AABB after displacement, then writes a new `.emesh` via
    `mesh::EmeshWriter`.
  - When every submesh of the source mesh shares one material, and that
    material's diffuse texture has an **uncompressed `.png` sibling** next
    to its `.dds` (see Decisions — Texture damage), also:
    - rasterizes the same per-vertex zone weights into UV space (a small
      software triangle rasterizer, barycentric-interpolated, max-blended
      across seam triangles) so the texture mask lines up with the mesh
      deformation;
    - composites darkening + desaturation + a rust-tint, modulated by
      procedural grime/streak noise, onto the loaded PNG pixels;
    - writes the damaged PNG and a **new material YAML** (the original
      material's YAML node is loaded, its `rendering.textures.diffuse` key
      is repointed at the new texture, and the whole node is re-emitted —
      preserving every other field automatically) under `.../generated/`;
    - rewrites the copied mesh's submesh `material_name` to the new
      material, so the damaged look travels with the mesh exactly like a
      hand-authored variant would (this is *why* `physics::DamageMeshVariant`
      doesn't need a texture field — the `.emesh`'s own submesh table already
      carries the material reference, per `mesh::SubMeshRange::material_name`
      and how `game::MeshTemplate`/`GameMesh` resolve it via
      `GameMaterial::GetOrLoad(material_name, ...)`).
  - Falls back to a mesh-only variant (original material references
    untouched) when no PNG source is found, or the mesh has mixed materials.
  - Output layout: `data/meshes/vehicles/generated/`,
    `data/textures/vehicles/generated/`, `data/materials/generated/` —
    keeps generated assets alongside their source folder but clearly
    separated from hand-authored art.

- **`editor::VehicleEditorWindow`** (Damage tab, Body Damage Meshes section):
  - "Generate Variant" button next to "Add Mesh Variant": appends a new
    `mesh_variants` row and immediately runs the generator with a fresh
    random `variant_id` (a 6-hex-char string) and noise seed at a default
    severity of 0.5.
  - Generated rows show a "Severity" slider + "Reroll" button (hand-picked
    rows via "Browse..." don't). Reroll keeps the row's `variant_id` (so
    output filenames — and thus the row's `mesh_path` — stay identical) but
    draws a fresh random noise seed, so re-running overwrites the same files
    in place rather than orphaning old ones.
  - This per-row "is this a generated row, what severity/id does it use"
    state lives in a new UI-only `MeshVariantGenState` struct
    (`is_generated`, `severity`, `variant_id`), parallel to the existing
    `mesh_variant_tmpls_`/`mesh_variant_previews_` arrays. It is **not**
    serialized — `physics::DamageMeshVariant` is unchanged, exactly as the
    issue specified ("no schema changes needed"). A reloaded row therefore
    always starts as hand-authored (no Reroll button) until regenerated in
    the current session; see Non-goals/Open questions in the issue (reroll
    history was explicitly decided as out of scope for v1).

- **Pre-existing gap fixed in passing**: `VehicleEditorWindow::LoadFromYaml()`/
  `SaveToYaml()` never parsed or wrote the `physics.half_extents` key, even
  though `game::VehicleTemplate` (the runtime loader) does, and
  `data/vehicles/test_box.vehicle.yaml` sets it. Any vehicle with a non-default
  body size opened in the editor would silently have its `vehicle_desc_`
  fall back to `physics::VehicleDesc`'s default `{1, 0.5, 2}` half-extents,
  and saving would drop the field from the YAML entirely. Since the new
  generator's zone-weighting is directly a function of `half_extents`, this
  bug would have made the "extremity" weighting wrong for any such vehicle,
  so it's fixed here: `half_extents` is now parsed/written like `com_offset`,
  and exposed as a `DragFloat3` in `DrawPhysicsSection()` (it previously had
  no editor UI control at all).

## Decisions (raised with the user, since the issue left them open)

1. **Texture damage v1 — require an uncompressed PNG source per vehicle.**
   Investigation found every existing vehicle diffuse texture is a
   BC-compressed `.dds` (loaded via `gli::load()` straight to GPU-compressed
   blocks in `GLTexture.cpp`), and this engine has **no CPU-side BC
   decompressor** anywhere (`stb_image` can't read `.dds`). Compositing a
   damage mask onto the albedo needs decoded pixels. Options considered:
   mesh-only v1 (defer texture entirely), write a minimal BC decoder, or
   require an uncompressed PNG sibling per vehicle. Chose the PNG-sibling
   convention: `TryGenerateDamagedMaterial()` looks for `{diffuse_stem}.png`
   next to the material's `.dds` and silently falls back to mesh-only when
   absent (logged at INFO, not a hard failure) — this means the full mesh+texture
   path is not yet exercised for any *existing* vehicle (none ship a PNG
   source today; `tools/texture_compressor` deletes the PNG source by
   default after compressing to `.dds`), but is real, tested-by-construction
   code that activates the moment a vehicle adds one. **Follow-up**: either
   author a PNG source for at least one vehicle (e.g. `hell_sized`) to
   exercise the texture path end-to-end, or revisit with a BC decode/encode
   step if that's preferred over the authoring convention.
2. **Whole-body only, no per-zone picker** — matches #834's single-list,
   average-damage-fraction design. The zone weighting still shapes the noise
   falloff internally (corners/edges get more, the flat centre gets less),
   it's just not user-selectable per generation.
3. **`generated/` subfolder** convention (not a `_generated` filename
   suffix) for all three asset kinds (mesh/texture/material).
4. **Reroll is overwrite-only**, no seed history/undo — matches the
   acceptance criteria ("without leaving orphaned generated files").

## Manual QA

Not performed — this is a headless dev environment (per root `CLAUDE.md`,
`wreckoning_editor` must not be launched here). Verified instead via:
`cpplint` (clean), `cppcheck --enable=all` on the new file (no findings
beyond pre-existing unrelated `unmatchedSuppression` info-notes in headers
this file includes), a full `wreckoning_editor` rebuild (succeeds), and the
existing `ctest` suite (all non-`gli`-third-party tests pass — the 70
"Not Run" failures are pre-existing vendored `gli` library test binaries
unrelated to this change, not built in this environment). **Flagging as a
follow-up**: visually confirming a generated dented/wrecked mesh actually
looks plausible in the live editor preview, and exercising the texture-
compositing path with a real PNG source, both need a graphical session.

## Output for future Milestone 3 issues

- The `generated/` output convention (mesh/texture/material each get a
  sibling `generated/` folder) is now established — any future generator-
  style tooling should follow the same pattern for discoverability.
- `VehicleEditorWindow`'s per-row transient UI state pattern
  (`MeshVariantGenState`, parallel-indexed array, rebuilt in
  `RebuildMeshVariantPreviews()`) is a reusable template for other
  "generated but not serialized" per-row metadata, should a similar need
  arise elsewhere in the editor.
- The `half_extents` load/save/UI gap fix means vehicles with non-default
  body sizes now round-trip correctly through the editor — worth a spot
  check if any other `physics::VehicleDesc` fields have a similar
  parse/UI gap (this one was found only because the new feature happened to
  depend on it).

## Skills used

- `impl-issue` (this contribution)

## CLAUDE.md / instruction sections specifically applied

- `src/editor/CLAUDE.md` — "GUI vs. edition logic separation (CRITICAL)":
  drove putting all generation logic in `DamageMeshGenerator` rather than
  inline in `VehicleEditorWindow::DrawBodyMeshVariantsSection()`.
- `src/editor/CLAUDE.md` — dependency leaf rule and `PRIVATE ${stb_SOURCE_DIR}`
  availability on the `editor` target confirmed `stb_image`/`stb_image_write`
  could be used directly without new impl translation units (the `editor`
  target already compiles `stb_image_impl.cpp`/`stb_image_write_impl.cpp`).
- `src/physics/CLAUDE.md` — confirmed `physics::DamageMeshVariant` stays
  POD/Jolt-free and unmodified; the generator only ever populates its
  existing `mesh_path` field.
- Root `CLAUDE.md` — "Verification" section: no windowed binary was launched;
  relied on `cpplint`/`cppcheck`/full rebuild/`ctest` and flagged the
  live-preview QA gap explicitly above instead of claiming untested success.
