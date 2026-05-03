#pragma once

#include "abstract/Devices.h"
#include "core/Key.h"

struct GLFWwindow;  // opaque forward declaration — keeps GLFW headers out of callers

namespace gldevices {

// Concrete Devices implementation backed by GLFW.
//
// Creates an OpenGL 4.6 core-profile context window and registers GLFW
// callbacks that translate OS events into core::Event objects published to
// the EventManager singleton.
class GLDevices : public abstract::Devices {
 public:
  // Creates the GLFW window and registers all input callbacks.
  // Pass fullscreen=false for a standard desktop window.
  GLDevices(int width, int height, bool fullscreen);
  ~GLDevices() override;

  // Calls glfwPollEvents(), which triggers the registered callbacks and
  // fills the EventManager queue for the current frame.
  void Update() override;

  // Returns the underlying GLFWwindow* as void*. Cast at the call site.
  [[nodiscard]] void* GetWindow() override;

 private:
  GLFWwindow* window_ = nullptr;

  // Maps a GLFW_KEY_* constant to the platform-independent core::Key.
  static core::Key GlfwKeyToKey(int glfw_key);

  // GLFW callback statics — publish events directly to EventManager.
  static void OnWindowClose(GLFWwindow* w);
  static void OnWindowFocus(GLFWwindow* w, int focused);
  static void OnWindowIconify(GLFWwindow* w, int iconified);
  static void OnCursorPos(GLFWwindow* w, double x, double y);
  static void OnScroll(GLFWwindow* w, double xoffset, double yoffset);
  static void OnMouseButton(GLFWwindow* w, int button, int action, int mods);
  static void OnKey(GLFWwindow* w, int key, int scancode, int action, int mods);
};

}  // namespace gldevices
