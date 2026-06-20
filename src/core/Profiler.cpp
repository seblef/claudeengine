#include "core/Profiler.h"

#include <algorithm>

#include <loguru.hpp>

namespace core {

Profiler::Profiler(double report_interval_s)
    : report_interval_s_(report_interval_s),
      interval_start_(Clock::now()) {}

void Profiler::MarkFrame() {
  std::lock_guard<std::mutex> lock(mutex_);
  ++frame_count_;

  const auto   now       = Clock::now();
  const double elapsed_s =
      std::chrono::duration<double>(now - interval_start_).count();

  if (elapsed_s >= report_interval_s_)
    Flush(elapsed_s);
}

void Profiler::RecordSample(std::string_view name, double elapsed_ms) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto& acc    = accumulators_[std::string(name)];
  acc.total_ms += elapsed_ms;
  if (elapsed_ms < acc.min_ms) acc.min_ms = elapsed_ms;
  if (elapsed_ms > acc.max_ms) acc.max_ms = elapsed_ms;
  ++acc.count;
}

std::vector<ProfileSample> Profiler::GetLastReport() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return last_report_;
}

double Profiler::GetLastFps() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return last_fps_;
}

void Profiler::Flush(double elapsed_s) {
  last_fps_ = static_cast<double>(frame_count_) / elapsed_s;

  last_report_.clear();
  last_report_.reserve(accumulators_.size());
  for (const auto& [name, acc] : accumulators_) {
    ProfileSample s;
    s.name     = name;
    s.total_ms = acc.total_ms;
    s.min_ms   = (acc.count > 0) ? acc.min_ms : 0.0;
    s.max_ms   = acc.max_ms;
    s.calls    = acc.count;
    s.avg_ms   = (acc.count > 0) ? acc.total_ms / acc.count : 0.0;
    last_report_.push_back(std::move(s));
  }

  std::sort(last_report_.begin(), last_report_.end(),
            [](const ProfileSample& a, const ProfileSample& b) {
              return a.total_ms > b.total_ms;
            });

  LOG_F(INFO, "[Profiler] === %.1f s window | FPS: %.1f ===",
        elapsed_s, last_fps_);
  for (const auto& s : last_report_) {
    LOG_F(INFO,
          "[Profiler]   %-30s avg=%7.3f ms  min=%7.3f ms  max=%7.3f ms"
          "  calls=%u",
          s.name.c_str(), s.avg_ms, s.min_ms, s.max_ms, s.calls);
  }

  for (auto& [name, acc] : accumulators_) {
    acc.total_ms = 0.0;
    acc.min_ms   = std::numeric_limits<double>::max();
    acc.max_ms   = 0.0;
    acc.count    = 0;
  }
  frame_count_    = 0;
  interval_start_ = Clock::now();
}

// ----- ProfileScope ----------------------------------------------------------

ProfileScope::ProfileScope(std::string_view name)
    : name_(name),
      start_(std::chrono::steady_clock::now()),
      active_(Profiler::IsInstanced()) {}

ProfileScope::~ProfileScope() {
  if (!active_) return;
  const auto   now        = std::chrono::steady_clock::now();
  const double elapsed_ms =
      std::chrono::duration<double, std::milli>(now - start_).count();
  Profiler::Instance().RecordSample(name_, elapsed_ms);
}

}  // namespace core
