#pragma once

#include <string>

namespace core {

// A single GPU timing measurement returned by VideoDevice::CollectGpuTimings().
struct GpuTimeSample {
  // cppcheck-suppress unusedStructMember
  std::string name;
  // cppcheck-suppress unusedStructMember
  double      elapsed_ms;
};

}  // namespace core
