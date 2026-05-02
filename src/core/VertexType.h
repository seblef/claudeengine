#pragma once

#include <cstddef>
#include <cstdint>

#include "core/Vertex2D.h"
#include "core/Vertex3D.h"
#include "core/VertexBase.h"

namespace core {

// Identifies the vertex format used by a mesh or draw call.
enum class VertexType : uint8_t {
  kBase = 0,
  k2D,
  k3D,
  kCount
};

// Byte size of each vertex type, indexed by VertexType.
constexpr size_t kVertexSize[] = {
    sizeof(VertexBase),  // kBase
    sizeof(Vertex2D),    // k2D
    sizeof(Vertex3D),    // k3D
};

static_assert(sizeof(kVertexSize) / sizeof(kVertexSize[0]) ==
              static_cast<size_t>(VertexType::kCount),
              "kVertexSize length must match VertexType::kCount");

}  // namespace core
