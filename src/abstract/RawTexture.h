#pragma once

namespace abstract {

// Minimal GPU texture interface for data uploaded directly from CPU memory.
//
// Unlike abstract::Texture, RawTexture is not reference-counted and not
// registered in any resource registry. Callers own it via unique_ptr.
// Used for textures whose data comes from in-engine sources (e.g. heightmaps,
// normal maps) rather than asset files.
class RawTexture {
 public:
  virtual ~RawTexture() = default;

  RawTexture(const RawTexture&)            = delete;
  RawTexture& operator=(const RawTexture&) = delete;

  // Binds this texture to the given sampler slot for subsequent draw calls.
  virtual void Bind(int slot) = 0;

  // Re-uploads a rectangular sub-region of the texture from CPU data.
  // data must point to (w * h) texels in the texture's native format.
  // Used for incremental updates (e.g. sculpt-brush dirty tiles).
  virtual void UpdateRegion(int x, int y, int w, int h,
                             const void* data) = 0;

 protected:
  RawTexture() = default;
};

}  // namespace abstract
