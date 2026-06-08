#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "editor/EditorCommand.h"

namespace abstract { class VideoDevice; }
namespace terrain {
class TerrainData;
}  // namespace terrain

namespace editor {

// Reversible sculpt-brush stroke that snapshots a rectangular tile of the
// heightmap before and after the stroke.
//
// Execute/Redo restores the post-stroke state; Undo restores the pre-stroke
// state. Both paths re-upload the affected heightmap and normal-map tiles to
// the GPU so the rendered terrain reflects the change immediately.
class SculptBrushCommand : public EditorCommand {
 public:
  // Constructs the command with full ownership of the snapshot buffers.
  //
  // (tx, tz) — top-left texel coordinate of the affected region.
  // (tw, th) — width and height of the affected region in texels.
  // pre_snapshot  — row-major uint16_t tile captured before the stroke.
  // post_snapshot — row-major uint16_t tile captured after the stroke.
  SculptBrushCommand(terrain::TerrainData* data,
                     abstract::VideoDevice* video,
                     int tx, int tz, int tw, int th,
                     std::vector<uint16_t> pre_snapshot,
                     std::vector<uint16_t> post_snapshot);

  void Execute() override;
  void Undo()    override;
  std::string_view GetDescription() const override { return "Sculpt Terrain"; }

 private:
  void ApplySnapshot(const std::vector<uint16_t>& snapshot);

  // cppcheck-suppress unusedStructMember
  terrain::TerrainData*      data_;
  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice*     video_;
  // cppcheck-suppress unusedStructMember
  int tx_, tz_, tw_, th_;
  // cppcheck-suppress unusedStructMember
  std::vector<uint16_t> pre_snapshot_;
  // cppcheck-suppress unusedStructMember
  std::vector<uint16_t> post_snapshot_;
};

}  // namespace editor
