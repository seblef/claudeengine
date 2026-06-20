#pragma once

#include <chrono>
#include <limits>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/Singleton.h"

namespace core {

// Read-only snapshot of one named block's statistics over the last report window.
struct ProfileSample {
  // cppcheck-suppress unusedStructMember
  std::string name;
  // cppcheck-suppress unusedStructMember
  double      avg_ms;
  // cppcheck-suppress unusedStructMember
  double      min_ms;
  // cppcheck-suppress unusedStructMember
  double      max_ms;
  // cppcheck-suppress unusedStructMember
  double      total_ms;
  // cppcheck-suppress unusedStructMember
  uint32_t    calls;
};

// Lightweight selective CPU profiler. Collects timing samples recorded by
// ProfileScope and emits a periodic aggregate report to the log.
//
// Lifecycle:
//   new core::Profiler(interval_s);   // activate
//   core::Profiler::Instance().MarkFrame();  // call once per main-loop tick
//   core::Profiler::Shutdown();        // before Logger::Shutdown()
class Profiler : public Singleton<Profiler> {
 public:
  explicit Profiler(double report_interval_s = 5.0);

  // Called once per main-loop iteration. Drives FPS tracking and fires the
  // periodic log report when the interval elapses.
  void MarkFrame();

  // Returns samples from the last completed report window, sorted by
  // total_ms descending. Empty until the first report fires.
  [[nodiscard]] std::vector<ProfileSample> GetLastReport() const;

  // Returns FPS measured in the last report window. 0 until first report.
  [[nodiscard]] double GetLastFps() const;

  // Called by ProfileScope only; thread-safe.
  void RecordSample(std::string_view name, double elapsed_ms);

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

  // Emits the log report and resets all accumulators.
  void Flush(double elapsed_s);
};

// RAII scoped timer. Records elapsed ms and calls Profiler::RecordSample on
// destruction. No-op when the profiler is not instantiated.
class ProfileScope {
 public:
  explicit ProfileScope(std::string_view name);
  ~ProfileScope();

  ProfileScope(const ProfileScope&)            = delete;
  ProfileScope& operator=(const ProfileScope&) = delete;
  ProfileScope(ProfileScope&&)                 = delete;
  ProfileScope& operator=(ProfileScope&&)      = delete;

 private:
  // cppcheck-suppress unusedStructMember
  std::string_view                      name_;
  std::chrono::steady_clock::time_point start_;
  // cppcheck-suppress unusedStructMember
  bool                                  active_;
};

}  // namespace core

// Instruments the enclosing scope under the given name string.
// Expands to a no-op when the Profiler singleton is not active.
#define PROFILE_SCOPE(name) ::core::ProfileScope profscope_##__LINE__(name)
