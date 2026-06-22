#pragma once

#include <chrono>
#include <limits>
#include <mutex>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/GpuTimeSample.h"
#include "core/Profiler.h"
#include "core/Singleton.h"

namespace core {

// Aggregates GPU timing samples collected from the graphics backend and emits
// periodic reports to the log, mirroring Profiler for the GPU timeline.
//
// Lifecycle:
//   new core::GpuProfiler(interval_s);            // activate
//   core::GpuProfiler::Instance().RecordSamples(  // once per frame, before MarkFrame
//       video->CollectGpuTimings());
//   core::GpuProfiler::Instance().MarkFrame();     // drives flush
//   core::GpuProfiler::Shutdown();                 // before Logger::Shutdown()
class GpuProfiler : public Singleton<GpuProfiler> {
 public:
  explicit GpuProfiler(double report_interval_s = 5.0);

  // Called once per main-loop iteration to track FPS and fire the periodic
  // log report when the interval elapses.
  void MarkFrame();

  // Ingests a batch of GPU timing samples from VideoDevice::CollectGpuTimings().
  // Thread-safe; samples arrive in bulk from the main thread only.
  void RecordSamples(std::span<const GpuTimeSample> samples);

  // Returns GPU samples from the last completed report window, sorted by
  // total_ms descending.  Empty until the first report fires.
  [[nodiscard]] std::vector<ProfileSample> GetLastReport() const;

  // Returns FPS measured in the last report window. 0 until first report.
  [[nodiscard]] double GetLastFps() const;

  // Returns the report aggregation interval in seconds (set at construction).
  [[nodiscard]] double GetReportInterval() const { return report_interval_s_; }

 private:
  struct Accumulator {
    double   total_ms = 0.0;
    // cppcheck-suppress unusedStructMember
    double   min_ms   = std::numeric_limits<double>::max();
    // cppcheck-suppress unusedStructMember
    double   max_ms   = 0.0;
    // cppcheck-suppress unusedStructMember
    uint32_t count    = 0;
  };

  using Clock     = std::chrono::steady_clock;
  using TimePoint = Clock::time_point;

  // cppcheck-suppress unusedStructMember
  double    report_interval_s_;
  TimePoint interval_start_;
  uint32_t  frame_count_ = 0;

  double last_fps_ = 0.0;
  // cppcheck-suppress unusedStructMember
  std::vector<ProfileSample> last_report_;

  // cppcheck-suppress unusedStructMember
  std::unordered_map<std::string, Accumulator> accumulators_;
  mutable std::mutex mutex_;

  void Flush(double elapsed_s);
};

}  // namespace core
