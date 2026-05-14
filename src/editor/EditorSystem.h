#pragma once

#include "abstract/Devices.h"
#include "core/Singleton.h"

namespace editor {

// Singleton orchestrating the editor loop.
//
// Owns the main Run() loop, ImGui lifecycle (init/shutdown), and top-level
// editor subsystems (EditorWindow, EditorScene, EditorViewport).
//
// Lifecycle: new EditorSystem(devices) → Run() → Shutdown().
class EditorSystem : public core::Singleton<EditorSystem> {
 public:
  // devices must outlive this EditorSystem.
  explicit EditorSystem(abstract::Devices* devices);

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
  abstract::Devices* devices_;
  // cppcheck-suppress unusedStructMember
  bool running_ = true;
};

}  // namespace editor
