#include "track/TerrainTile.h"

namespace track {

TileMeshData TerrainTile::BuildQuad(float width, float length) {
  const float hw = width  * 0.5f;
  const float hl = length * 0.5f;

  static const core::Vec3f kUp{0.f, 1.f, 0.f};
  static const core::Vec3f kTangent{1.f, 0.f, 0.f};
  static const core::Vec3f kBinormal{0.f, 0.f, 1.f};

  TileMeshData data;
  data.vertices = {
      core::Vertex3D({-hw, 0.f, -hl}, kUp, kBinormal, kTangent, {0.f, 0.f}),
      core::Vertex3D({ hw, 0.f, -hl}, kUp, kBinormal, kTangent, {1.f, 0.f}),
      core::Vertex3D({-hw, 0.f,  hl}, kUp, kBinormal, kTangent, {0.f, 1.f}),
      core::Vertex3D({ hw, 0.f,  hl}, kUp, kBinormal, kTangent, {1.f, 1.f}),
  };
  data.indices = {0, 1, 2, 2, 1, 3};
  return data;
}

}  // namespace track
