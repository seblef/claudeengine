#pragma once

#include <string>

#include "core/Resource.h"

namespace abstract {

// Abstract shader program. Managed resource keyed by shader name.
//
// Concrete implementations (e.g., GLShader) inherit this class, upload the
// program to the GPU, set initialized_ = true on success, and implement
// Activate() to bind the program for subsequent draw calls.
class Shader : public core::Resource<std::string, Shader> {
 public:
  // Binds this shader program for subsequent draw calls.
  virtual void Activate() = 0;

  // Returns true if this shader also has tessellation-control / evaluation stages.
  [[nodiscard]] virtual bool HasTessellation() const { return false; }

  // Binds the tessellation variant of this shader program.
  // Falls back to Activate() if HasTessellation() is false.
  virtual void ActivateTess() { Activate(); }

  // Sets an integer uniform by name. Must be called after Activate().
  virtual void SetUniformInt(const std::string& name, int value) = 0;

  // Sets a float uniform by name. Must be called after Activate().
  virtual void SetUniformFloat(const std::string& name, float value) = 0;

  // Sets a vec2 uniform by name. Must be called after Activate().
  virtual void SetUniform2f(const std::string& name, float x, float y) = 0;

 protected:
  explicit Shader(const std::string& name)
      : core::Resource<std::string, Shader>(name) {}
  ~Shader() override = default;
};

}  // namespace abstract
