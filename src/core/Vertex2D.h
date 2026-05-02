#pragma once

#include "core/VertexBase.h"

namespace core {

// 2D vertex — same layout as VertexBase; distinguished by type so render
// pipelines can select the correct shader and buffer stride via VertexType.
class Vertex2D : public VertexBase {
 public:
  using VertexBase::VertexBase;
  Vertex2D() = default;
  Vertex2D(const Vertex2D&) = default;
  Vertex2D& operator=(const Vertex2D&) = default;
};

}  // namespace core
