# Window, mouse & keyboard event system (Issue #12)

## Changes

### `src/core/EventType.h` (new)

`EventType` enum covering all window, mouse, and keyboard event variants.

### `src/core/MouseButton.h` (new)

`MouseButton` enum: `kLeft`, `kRight`. OS-independent.

### `src/core/Key.h` (new)

`Key` enum: A–Z, 0–9, F1–F12, arrow keys, modifier keys (LShift/RShift, LCtrl/RCtrl, LAlt/RAlt), and common special keys (Escape, Enter, Space, Tab, Backspace, Delete, Insert, Home, End, PageUp, PageDown), plus `kUnknown` sentinel.

### `src/core/Event.h` (new)

Flat data class with an `EventType` tag and payload fields.

| Member | Description |
|--------|-------------|
| `EventType type` | Identifies which payload fields are active |
| `float mouse_x, mouse_y` | Mouse position (kMouseMoved) |
| `float wheel_delta` | Scroll delta (kMouseWheelChanged) |
| `MouseButton mouse_button` | Button (kMouseButtonDown/Up) |
| `Key key` | Key code (kKeyDown/Up) |

Static factory helpers: `Event::MouseMoved`, `Event::MouseWheel`, `Event::MouseButton`, `Event::Key`. Window events use the single-argument `explicit Event(EventType)` constructor.

### `src/core/Singleton.h` (new)

CRTP singleton template. Derived class pattern:
```cpp
class MySystem : public Singleton<MySystem> {
  friend class Singleton<MySystem>;
  MySystem() = default;
};
```
`Instance()` is thread-safe (C++11 static local). Copy/move deleted.

### `src/core/EventManager.h` (new)

Singleton FIFO event queue backed by `std::queue<Event>`.

| Method | Description |
|--------|-------------|
| `Publish(event)` | Appends to the back of the queue |
| `HasEvents()` | Returns true if the queue is non-empty |
| `Consume()` | Removes and returns the oldest event |

---

## Decisions and rationale

### Flat `Event` struct instead of class hierarchy or `std::variant`

A tagged struct is cache-friendly, zero-allocation, and trivially copyable. The fixed, closed set of event shapes makes a union approach simpler than a polymorphic hierarchy. Fields outside the active type are present but callers ignore them by convention (documented in the header).

### Static factory methods on `Event`

Direct field construction would leave the intent unclear (which of the five fields matter for this event type?). Factory methods name the event semantically and only set relevant fields.

### CRTP `Singleton<T>` rather than a macro or per-class boilerplate

A template captures the pattern once. The C++11 static-local guarantee makes it thread-safe without explicit locking. Future singletons (audio manager, resource cache, etc.) inherit the same pattern for free.

### No CMakeLists changes

All six files are header-only. The `core` library already exports `src/` as an include root via `target_include_directories`.

## Output to keep in mind

- `Event` fields outside the active type are meaningless — callers must check `type` before reading payload fields.
- `Consume()` has a precondition: `HasEvents()` must be true. Calling it on an empty queue is UB (front() on empty std::queue).
- `Key::kUnknown` is the sentinel for unmapped platform keys.
- Adding a new event type: add a value to `EventType`, add a factory to `Event`, update any switch statements in consumers.

## Skills and guidance followed

- `src/CLAUDE.md`: one class per header, project-relative includes, Google C++ style
- Root `CLAUDE.md`: branch/PR workflow, history file, conventional commits
