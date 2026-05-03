#pragma once

#include "core/EventType.h"
#include "core/Key.h"
#include "core/MouseButton.h"

namespace core {

// Stores a single input or window event.
//
// `type` identifies which payload fields are meaningful; fields unrelated to
// the active event type are present in memory but their values are undefined.
//
// Prefer the static factory methods over direct construction to make the
// intent clear at the call site.
class Event {
 public:
  Event() = default;
  Event(const Event&) = default;
  Event& operator=(const Event&) = default;

  // Window event (no payload beyond the type).
  inline explicit Event(EventType t) : type(t) {}

  // ---- Factory helpers ------------------------------------------------------

  [[nodiscard]] static inline Event MouseMoved(float x, float y) {
    Event e(EventType::kMouseMoved);
    e.mouse_x = x;
    e.mouse_y = y;
    return e;
  }

  [[nodiscard]] static inline Event MouseWheel(float delta) {
    Event e(EventType::kMouseWheelChanged);
    e.wheel_delta = delta;
    return e;
  }

  // type must be kMouseButtonDown or kMouseButtonUp.
  [[nodiscard]] static inline Event MouseButton(EventType t,
                                                core::MouseButton button) {
    Event e(t);
    e.mouse_button = button;
    return e;
  }

  // type must be kKeyDown or kKeyUp.
  [[nodiscard]] static inline Event Key(EventType t, core::Key k) {
    Event e(t);
    e.key = k;
    return e;
  }

  // ---- Payload --------------------------------------------------------------

  EventType type = EventType::kWindowClose;

  float mouse_x = 0.f;
  float mouse_y = 0.f;
  float wheel_delta = 0.f;
  core::MouseButton mouse_button = core::MouseButton::kLeft;
  core::Key key = core::Key::kUnknown;
};

}  // namespace core
