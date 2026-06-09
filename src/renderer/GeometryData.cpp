#include "renderer/GeometryData.h"

#include "abstract/BufferUsage.h"
#include "abstract/IndexType.h"
#include "core/VertexType.h"

namespace renderer {

namespace {

core::BBox3 ComputeBBox(const core::Vertex3D* vertices, int num_vertices) {
  core::BBox3 bbox(vertices[0].position, vertices[0].position);
  for (int i = 1; i < num_vertices; ++i) {
    bbox << vertices[i].position;
  }
  return bbox;
}

}  // namespace

GeometryData::GeometryData(abstract::VideoDevice* video,
                           int num_vertices, const core::Vertex3D* vertices,
                           int num_triangles, const uint32_t* indices)
    : vertex_buffer_(video->CreateVertexBuffer(
          core::VertexType::k3D, num_vertices,
          abstract::BufferUsage::kImmutable, vertices)),
      index_buffer_(video->CreateIndexBuffer(
          abstract::IndexType::kUInt32, num_triangles * 3,
          abstract::BufferUsage::kImmutable, indices)),
      bbox_(ComputeBBox(vertices, num_vertices)),
      num_triangles_(num_triangles) {}

void GeometryData::Set() const {
  vertex_buffer_->Bind();
  index_buffer_->Bind();
}

}  // namespace renderer
