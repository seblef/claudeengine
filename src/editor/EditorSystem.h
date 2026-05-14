#pragma once

#include <memory>

#include "abstract/Devices.h"
#include "abstract/VideoDevice.h"
#include "core/Singleton.h"

namespace editor {

class EditorWindow;

// Singleton orchestrating the editor loop.
//
// Initialises ImGui (GLFW + OpenGL3 backends) in the constructor and tears
// it down in the destructor. Each frame: pumps OS events, runs one ImGui
// frame, delegates rendering to EditorWindow, then presents the draw data.
//
// Lifecycle: new EditorSystem(devices) → Run() → Shutdown().
class EditorSystem : public core::Singleton<EditorSystem> {
 public:
  // Initialises ImGui and creates the EditorWindow.
  // devices must outlive this EditorSystem.
  explicit EditorSystem(abstract::Devices* devices);

  // Shuts down ImGui backends and destroys the context.
  ~EditorSystem();

  // Enters the main editor loop; returns when Stop() is called or the window
  // is closed.
  void Run();

  // Signals the loop to exit after the current frame.
  void Stop();

  // Returns false after Stop() has been called or a kWindowClose event is
  // received.
  [[nodiscard]] bool IsRunning() const { return running_; }

 private:
  // cppcheck-suppress unusedStructMember
  abstract::Devices*            devices_;
  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice*        video_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<EditorWindow> editor_window_;
  bool                          running_ = true;
};

}  // namespace editor
