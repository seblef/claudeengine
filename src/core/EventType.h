#pragma once

#include <cstdint>

namespace core {

// Identifies the kind of event stored in an Event instance.
enum class EventType : uint8_t {
  // ---- Window ---------------------------------------------------------------
  kWindowClose,
  kWindowLostFocus,
  kWindowGetFocus,
  kWindowMinimized,
  kWindowRestored,
  // ---- Mouse ----------------------------------------------------------------
  kMouseMoved,
  kMouseWheelChanged,
  kMouseButtonDown,
  kMouseButtonUp,
  // ---- Keyboard -------------------------------------------------------------
  kKeyDown,
  kKeyUp,
};

}  // namespace core
