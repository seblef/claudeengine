#include "mesh/FbxImporter.h"

#include <cmath>
#include <string>

#include <gtest/gtest.h>

#include "mesh/LodData.h"
#include "mesh/MeshData.h"

using namespace mesh;  // NOLINT(build/namespaces)

namespace {

// blender_279_default_6100_ascii.fbx — Blender 2.79 default cube.
// Geometry: 8 positions, 6 quads → triangulated to 12 tris.
// Normals: ByPolygonVertex/Direct (4 normals per face, same direction).
// After fan-triangulation + WeldVertices: 24 unique vertices, 36 indices.
const char kCubeAscii[] =
    UFBX_DATA_DIR "/blender_279_default_6100_ascii.fbx";
const char kMissingFbx[] = UFBX_DATA_DIR "/does_not_exist.fbx";

bool Near(float a, float b, float eps = 1e-4f) {
  return std::fabs(a - b) <= eps;
}

}  // namespace

// ---- Import failure ---------------------------------------------------------

TEST(FbxImporterTest, MissingFileReturnsFalse) {
  FbxImporter importer;
  MeshData mesh;
  EXPECT_FALSE(importer.Import(kMissingFbx, &mesh));
}

// ---- Cube geometry ----------------------------------------------------------

class FbxImporterCubeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    FbxImporter importer;
    ASSERT_TRUE(importer.Import(kCubeAscii, &mesh_));
  }
  MeshData mesh_;
};

TEST_F(FbxImporterCubeTest, OneLod) {
  ASSERT_EQ(mesh_.submeshes.size(), 1u);
  EXPECT_EQ(mesh_.submeshes[0].lods.size(), 1u);
}

TEST_F(FbxImporterCubeTest, VertexCountAfterWeld) {
  // 6 quads × 4 unique vertices per quad = 24 (normals are face-flat,
  // preventing cross-face merging).
  EXPECT_EQ(mesh_.submeshes[0].lods[0].vertices.size(), 24u);
}

TEST_F(FbxImporterCubeTest, IndexCount) {
  // 6 quads × 2 triangles × 3 vertices = 36 indices.
  EXPECT_EQ(mesh_.submeshes[0].lods[0].indices.size(), 36u);
}

TEST_F(FbxImporterCubeTest, NormalsAreUnitLength) {
  for (const auto& v : mesh_.submeshes[0].lods[0].vertices) {
    const float len = std::sqrt(v.normal.x * v.normal.x +
                                v.normal.y * v.normal.y +
                                v.normal.z * v.normal.z);
    EXPECT_TRUE(Near(len, 1.f));
  }
}

TEST_F(FbxImporterCubeTest, AabbContainsOrigin) {
  const auto& aabb = mesh_.submeshes[0].lods[0].aabb;
  EXPECT_LE(aabb.GetMin().x, 0.f);
  EXPECT_LE(aabb.GetMin().y, 0.f);
  EXPECT_LE(aabb.GetMin().z, 0.f);
  EXPECT_GE(aabb.GetMax().x, 0.f);
  EXPECT_GE(aabb.GetMax().y, 0.f);
  EXPECT_GE(aabb.GetMax().z, 0.f);
}
