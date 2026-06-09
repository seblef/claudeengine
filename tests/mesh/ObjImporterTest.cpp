#include "mesh/ObjImporter.h"

#include <cmath>
#include <string>

#include <gtest/gtest.h>

#include "mesh/LodData.h"
#include "mesh/MeshData.h"

using namespace mesh;  // NOLINT(build/namespaces)

namespace {

const char kQuadObj[]      = MESH_TEST_FIXTURES_DIR "/quad.obj";
const char kTwoMatsObj[]   = MESH_TEST_FIXTURES_DIR "/two_mats.obj";
const char kMissingObj[]   = MESH_TEST_FIXTURES_DIR "/does_not_exist.obj";

bool Near(float a, float b, float eps = 1e-5f) {
  return std::fabs(a - b) <= eps;
}

}  // namespace

// ---- Import failure ---------------------------------------------------------

TEST(ObjImporterTest, MissingFileReturnsFalse) {
  ObjImporter importer;
  MeshData mesh;
  EXPECT_FALSE(importer.Import(kMissingObj, &mesh));
}

// ---- Quad geometry ----------------------------------------------------------

class ObjImporterQuadTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ObjImporter importer;
    ASSERT_TRUE(importer.Import(kQuadObj, &mesh_));
  }
  MeshData mesh_;
};

TEST_F(ObjImporterQuadTest, VertexCountAfterWeld) {
  // Two triangles share 2 vertices → 4 unique vertices after welding.
  EXPECT_EQ(mesh_.lod.vertices.size(), 4u);
}

TEST_F(ObjImporterQuadTest, IndexCount) {
  // Two triangles = 6 indices.
  EXPECT_EQ(mesh_.lod.indices.size(), 6u);
}

TEST_F(ObjImporterQuadTest, NormalsPointUp) {
  for (const auto& v : mesh_.lod.vertices) {
    EXPECT_TRUE(Near(v.normal.x, 0.f));
    EXPECT_TRUE(Near(v.normal.y, 1.f));
    EXPECT_TRUE(Near(v.normal.z, 0.f));
  }
}

TEST_F(ObjImporterQuadTest, AabbCoversQuad) {
  const auto& aabb = mesh_.lod.aabb;
  // XZ extents: [0,1] × [0,1]; Y is flat at 0.
  EXPECT_TRUE(Near(aabb.GetMin().x, 0.f));
  EXPECT_TRUE(Near(aabb.GetMin().z, 0.f));
  EXPECT_TRUE(Near(aabb.GetMax().x, 1.f));
  EXPECT_TRUE(Near(aabb.GetMax().z, 1.f));
}

TEST_F(ObjImporterQuadTest, SingleSubmesh) {
  // No usemtl in quad.obj → exactly one submesh covering all indices.
  ASSERT_EQ(mesh_.lod.submeshes.size(), 1u);
  EXPECT_EQ(mesh_.lod.submeshes[0].index_offset, 0u);
  EXPECT_EQ(mesh_.lod.submeshes[0].index_count, mesh_.lod.indices.size());
}

// ---- Two-material quad -------------------------------------------------------

class ObjImporterTwoMatsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ObjImporter importer;
    ASSERT_TRUE(importer.Import(kTwoMatsObj, &mesh_));
  }
  MeshData mesh_;
};

TEST_F(ObjImporterTwoMatsTest, TwoSubmeshes) {
  ASSERT_EQ(mesh_.lod.submeshes.size(), 2u);
}

TEST_F(ObjImporterTwoMatsTest, SubmeshNames) {
  EXPECT_EQ(mesh_.lod.submeshes[0].material_name, "red");
  EXPECT_EQ(mesh_.lod.submeshes[1].material_name, "blue");
}

TEST_F(ObjImporterTwoMatsTest, SubmeshRangesAreContiguousAndCoverAll) {
  const auto& s0 = mesh_.lod.submeshes[0];
  const auto& s1 = mesh_.lod.submeshes[1];
  EXPECT_EQ(s0.index_offset, 0u);
  EXPECT_EQ(s1.index_offset, s0.index_count);
  EXPECT_EQ(s0.index_count + s1.index_count, mesh_.lod.indices.size());
}

TEST_F(ObjImporterTwoMatsTest, EachSubmeshHasSixIndices) {
  // Each quad = 2 triangles = 6 indices.
  EXPECT_EQ(mesh_.lod.submeshes[0].index_count, 6u);
  EXPECT_EQ(mesh_.lod.submeshes[1].index_count, 6u);
}
