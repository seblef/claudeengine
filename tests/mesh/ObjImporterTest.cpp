#include "mesh/ObjImporter.h"

#include <cmath>
#include <string>

#include <gtest/gtest.h>

#include "mesh/LodData.h"
#include "mesh/MeshData.h"
#include "mesh/SubmeshData.h"

using namespace mesh;  // NOLINT(build/namespaces)

namespace {

const char kQuadObj[]    = MESH_TEST_FIXTURES_DIR "/quad.obj";
const char kMissingObj[] = MESH_TEST_FIXTURES_DIR "/does_not_exist.obj";

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

TEST_F(ObjImporterQuadTest, OneLod) {
  ASSERT_EQ(mesh_.submeshes.size(), 1u);
  EXPECT_EQ(mesh_.submeshes[0].lods.size(), 1u);
}

TEST_F(ObjImporterQuadTest, VertexCountAfterWeld) {
  // Two triangles share 2 vertices → 4 unique vertices after welding.
  EXPECT_EQ(mesh_.submeshes[0].lods[0].vertices.size(), 4u);
}

TEST_F(ObjImporterQuadTest, IndexCount) {
  // Two triangles = 6 indices.
  EXPECT_EQ(mesh_.submeshes[0].lods[0].indices.size(), 6u);
}

TEST_F(ObjImporterQuadTest, NormalsPointUp) {
  for (const auto& v : mesh_.submeshes[0].lods[0].vertices) {
    EXPECT_TRUE(Near(v.normal.x, 0.f));
    EXPECT_TRUE(Near(v.normal.y, 1.f));
    EXPECT_TRUE(Near(v.normal.z, 0.f));
  }
}

TEST_F(ObjImporterQuadTest, AabbCoversQuad) {
  const auto& aabb = mesh_.submeshes[0].lods[0].aabb;
  // XZ extents: [0,1] × [0,1]; Y is flat at 0.
  EXPECT_TRUE(Near(aabb.GetMin().x, 0.f));
  EXPECT_TRUE(Near(aabb.GetMin().z, 0.f));
  EXPECT_TRUE(Near(aabb.GetMax().x, 1.f));
  EXPECT_TRUE(Near(aabb.GetMax().z, 1.f));
}
