// Must be defined before any GL/gl.h include (which pulls in glext.h internally).
#define GL_GLEXT_PROTOTYPES

#include "gldevices/GLShader.h"

#include "core/Config.h"

#include <fstream>
#include <loguru.hpp>
#include <sstream>

namespace gldevices {

namespace {
std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream f(path);
  if (!f) return {};
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}
}  // namespace

GLShader::GLShader(const std::string& name) : abstract::Shader(name) {
  const std::filesystem::path dir =
      core::Config::GetDataFolder() / "shaders" / "glsl";

  const std::string vert_src = ReadFile(dir / (name + "_vs.glsl"));
  const std::string frag_src = ReadFile(dir / (name + "_ps.glsl"));

  if (vert_src.empty() || frag_src.empty()) {
    LOG_F(ERROR, "GLShader '%s': failed to read source files from '%s'",
          name.c_str(), dir.c_str());
    return;
  }

  const GLuint vert = CompileStage(GL_VERTEX_SHADER,   vert_src, name + "_vs.glsl");
  const GLuint frag = CompileStage(GL_FRAGMENT_SHADER, frag_src, name + "_ps.glsl");

  if (!vert || !frag) {
    glDeleteShader(vert);
    glDeleteShader(frag);
    return;
  }

  program_id_ = glCreateProgram();
  glAttachShader(program_id_, vert);
  glAttachShader(program_id_, frag);
  glLinkProgram(program_id_);

  glDeleteShader(vert);
  glDeleteShader(frag);

  GLint ok = 0;
  glGetProgramiv(program_id_, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[512];
    glGetProgramInfoLog(program_id_, sizeof(log), nullptr, log);
    LOG_F(ERROR, "GLShader '%s': link error: %s", name.c_str(), log);
    glDeleteProgram(program_id_);
    program_id_ = 0;
    return;
  }

  initialized_ = true;
  LOG_F(INFO, "GLShader '%s': compiled and linked successfully", name.c_str());
}

GLShader::~GLShader() {
  if (program_id_) glDeleteProgram(program_id_);
}

void GLShader::Activate() {
  glUseProgram(program_id_);
}

GLuint GLShader::CompileStage(GLenum stage, const std::string& source,
                               const std::string& label) {
  const GLuint id = glCreateShader(stage);
  const char* src = source.c_str();
  glShaderSource(id, 1, &src, nullptr);
  glCompileShader(id);

  GLint ok = 0;
  glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[512];
    glGetShaderInfoLog(id, sizeof(log), nullptr, log);
    LOG_F(ERROR, "GLShader '%s': compile error: %s", label.c_str(), log);
    glDeleteShader(id);
    return 0;
  }
  return id;
}

}  // namespace gldevices
