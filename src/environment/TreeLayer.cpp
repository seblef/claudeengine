#include "environment/TreeLayer.h"

namespace environment {

TreeLayer::TreeLayer(int map_width, int map_height, TreeLayerDesc desc)
    : desc_(std::move(desc)),
      map_width_(map_width),
      map_height_(map_height),
      density_map_(static_cast<std::size_t>(map_width) * map_height, 0u) {}

}  // namespace environment
