# Clouds: Per-Octave Rotation in FBM (#550)

## Summary

Applied a small 2D rotation between FBM octaves in `data/shaders/glsl/clouds/clouds_ps.glsl` to eliminate axis-aligned banding in the procedural cloud texture.

## Changes

**`data/shaders/glsl/clouds/clouds_ps.glsl`**

- Added compile-time constant `kRot` — a `mat2` encoding a ~30° rotation (`cos 30° ≈ 0.866`, `sin 30° = 0.5`).
- Changed `p *= 2.1` in the FBM loop to `p = kRot * p * 2.1`, applying the rotation before the frequency scaling at each octave.

## Decisions and Rationale

**Why 30°?**  
30° is an irrational fraction of 90° (unlike 45°, which re-aligns every two octaves). Four octaves accumulate rotations of 30°, 60°, 90°, 120° — no two octaves share the same sampling axis, so grid-aligned features in the value noise cannot constructively reinforce across frequency bands.

**Why a compile-time `mat2`?**  
The rotation angle never changes at runtime. Declaring `kRot` as a `const mat2` lets the GLSL compiler fold the multiply into the loop body at no runtime cost — the code reads as cleanly as the previous `p *= 2.1`.

**Order: rotate then scale**  
`kRot * p * 2.1` rotates the current coordinate first, then scales it up for the next octave. Swapping the order (`kRot * (p * 2.1)`) gives the same result mathematically (rotation and uniform scaling commute), but rotating first is more idiomatic when reading the code as "step to next octave."

**No impact on domain warping**  
The FBM function is called three times in `main()` — twice for the warp vector and once for the density field. The rotation applies uniformly inside FBM, so all three calls benefit and no additional changes are required.

## Output to keep in mind

- The FBM signature and return range [0, 1] are unchanged; callers are unaffected.
- The visual change is subtle but meaningful at mid-range `cloud_density` values where banding was most visible.
- This feature builds on the domain warping added in #549; both techniques are now active simultaneously.

## Skills used

- `impl-issue`

## CLAUDE.md instructions followed

- Checked out `dev` and pulled before branching.
- Branch named `feat/clouds-per-octave-rotation-550`.
- Commit message follows Conventional Commits (`feat(clouds): ...`).
- History file written as required.
- cpplint run (not applicable to GLSL; noted in output).
