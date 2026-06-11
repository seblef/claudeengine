# fix(terrain): unbind all material texture slots after Render()

**Issue:** #480  
**PR:** #487  
**Branch:** fix/terrain-albedo-slot5-unbind-480

## Problem

`TerrainRenderer::BindMaterialTextures()` binds:
- Slot 0: heightmap
- Slot 1: splatmap
- Slots 2–5: up to 4 albedo layers (`kAlbedoBaseSlot = 2`)
- Slots 6–9: up to 4 normal layers (`kNormalBaseSlot = 6`)
- Slot 10: macro texture
- Slot 11: caustic

None of these were unbound at the end of `Render()`. `Material::Set()` only covers slots 0–4 (`kTextureSlotCount = 5`), so slot 5 (the 4th albedo layer) was never overwritten in subsequent mesh passes. With a 4-layer terrain material, any shader reading sampler slot 5 after the terrain pass would sample stale terrain albedo.

## Fix

Added `UnbindMaterialTextures()` private method to `TerrainRenderer`, called at the end of `Render()`. It uses `video_->UnbindSampler(slot)` to release each slot the renderer owns, guarding the layer loop with the actual `layer_count` from the material.

```cpp
void TerrainRenderer::UnbindMaterialTextures() const {
  video_->UnbindSampler(kHeightmapSlot);
  video_->UnbindSampler(kSplatmapSlot);
  const int layer_count = material_ ? material_->GetLayerCount() : 0;
  for (int i = 0; i < layer_count; ++i) {
    video_->UnbindSampler(kAlbedoBaseSlot + i);
    video_->UnbindSampler(kNormalBaseSlot + i);
  }
  if (macro_texture_) video_->UnbindSampler(kMacroTextureSlot);
  if (caustic_tex_)   video_->UnbindSampler(kCausticSlot);
}
```

## Decisions

- **Approach 1 (unbind after render) chosen** over approach 2 (extend `kTextureSlotCount`). Extending `kTextureSlotCount` would have required updating all shaders and material code, a much wider blast radius for a boundary fix.
- Unbind is done comprehensively (all terrain slots, not just slot 5) to be consistent with patterns established in `WaterRenderer`, `SkyRenderer`, `LightRenderer`, and `ShadowRenderer`.

## Pattern note for future contributions

After any renderer that binds textures outside the `Material::Set()` range (slots 0–4), add an `UnbindXxx()` call at the end of the render method. See `WaterRenderer.cpp:455–458`, `ShadowRenderer.cpp:43`, `Renderer.cpp:268–271` for existing examples.

## Skills / CLAUDE.md references used

- `src/terrain/CLAUDE.md` — terrain module dependency rules
- `src/CLAUDE.md` — one class per file, Google C++ style
- Root `CLAUDE.md` — git workflow, conventional commits, history file requirement
