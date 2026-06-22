#pragma once

namespace editor {

// Dockable panel that displays real-time CPU profiling data captured by
// core::Profiler during play mode.
//
// Shows FPS and a per-scope table (avg / min / max / total ms, call count) with
// colour-coded rows: green (avg < 2 ms), yellow (2–8 ms), red (> 8 ms).
// Render() is a no-op when the Profiler singleton is not active.
//
// Pure UI — reads Profiler state each frame; holds no mutable state of its own.
class ProfilerPanel {
 public:
  ProfilerPanel() = default;

  // Renders profiler data inside the current ImGui window.
  // Must be called between ImGui::Begin() / ImGui::End() by the caller.
  void Render();
};

}  // namespace editor
