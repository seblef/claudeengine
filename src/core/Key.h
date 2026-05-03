#pragma once

#include <cstdint>

namespace core {

// Platform-independent keyboard key identifier.
//
// Platform input drivers map native key codes to these values before
// publishing events, keeping the rest of the engine free from OS headers.
enum class Key : uint16_t {
  // ---- Letters --------------------------------------------------------------
  kA, kB, kC, kD, kE, kF, kG, kH, kI, kJ, kK, kL, kM,
  kN, kO, kP, kQ, kR, kS, kT, kU, kV, kW, kX, kY, kZ,
  // ---- Digits ---------------------------------------------------------------
  k0, k1, k2, k3, k4, k5, k6, k7, k8, k9,
  // ---- Function keys --------------------------------------------------------
  kF1, kF2, kF3, kF4, kF5, kF6,
  kF7, kF8, kF9, kF10, kF11, kF12,
  // ---- Arrow keys -----------------------------------------------------------
  kUp, kDown, kLeft, kRight,
  // ---- Modifier keys --------------------------------------------------------
  kLShift, kRShift,
  kLCtrl,  kRCtrl,
  kLAlt,   kRAlt,
  // ---- Special keys ---------------------------------------------------------
  kEscape, kEnter, kSpace, kTab, kBackspace,
  kDelete, kInsert, kHome, kEnd, kPageUp, kPageDown,
  // ---- Sentinel -------------------------------------------------------------
  kUnknown,
};

}  // namespace core
