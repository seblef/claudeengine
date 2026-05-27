#include "editor/commands/PaintBrushCommand.h"

#include "abstract/VideoDevice.h"
#include "terrain/TerrainMaterial.h"

namespace editor {

PaintBrushCommand::PaintBrushCommand(terrain::TerrainMaterial* material,
                                     abstract::VideoDevice* video,
                                     int tx, int tz, int tw, int th,
                                     std::vector<uint8_t> pre_snapshot,
                                     std::vector<uint8_t> post_snapshot)
    : material_(material),
      video_(video),
      tx_(tx), tz_(tz), tw_(tw), th_(th),
      pre_snapshot_(std::move(pre_snapshot)),
      post_snapshot_(std::move(post_snapshot)) {}

void PaintBrushCommand::ApplySnapshot(const std::vector<uint8_t>& snapshot) {
  for (int z = 0; z < th_; ++z) {
    for (int x = 0; x < tw_; ++x) {
      const std::size_t si = static_cast<std::size_t>(z * tw_ + x) * 4;
      for (int c = 0; c < 4; ++c)
        material_->SetSplatWeight(tx_ + x, tz_ + z, c,
                                  snapshot[si + c] / 255.f);
    }
  }
  material_->UploadSplatTile(video_, tx_, tz_, tw_, th_);
}

void PaintBrushCommand::Execute() { ApplySnapshot(post_snapshot_); }
void PaintBrushCommand::Undo()    { ApplySnapshot(pre_snapshot_);  }

}  // namespace editor
