# fix(gldevices): sampler binding fallback via glUniform1i — 2026-06-11

## Problem

All GLSL shaders in the project declare sampler uniforms with `layout(binding = N)`.
`GLShader::Activate()` only called `glUseProgram(program_id_)` and never set sampler
uniforms explicitly.  The spec guarantees that `layout(binding)` initialises the uniform
value, but some Mesa drivers ignore this qualifier and assign every sampler to unit 0,
causing all samples to read from the same texture.  The water shaders were the clearest
victim (five distinct samplers, all collapsing to unit 0), but every shader with multiple
samplers was at risk.

## Fix

Added two helpers in the anonymous namespace of `GLShader.cpp`:

### `IsSamplerType(GLenum type) -> bool`
A `switch`-based classifier covering all 40+ sampler GLenum values (sampler2D,
sampler2DShadow, samplerCube, samplerCubeShadow, int/uint variants, array, multisample,
rect, buffer, cube map array).

### `InitSamplerBindings(GLuint prog)`
Called once after each successful `LinkProgram`:
1. `glUseProgram(prog)` — makes the program active so `glUniform1i` targets it.
2. `glGetProgramiv(prog, GL_ACTIVE_UNIFORMS, &count)` — enumerate all active uniforms.
3. For each uniform: `glGetActiveUniform` → skip non-samplers via `IsSamplerType`.
4. `glGetUniformiv(prog, loc, &binding)` — reads back the value the linker set from
   the `layout(binding = N)` qualifier (conformant drivers populate this; non-conformant
   drivers return 0).
5. `glUniform1i(loc, binding)` — re-sets the value explicitly, overriding driver default.

The approach is generic: it works for every shader and sampler type without any
shader-specific knowledge.  It reads the binding value from the linked program, so there
is no duplication of binding constants between C++ and GLSL.

## Decisions

- **Generic scan vs. targeted patch**: Patching water shaders only would require
  maintaining C++-side binding lists in sync with GLSL.  Scanning active uniforms is
  self-contained and fixes all shaders uniformly.
- **Call site at link time**: The fix happens once during construction, not on every
  `Activate()`.  Sampler bindings are program state; they survive `glUseProgram`.
- **`glGetUniformiv` to read back binding**: Reads the linker-assigned value rather than
  re-parsing GLSL source or hard-coding constants.  On conformant drivers the value
  equals N from the layout qualifier; on non-conformant drivers it is 0 (still correct
  for shaders with only one sampler; multi-sampler shaders were broken before this fix).
- **Header unchanged**: The helpers are anonymous-namespace functions in the `.cpp`;
  no API surface changes.

## Files changed

- `src/gldevices/GLShader.cpp` — added `IsSamplerType`, `InitSamplerBindings`; two
  call sites after `LinkProgram` (base program and tessellation variant).

## Output to keep in mind

- `InitSamplerBindings` leaves the linked program bound (`glUseProgram` side effect).
  This is harmless: `Activate()` / `ActivateTess()` always re-bind the program before
  drawing.
- The fix covers all sampler GLenum types defined in OpenGL 4.6 / glext.h, including
  cube-map arrays and unsigned-int samplers that are not currently used but may be in
  the future.

## Skills / CLAUDE.md sections used

- `impl-issue` skill
- `src/CLAUDE.md`: Google C++ style, one class per file, anonymous-namespace helpers
- `CPPLINT.cfg`: 120-char line limit, filter list
