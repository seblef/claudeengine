// Must be defined before any GL/gl.h include (which pulls in glext.h internally).
#define GL_GLEXT_PROTOTYPES

#include "gldevices/GLShader.h"

#include "core/Config.h"

#include <fstream>
#include <loguru.hpp>
#include <sstream>
#include <unordered_set>

namespace gldevices {

namespace {
std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream f(path);
  if (!f) return {};
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

bool ParseIncludePath(const std::string& line, std::string& out_path) {
  size_t i = 0;
  while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
  if (line.compare(i, 8, "#include") != 0) return false;
  i += 8;
  while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
  if (i >= line.size() || (line[i] != '<' && line[i] != '"')) return false;
  const char close = (line[i++] == '<') ? '>' : '"';
  const size_t start = i;
  while (i < line.size() && line[i] != close) ++i;
  if (i >= line.size()) return false;
  out_path = line.substr(start, i - start);
  return true;
}

std::string ResolveIncludes(const std::string& source,
                             const std::filesystem::path& shader_dir,
                             std::unordered_set<std::string>& included) {
  std::istringstream in(source);
  std::ostringstream out;
  std::string line;
  while (std::getline(in, line)) {
    std::string inc_path;
    if (ParseIncludePath(line, inc_path)) {
      if (included.insert(inc_path).second) {
        const std::string inc_src = ReadFile(shader_dir / inc_path);
        if (inc_src.empty()) {
          LOG_F(WARNING, "GLShader: include not found: '%s'", inc_path.c_str());
        } else {
          out << ResolveIncludes(inc_src, shader_dir, included) << '\n';
        }
      }
    } else {
      out << line << '\n';
    }
  }
  return out.str();
}
}  // namespace

namespace {
GLuint LinkProgram(const std::string& label,
                   std::initializer_list<GLuint> stages) {
  const GLuint prog = glCreateProgram();
  for (GLuint s : stages) glAttachShader(prog, s);
  glLinkProgram(prog);
  GLint ok = 0;
  glGetProgramiv(prog, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[512];
    glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
    LOG_F(ERROR, "GLShader '%s': link error: %s", label.c_str(), log);
    glDeleteProgram(prog);
    return 0;
  }
  return prog;
}
}  // namespace

GLShader::GLShader(const std::string& name) : abstract::Shader(name) {
  const std::filesystem::path dir =
      core::Config::GetDataFolder() / "shaders" / "glsl";

  std::unordered_set<std::string> vert_included, frag_included;
  const std::string vert_src =
      ResolveIncludes(ReadFile(dir / (name + "_vs.glsl")), dir, vert_included);
  const std::string frag_src =
      ResolveIncludes(ReadFile(dir / (name + "_ps.glsl")), dir, frag_included);

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

  program_id_ = LinkProgram(name, {vert, frag});
  glDeleteShader(vert);
  glDeleteShader(frag);

  if (!program_id_) return;
  initialized_ = true;
  LOG_F(INFO, "GLShader '%s': compiled and linked successfully", name.c_str());

  // Optionally load tessellation stages if <name>_tesc.glsl / _tese.glsl exist.
  std::unordered_set<std::string> tesc_inc, tese_inc;
  const std::string tesc_src =
      ResolveIncludes(ReadFile(dir / (name + "_tesc.glsl")), dir, tesc_inc);
  const std::string tese_src =
      ResolveIncludes(ReadFile(dir / (name + "_tese.glsl")), dir, tese_inc);

  if (tesc_src.empty() || tese_src.empty()) return;

  const GLuint vert2 = CompileStage(GL_VERTEX_SHADER,          vert_src, name + "_vs.glsl");
  const GLuint tesc  = CompileStage(GL_TESS_CONTROL_SHADER,    tesc_src, name + "_tesc.glsl");
  const GLuint tese  = CompileStage(GL_TESS_EVALUATION_SHADER, tese_src, name + "_tese.glsl");
  const GLuint frag2 = CompileStage(GL_FRAGMENT_SHADER,        frag_src, name + "_ps.glsl");

  if (vert2 && tesc && tese && frag2) {
    tess_program_id_ = LinkProgram(name + "(tess)", {vert2, tesc, tese, frag2});
    if (tess_program_id_)
      LOG_F(INFO, "GLShader '%s': tessellation variant linked successfully", name.c_str());
  }
  glDeleteShader(vert2);
  glDeleteShader(tesc);
  glDeleteShader(tese);
  glDeleteShader(frag2);
}

GLShader::~GLShader() {
  if (program_id_)      glDeleteProgram(program_id_);
  if (tess_program_id_) glDeleteProgram(tess_program_id_);
}

void GLShader::Activate() {
  glUseProgram(program_id_);
}

void GLShader::ActivateTess() {
  glUseProgram(tess_program_id_);
}

void GLShader::SetUniformInt(const std::string& name, int value) {
  glUniform1i(glGetUniformLocation(program_id_, name.c_str()), value);
}

void GLShader::SetUniformFloat(const std::string& name, float value) {
  glUniform1f(glGetUniformLocation(program_id_, name.c_str()), value);
}

void GLShader::SetUniform2f(const std::string& name, float x, float y) {
  glUniform2f(glGetUniformLocation(program_id_, name.c_str()), x, y);
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
