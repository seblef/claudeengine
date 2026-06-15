# fix(editor): UV mapping on sphere preview mesh (#540)

## Problem

When switching to the sphere preview in the material editor, no texture
was displayed. The `CreateSphere` function in `GeometryUtils.cpp` only
set vertex positions; normals, binormals, tangents, and UV coordinates
were all left zeroed. With UV always (0, 0), every texel sampled from
the same point, producing a solid colour rather than the mapped texture.

## Changes

**`src/renderer/GeometryUtils.cpp`** — `CreateSphere`:

Added full per-vertex attribute computation for every sphere vertex:

| Attribute | Formula |
|---|---|
| Normal | equals position vector (unit sphere, outward) |
| Tangent | `(-sin θ, 0, cos θ)` — d/dθ(pos) normalised; degenerate at poles but fine since pole triangles have zero area |
| Binormal | `(cos φ cos θ, −sin φ, cos φ sin θ)` — d/dφ(pos), already unit length |
| UV | `u = r / rings`, `v = s / stacks` |

**`src/renderer/GeometryUtils.h`** — updated the block comment that
previously said "Quad/sphere/cone/pyramid populate only the position
field" to exclude sphere from that list.

## Decisions

- **Standard UV sphere parameterisation**: u wraps around longitude (0→1
  from west to east), v covers latitude (0 at north pole, 1 at south
  pole). This matches the convention used by most DCC tools and texture
  artists.
- **No seam duplication**: rings+1 vertices per stack row are generated
  (the last vertex duplicates the first longitude but with u=1 instead
  of u=0), which is the standard way to avoid a visible UV seam — the
  triangle winding already exploits this.
- **Pole degeneracy**: at s=0 and s=stacks, sin_phi=0 so the tangent
  formula would be (−sin θ, 0, cos θ) but the triangle area is zero,
  so no artefacts appear.

## Output to keep in mind

- `CreateCone` and `CreatePyramid` still zero all non-position fields.
  If either is later used as a preview mesh with textures those will
  need the same treatment.
- The sphere stacks/rings are currently hard-coded in
  `MaterialEditorWindow` (32 stacks, 64 rings); no changes needed there.

## Skills and instructions used

- `impl-issue` skill for issue-driven workflow
- CLAUDE.md: branch naming convention, conventional commit format,
  history file requirement, cpplint step
- src/CLAUDE.md: one class per file, Google C++ style
