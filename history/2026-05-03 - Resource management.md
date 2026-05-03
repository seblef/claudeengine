# Generic resource management (Issue #18)

## Changes

### `src/core/Resource.h` (new)

Header-only template base class for all engine resources.

**Template parameter**: `TId` — any type satisfying `std::map` key requirements (`operator<`).

| Member | Description |
|--------|-------------|
| `AddRef()` | Increments the reference count |
| `Release()` | Decrements the count; removes from registry and `delete this` at zero |
| `static Get(id)` | Returns the registered `Resource<TId>*` or `nullptr` |
| `GetId()` | Returns this instance's ID |
| `IsInitialized()` | Returns the `initialized_` flag |
| `initialized_` (protected) | Set by derived constructors on successful init |
| `static resources_` | Per-`TId` `std::map<TId, Resource<TId>*>` |

---

## Decisions and rationale

### Header-only
Templates require full definitions at the point of use. No `.cpp` file exists.

### Ref count starts at 1
The creator holds the first reference, matching the conventional COM/engine pattern. Callers call `AddRef()` when they store the pointer and `Release()` when done.

### `Release()` removes from map before `delete this`
`erase` is called first so no other thread or code can find a dangling pointer in the map between removal and deletion.

### Per-`TId` static map
Each instantiation (e.g., `Resource<std::string>`, `Resource<uint32_t>`) has its own independent `resources_` map. Shader IDs and texture IDs cannot collide even if they share the same underlying type.

### Protected `initialized_` flag
Derived classes need to set it; external callers only need to read it. Making it `protected` avoids an extra setter while remaining testable via `IsInitialized()`.

### No copy, no move
Resources are identity-typed objects managed through raw pointers and ref counts. Copying or moving would invalidate the map entry and corrupt the count.

## Output to keep in mind

- A resource with ref_count = 0 no longer exists — `Get()` returns `nullptr` for it.
- Concrete subclasses must call the base `Resource(id)` constructor and set `initialized_ = true` on success.
- The static map is a raw `std::map`; it is not thread-safe. Concurrent `AddRef`/`Release` across threads requires external locking or a lock-free ref-count.
- Adding a new resource type: inherit from `Resource<TId>`, call `Resource<TId>(id)` in your constructor, set `initialized_ = true` when ready.

## Skills and guidance followed

- `src/CLAUDE.md`: one class per header, project-relative includes, Google C++ style
- Root `CLAUDE.md`: branch/PR workflow, history file, conventional commits
