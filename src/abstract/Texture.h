#pragma once

#include <string>

#include "abstract/BufferUsage.h"
#include "abstract/TextureFormat.h"
#include "core/Resource.h"

namespace abstract {

// Abstract texture resource, keyed by filename in the resource registry.
// Concrete subclasses load image data onto the GPU and implement Bind().
class Texture : public core::Resource<std::string, Texture> {
 public:
  // Binds this texture to the given texture unit for subsequent draw calls.
  virtual void Bind(int slot = 0) = 0;

  [[nodiscard]] int           GetWidth()  const { return width_; }
  [[nodiscard]] int           GetHeight() const { return height_; }
  [[nodiscard]] TextureFormat GetFormat() const { return format_; }
  [[nodiscard]] BufferUsage   GetUsage()  const { return usage_; }

 protected:
  Texture(const std::string& name, int width, int height,
          TextureFormat format, BufferUsage usage)
      : core::Resource<std::string, Texture>(name),
        width_(width), height_(height), format_(format), usage_(usage) {}

  ~Texture() override = default;

  int           width_;
  int           height_;
  TextureFormat format_;
  BufferUsage   usage_;
};

}  // namespace abstract
