# Abstract devices class (Issue #14)

## Changes

### `src/abstract/Devices.h` (new)

Abstract base class for all platform device managers.

| Member | Description |
|--------|-------------|
| `Devices()` | Initialises the `EventManager` singleton |
| `virtual ~Devices()` | Virtual destructor (default) |
| `virtual void Update() = 0` | Pumps OS messages into the EventManager queue each frame |
| `virtual void* GetWindow() = 0` | Returns the native window handle |

### `src/abstract/CMakeLists.txt` (new)

INTERFACE library declaration linking against `core`.

```cmake
add_library(abstract INTERFACE)
target_link_libraries(abstract INTERFACE core)
```

### `src/CMakeLists.txt` (modified)

Uncommented `add_subdirectory(abstract)` to activate the module.

---

## Decisions and rationale

### INTERFACE library
`Devices.h` is the only file and it is header-only (inline constructor, `= default` destructor, pure virtual methods). An INTERFACE library avoids a compilation unit while correctly expressing the `core` dependency for consumers. It will be upgraded to STATIC when the first platform backend `.cpp` file is added.

### Constructor anchors `EventManager` lifetime
Calling `core::EventManager::Instance()` in the `Devices` constructor guarantees the singleton exists before the first `Update()` call, regardless of static-initialisation order. No member reference is stored — callers access the manager directly via `EventManager::Instance()`.

### `GetWindow()` returns `void*`
The native window type differs per platform (`HWND` on Windows, `Display*` / `Window` on X11, `NSWindow*` on macOS). `void*` is the minimal common abstraction; concrete platform code casts to the type it knows.

### `abstract` namespace
Mirrors the `core` module convention. `abstract` is not a C++ reserved keyword.

## Output to keep in mind

- Concrete platform backends inherit from `abstract::Devices` and implement `Update()` and `GetWindow()`.
- The `abstract` CMake target must be added to the `target_link_libraries` of any module that instantiates or uses a concrete `Devices` subclass.
- When the first `.cpp` file is added to `abstract`, change `add_library(abstract INTERFACE)` to `add_library(abstract STATIC ...)`.

## Skills and guidance followed

- `src/CLAUDE.md`: one class per header, project-relative includes, new modules need CMakeLists + entry in `src/CMakeLists.txt`
- Root `CLAUDE.md`: branch/PR workflow, history file, conventional commits
