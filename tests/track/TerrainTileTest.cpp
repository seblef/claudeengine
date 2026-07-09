#include "track/TerrainTile.h"

#include <algorithm>

#include <gtest/gtest.h>

using track::TerrainTile;

TEST(TerrainTileTest, BuildQuadProducesFourVerticesAndTwoTriangles) {
  const auto data = TerrainTile::BuildQuad(4.f, 6.f);
  EXPECT_EQ(data.vertices.size(), 4u);
  EXPECT_EQ(data.indices.size(), 6u);
}

TEST(TerrainTileTest, BuildQuadSpansHalfExtentsInLocalXZ) {
  const auto data = TerrainTile::BuildQuad(4.f, 6.f);

  float min_x = data.vertices[0].position.x, max_x = min_x;
  float min_z = data.vertices[0].position.z, max_z = min_z;
  for (const auto& v : data.vertices) {
    min_x = std::min(min_x, v.position.x);
    max_x = std::max(max_x, v.position.x);
    min_z = std::min(min_z, v.position.z);
    max_z = std::max(max_z, v.position.z);
    EXPECT_FLOAT_EQ(v.position.y, 0.f);
    EXPECT_FLOAT_EQ(v.normal.y, 1.f);
  }
  EXPECT_FLOAT_EQ(min_x, -2.f);
  EXPECT_FLOAT_EQ(max_x,  2.f);
  EXPECT_FLOAT_EQ(min_z, -3.f);
  EXPECT_FLOAT_EQ(max_z,  3.f);
}

TEST(TerrainTileTest, BuildQuadIndicesStayWithinVertexRange) {
  const auto data = TerrainTile::BuildQuad(2.f, 2.f);
  for (uint32_t idx : data.indices)
    EXPECT_LT(idx, data.vertices.size());
}
