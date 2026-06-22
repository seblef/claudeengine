#pragma once

#include <string_view>

#include "abstract/VideoDevice.h"

namespace abstract {

// RAII GPU timer scope. Calls VideoDevice::BeginGpuTimer on construction and
// VideoDevice::EndGpuTimer on destruction. No-op when video is null.
//
// Do not nest instances: OpenGL GL_TIME_ELAPSED queries cannot overlap.
// Sequential scopes within the same function are safe.
class GpuProfileScope {
 public:
  inline GpuProfileScope(VideoDevice* video, std::string_view name)
      : video_(video) {
    if (video_) video_->BeginGpuTimer(name);
  }

  inline ~GpuProfileScope() {
    if (video_) video_->EndGpuTimer();
  }

  GpuProfileScope(const GpuProfileScope&)            = delete;
  GpuProfileScope& operator=(const GpuProfileScope&) = delete;
  GpuProfileScope(GpuProfileScope&&)                 = delete;
  GpuProfileScope& operator=(GpuProfileScope&&)      = delete;

 private:
  // cppcheck-suppress unusedStructMember
  VideoDevice* video_;
};

}  // namespace abstract

// Instruments the enclosing scope on the GPU under the given name string.
// Expands to a no-op when video is null or the backend does not support
// GPU timer queries.
#define GPU_PROFILE_SCOPE(video, name) \
  ::abstract::GpuProfileScope gpu_profscope_##__LINE__((video), (name))
