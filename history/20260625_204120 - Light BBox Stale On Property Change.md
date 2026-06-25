# Fix: Light BBox Stale on Property Change

**Issue**: #803  
**PR**: #805  
**Branch**: `fix/light-bbox-stale-on-property-change`

---

## Problem

Changing a spot light's `range`, angles, or direction in the editor did not update its bounding box. Same for `OmniLight` when `radius` changes. This caused:

- Stale selection/picking boxes in the editor
- Incorrect renderer culling (light remains invisible after growing past viewport edge)
- Undo/redo not refreshing bboxes

Two bbox objects were both stale:
- `renderer::Renderable::local_bbox_` / `world_bbox_` — used by renderer culling
- `game::GameObject::local_bbox_` / `world_bbox_` — used by editor for selection/gizmo

Both were computed once at construction and never updated by setters.

---

## Design

**Principle**: the renderer light owns its geometry → it owns its bbox. The `ComputeLocalBBox()` logic previously duplicated in `GameLight.cpp` was moved onto the renderer lights, which now recompute their local bbox whenever a geometry-affecting property changes.

---

## Changes

### `Renderable` (renderer)

Added `protected void SetLocalBBox(const core::BBox3& bbox)`:
- Updates `local_bbox_` and recomputes `world_bbox_ = local_bbox_ * world_matrix_`
- Notifies the visibility system (`OnRenderableMoved`) so culling structures (octree, etc.) stay consistent

### `CircleSpotLight`

Added private `RecomputeLocalBBox()`:
```cpp
void CircleSpotLight::RecomputeLocalBBox() {
  const float base_r = std::tan(outer_angle_) * range_;
  const core::Vec3f base_center = local_direction_ * range_;
  core::BBox3 bbox(core::Vec3f::kZero, core::Vec3f::kZero);
  bbox << base_center;
  const core::Vec3f exp(base_r, base_r, base_r);
  SetLocalBBox({bbox.GetMin() - exp, bbox.GetMax() + exp});
}
```

Called from: constructor (replaces conservative sphere), `SetOuterAngle()`, `SetRange()`, `SetDirection()`.  
`SetInnerAngle()` left as inline — inner angle does not affect bbox.

### `RectangleSpotLight`

Same pattern: `RecomputeLocalBBox()` computes the 4 base corners of the frustum pyramid and takes their AABB. Called from constructor, `SetHAngle()`, `SetVAngle()`, `SetRange()`, `SetDirection()`.

### `OmniLight`

`SetRadius()` moved from inline header to `.cpp`:
```cpp
void OmniLight::SetRadius(float r) {
  radius_ = r;
  SetLocalBBox({{-r, -r, -r}, {r, r, r}});
}
```

### `GameObject` (game)

Added `protected void SetLocalBBox(const core::BBox3& bbox)`:
- Updates `local_bbox_` and recomputes `world_bbox_ = local_bbox_ * world_transform_`
- No child propagation needed (children's bboxes are independent)

### `GameLight`

Added `public void RefreshBBox()`:
```cpp
void GameLight::RefreshBBox() {
  SetLocalBBox(light_->GetLocalBBox());
}
```

Removed the anonymous `ComputeLocalBBox()` helper — its logic now lives on the renderer lights. The constructor initializes `GameObject` with a placeholder `{-1,-1,-1},{1,1,1}` then calls `RefreshBBox()` after `light_` is constructed.

### `PropertiesPanel`

Called `game_light->RefreshBBox()` after every geometry-affecting setter:
- `OmniLight::SetRadius()`
- `CircleSpotLight::SetOuterAngle()`, `SetRange()`, `SetDirection()`
- `RectangleSpotLight::SetHAngle()`, `SetVAngle()`, `SetRange()`, `SetDirection()`

### `LightPropertyCommand::ApplySnapshot()`

Added `game_light->RefreshBBox()` at the end, after all setters. This covers both `Execute()`/`Redo()` and `Undo()`.

---

## Key decisions

1. **Renderer light owns the bbox geometry**. Previously `GameLight.cpp` duplicated the AABB formula. Now the renderer light is the single source of truth; `RefreshBBox()` is a one-line pull from that source.

2. **`SetInnerAngle` left unchanged**. Inner angle controls intensity falloff only; it does not affect the light's spatial envelope.

3. **`GlobalLight` not touched**. Global light uses a fixed 1×1×1 bbox (it's always-visible, fullscreen) — no geometry properties to update.

4. **`RefreshBBox()` is unconditional in `ApplySnapshot()`**. Since `ApplySnapshot` switches on light type, it would be possible to call `RefreshBBox()` only for geometry-affecting types; but calling it unconditionally is cheaper than the switch and robust against future changes.

---

## Skills / instructions used

- `impl-issue` skill
- `src/CLAUDE.md` — Google C++ style, one class per file, include root is `src/`
- `src/editor/CLAUDE.md` — editor is the dependency-graph leaf; panel classes must be pure UI
- `src/game/CLAUDE.md` — `SetWorldTransform` is the single entry point for spatial updates

---

## Notes for future work

- `OctreeVisibilitySystem::OnRenderableMoved` will be called on every setter call during a drag. This is correct but could be optimized if needed (e.g. defer until drag end). For now the octree is rebuilt per-call, which is negligible for the number of lights typically in a scene.
- If a new light type is added, `RecomputeLocalBBox()` must be implemented and hooked into its setters, and `PropertiesPanel` + `ApplySnapshot` must call `RefreshBBox()` after its setters.
