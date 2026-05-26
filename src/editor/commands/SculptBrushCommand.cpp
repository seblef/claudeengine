#include "editor/commands/SculptBrushCommand.h"

#include "abstract/VideoDevice.h"
#include "terrain/TerrainData.h"
#include "terrain/TerrainNormalMap.h"
#include "terrain/TerrainRenderer.h"

namespace editor {

SculptBrushCommand::SculptBrushCommand(terrain::TerrainData* data,
                                       terrain::TerrainNormalMap* normal_map,
                                       abstract::VideoDevice* video,
                                       int tx, int tz, int tw, int th,
                                       std::vector<uint16_t> pre_snapshot,
                                       std::vector<uint16_t> post_snapshot)
    : data_(data),
      normal_map_(normal_map),
      video_(video),
      tx_(tx), tz_(tz), tw_(tw), th_(th),
      pre_snapshot_(std::move(pre_snapshot)),
      post_snapshot_(std::move(post_snapshot)) {}

void SculptBrushCommand::ApplySnapshot(const std::vector<uint16_t>& snapshot) {
  for (int z = 0; z < th_; ++z)
    for (int x = 0; x < tw_; ++x)
      data_->SetSample(tx_ + x, tz_ + z, snapshot[static_cast<std::size_t>(z * tw_) + x]);

  normal_map_->UploadTile(video_, tx_, tz_, tw_, th_);

  if (terrain::TerrainRenderer::IsInstanced())
    terrain::TerrainRenderer::Instance().UpdateHeightmapTile(tx_, tz_, tw_, th_, *data_);
}

void SculptBrushCommand::Execute() { ApplySnapshot(post_snapshot_); }
void SculptBrushCommand::Undo()    { ApplySnapshot(pre_snapshot_);  }

}  // namespace editor
