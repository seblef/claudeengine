# Shaders rules

- A shader includes both vertex and pixel programs
- Vertex and pixel programs must be stored in different glsl files.
- Both vertex and pixel programs must have same name with different suffix, for instance to the shader `shadows` must correspond vertex program `shadows_vs.glsl` and the pixel program `shadows_ps.glsl`
- Shaders source code must follow Blend GLSL coding style
- Provide documentation in comments for equations
- Custom `#include <file>` is supported by the application; uniforms and layouts must be imported using the `#include` command
- Code should be factorized using the custom `#include` command

# Folder structure

Shaders are organized in subfolders:
* `layouts` includes layouts declarations
* `uniforms` includes uniforms declarations
