#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "editor/EditorCommand.h"

namespace abstract { class VideoDevice; }
namespace terrain { class TerrainMaterial; }

namespace editor {

// Reversible paint-brush stroke that snapshots a rectangular splatmap tile
// before and after the stroke.
//
// Execute/Redo restores the post-stroke splatmap state; Undo restores the
// pre-stroke state. Both paths call TerrainMaterial::UploadSplatTile() so the
// rendered terrain reflects the change immediately.
class PaintBrushCommand : public EditorCommand {
 public:
  // Constructs the command with full ownership of the snapshot buffers.
  //
  // (tx, tz) — top-left texel coordinate of the affected tile.
  // (tw, th) — width and height of the tile in texels.
  // pre_snapshot  — row-major RGBA uint8_t tile captured before the stroke.
  // post_snapshot — row-major RGBA uint8_t tile captured after the stroke.
  PaintBrushCommand(terrain::TerrainMaterial* material,
                    abstract::VideoDevice* video,
                    int tx, int tz, int tw, int th,
                    std::vector<uint8_t> pre_snapshot,
                    std::vector<uint8_t> post_snapshot);

  void Execute() override;
  void Undo()    override;
  std::string_view GetDescription() const override { return "Paint Terrain"; }

 private:
  void ApplySnapshot(const std::vector<uint8_t>& snapshot);

  // cppcheck-suppress unusedStructMember
  terrain::TerrainMaterial* material_;
  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice*    video_;
  // cppcheck-suppress unusedStructMember
  int tx_, tz_, tw_, th_;
  // cppcheck-suppress unusedStructMember
  std::vector<uint8_t> pre_snapshot_;
  // cppcheck-suppress unusedStructMember
  std::vector<uint8_t> post_snapshot_;
};

}  // namespace editor
