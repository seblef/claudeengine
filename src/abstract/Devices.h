#pragma once

#include "core/EventManager.h"

namespace abstract {

class VideoDevice;

// Abstract base for all platform device managers.
//
// One concrete subclass exists per supported platform. It overrides Update()
// to pump OS messages into the EventManager queue each frame, and GetWindow()
// to expose the native window handle to render backends.
class Devices {
 public:
  // Ensures the EventManager singleton is initialised before any platform
  // driver starts publishing events.
  Devices() { (void)core::EventManager::Instance(); }
  virtual ~Devices() = default;

  // Pumps OS messages and publishes input/window events for the current frame.
  virtual void Update() = 0;

  // Returns the native window handle (HWND, Display*, NSWindow*, etc.).
  // Cast to the platform-specific type at the call site.
  [[nodiscard]] virtual void* GetWindow() = 0;

  // Returns the GPU backend owned by this device manager.
  [[nodiscard]] VideoDevice* GetVideoDevice() const { return video_device_; }

  // Captures or releases the OS cursor. When captured the cursor is hidden and
  // confined, and the platform delivers unbounded relative mouse events — this
  // keeps camera look-direction updating even at window/screen boundaries.
  // Default implementation is a no-op; override in concrete platform drivers.
  virtual void SetCursorCapture(bool /*capture*/) {}

 protected:
  VideoDevice* video_device_ = nullptr;
};

}  // namespace abstract
