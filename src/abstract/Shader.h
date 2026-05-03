#pragma once

#include <string>

#include "core/Resource.h"

namespace abstract {

// Abstract shader program. Managed resource keyed by shader name.
//
// Concrete implementations (e.g., GLShader) inherit this class, upload the
// program to the GPU, set initialized_ = true on success, and implement
// Activate() to bind the program for subsequent draw calls.
class Shader : public core::Resource<std::string> {
 public:
  // Binds this shader program for subsequent draw calls.
  virtual void Activate() = 0;

 protected:
  explicit Shader(const std::string& name) : core::Resource<std::string>(name) {}
  virtual ~Shader() = default;
};

}  // namespace abstract
