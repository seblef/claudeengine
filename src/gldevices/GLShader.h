#pragma once

#include "abstract/Shader.h"

#include <GL/gl.h>

namespace gldevices {

// OpenGL shader program loaded from GLSL source files.
//
// On construction, reads <name>_vs.glsl (vertex) and <name>_ps.glsl (pixel)
// from Config::GetDataFolder() / "shaders/glsl", compiles each stage, and
// links them into a program. IsInitialized() returns true only on success.
class GLShader : public abstract::Shader {
 public:
  explicit GLShader(const std::string& name);
  ~GLShader() override;

  // Binds this program for subsequent draw calls.
  void Activate() override;

  // Sets an integer uniform by name. Must be called after Activate().
  void SetUniformInt(const std::string& name, int value) override;

 private:
  GLuint program_id_ = 0;

  // Compiles a single shader stage from source. Returns the shader object ID
  // on success or 0 on compile failure (errors are logged).
  static GLuint CompileStage(GLenum stage, const std::string& source,
                              const std::string& label);
};

}  // namespace gldevices
