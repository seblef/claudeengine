#include "gldevices/GLDevices.h"

#include "core/Event.h"
#include "core/EventManager.h"
#include "core/EventType.h"
#include "core/MouseButton.h"

#include <GLFW/glfw3.h>

namespace gldevices {

GLDevices::GLDevices(int width, int height, bool fullscreen) {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWmonitor* monitor = fullscreen ? glfwGetPrimaryMonitor() : nullptr;
  window_ = glfwCreateWindow(width, height, "ClaudeEngine", monitor, nullptr);
  glfwMakeContextCurrent(window_);

  glfwSetWindowCloseCallback(window_,   OnWindowClose);
  glfwSetWindowFocusCallback(window_,   OnWindowFocus);
  glfwSetWindowIconifyCallback(window_, OnWindowIconify);
  glfwSetCursorPosCallback(window_,     OnCursorPos);
  glfwSetScrollCallback(window_,        OnScroll);
  glfwSetMouseButtonCallback(window_,   OnMouseButton);
  glfwSetKeyCallback(window_,           OnKey);
}

GLDevices::~GLDevices() {
  glfwDestroyWindow(window_);
  glfwTerminate();
}

void GLDevices::Update() {
  glfwPollEvents();
}

void* GLDevices::GetWindow() {
  return window_;
}

// ---- Callbacks -------------------------------------------------------------

void GLDevices::OnWindowClose(GLFWwindow*) {
  core::EventManager::Instance().Publish(core::Event(core::EventType::kWindowClose));
}

void GLDevices::OnWindowFocus(GLFWwindow*, int focused) {
  core::EventType type = focused ? core::EventType::kWindowGetFocus
                                 : core::EventType::kWindowLostFocus;
  core::EventManager::Instance().Publish(core::Event(type));
}

void GLDevices::OnWindowIconify(GLFWwindow*, int iconified) {
  core::EventType type = iconified ? core::EventType::kWindowMinimized
                                   : core::EventType::kWindowRestored;
  core::EventManager::Instance().Publish(core::Event(type));
}

void GLDevices::OnCursorPos(GLFWwindow*, double x, double y) {
  core::EventManager::Instance().Publish(
      core::Event::MouseMoved(static_cast<float>(x), static_cast<float>(y)));
}

void GLDevices::OnScroll(GLFWwindow*, double /*xoffset*/, double yoffset) {
  core::EventManager::Instance().Publish(
      core::Event::MouseWheel(static_cast<float>(yoffset)));
}

void GLDevices::OnMouseButton(GLFWwindow*, int button, int action, int /*mods*/) {
  if (action == GLFW_PRESS || action == GLFW_RELEASE) {
    core::EventType type = (action == GLFW_PRESS) ? core::EventType::kMouseButtonDown
                                                   : core::EventType::kMouseButtonUp;
    core::MouseButton btn = (button == GLFW_MOUSE_BUTTON_LEFT) ? core::MouseButton::kLeft
                                                                : core::MouseButton::kRight;
    core::EventManager::Instance().Publish(core::Event::MouseButton(type, btn));
  }
}

void GLDevices::OnKey(GLFWwindow*, int key, int /*scancode*/, int action, int /*mods*/) {
  if (action == GLFW_PRESS || action == GLFW_REPEAT || action == GLFW_RELEASE) {
    core::EventType type = (action == GLFW_RELEASE) ? core::EventType::kKeyUp
                                                     : core::EventType::kKeyDown;
    core::EventManager::Instance().Publish(core::Event::Key(type, GlfwKeyToKey(key)));
  }
}

// ---- Key mapping -----------------------------------------------------------

core::Key GLDevices::GlfwKeyToKey(int k) {
  switch (k) {
    case GLFW_KEY_A: return core::Key::kA; case GLFW_KEY_B: return core::Key::kB;
    case GLFW_KEY_C: return core::Key::kC; case GLFW_KEY_D: return core::Key::kD;
    case GLFW_KEY_E: return core::Key::kE; case GLFW_KEY_F: return core::Key::kF;
    case GLFW_KEY_G: return core::Key::kG; case GLFW_KEY_H: return core::Key::kH;
    case GLFW_KEY_I: return core::Key::kI; case GLFW_KEY_J: return core::Key::kJ;
    case GLFW_KEY_K: return core::Key::kK; case GLFW_KEY_L: return core::Key::kL;
    case GLFW_KEY_M: return core::Key::kM; case GLFW_KEY_N: return core::Key::kN;
    case GLFW_KEY_O: return core::Key::kO; case GLFW_KEY_P: return core::Key::kP;
    case GLFW_KEY_Q: return core::Key::kQ; case GLFW_KEY_R: return core::Key::kR;
    case GLFW_KEY_S: return core::Key::kS; case GLFW_KEY_T: return core::Key::kT;
    case GLFW_KEY_U: return core::Key::kU; case GLFW_KEY_V: return core::Key::kV;
    case GLFW_KEY_W: return core::Key::kW; case GLFW_KEY_X: return core::Key::kX;
    case GLFW_KEY_Y: return core::Key::kY; case GLFW_KEY_Z: return core::Key::kZ;

    case GLFW_KEY_0: return core::Key::k0; case GLFW_KEY_1: return core::Key::k1;
    case GLFW_KEY_2: return core::Key::k2; case GLFW_KEY_3: return core::Key::k3;
    case GLFW_KEY_4: return core::Key::k4; case GLFW_KEY_5: return core::Key::k5;
    case GLFW_KEY_6: return core::Key::k6; case GLFW_KEY_7: return core::Key::k7;
    case GLFW_KEY_8: return core::Key::k8; case GLFW_KEY_9: return core::Key::k9;

    case GLFW_KEY_F1:  return core::Key::kF1;  case GLFW_KEY_F2:  return core::Key::kF2;
    case GLFW_KEY_F3:  return core::Key::kF3;  case GLFW_KEY_F4:  return core::Key::kF4;
    case GLFW_KEY_F5:  return core::Key::kF5;  case GLFW_KEY_F6:  return core::Key::kF6;
    case GLFW_KEY_F7:  return core::Key::kF7;  case GLFW_KEY_F8:  return core::Key::kF8;
    case GLFW_KEY_F9:  return core::Key::kF9;  case GLFW_KEY_F10: return core::Key::kF10;
    case GLFW_KEY_F11: return core::Key::kF11; case GLFW_KEY_F12: return core::Key::kF12;

    case GLFW_KEY_UP:    return core::Key::kUp;
    case GLFW_KEY_DOWN:  return core::Key::kDown;
    case GLFW_KEY_LEFT:  return core::Key::kLeft;
    case GLFW_KEY_RIGHT: return core::Key::kRight;

    case GLFW_KEY_LEFT_SHIFT:    return core::Key::kLShift;
    case GLFW_KEY_RIGHT_SHIFT:   return core::Key::kRShift;
    case GLFW_KEY_LEFT_CONTROL:  return core::Key::kLCtrl;
    case GLFW_KEY_RIGHT_CONTROL: return core::Key::kRCtrl;
    case GLFW_KEY_LEFT_ALT:      return core::Key::kLAlt;
    case GLFW_KEY_RIGHT_ALT:     return core::Key::kRAlt;

    case GLFW_KEY_ESCAPE:    return core::Key::kEscape;
    case GLFW_KEY_ENTER:     return core::Key::kEnter;
    case GLFW_KEY_SPACE:     return core::Key::kSpace;
    case GLFW_KEY_TAB:       return core::Key::kTab;
    case GLFW_KEY_BACKSPACE: return core::Key::kBackspace;
    case GLFW_KEY_DELETE:    return core::Key::kDelete;
    case GLFW_KEY_INSERT:    return core::Key::kInsert;
    case GLFW_KEY_HOME:      return core::Key::kHome;
    case GLFW_KEY_END:       return core::Key::kEnd;
    case GLFW_KEY_PAGE_UP:   return core::Key::kPageUp;
    case GLFW_KEY_PAGE_DOWN: return core::Key::kPageDown;

    default: return core::Key::kUnknown;
  }
}

}  // namespace gldevices
