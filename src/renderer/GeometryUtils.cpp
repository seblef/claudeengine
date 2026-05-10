#include "renderer/GeometryUtils.h"

#include <cmath>
#include <vector>

#include "core/Vertex3D.h"

namespace renderer {

std::unique_ptr<GeometryData> CreateQuad(abstract::VideoDevice* video) {
  const core::Vertex3D verts[4] = {
    {{-1.f, -1.f, 0.f}, {}, {}, {}, {}},
    {{ 1.f, -1.f, 0.f}, {}, {}, {}, {}},
    {{ 1.f,  1.f, 0.f}, {}, {}, {}, {}},
    {{-1.f,  1.f, 0.f}, {}, {}, {}, {}},
  };
  const uint16_t idx[6] = {0, 1, 2, 0, 2, 3};
  return std::make_unique<GeometryData>(video, 4, verts, 2, idx);
}

std::unique_ptr<GeometryData> CreateSphere(abstract::VideoDevice* video,
                                            int stacks, int rings) {
  const int num_verts = (stacks + 1) * (rings + 1);
  const int num_tris  = stacks * rings * 2;

  std::vector<core::Vertex3D> verts;
  verts.reserve(num_verts);
  for (int s = 0; s <= stacks; ++s) {
    const float phi     = static_cast<float>(M_PI) * static_cast<float>(s) /
                          static_cast<float>(stacks);
    const float sin_phi = std::sin(phi);
    const float cos_phi = std::cos(phi);
    for (int r = 0; r <= rings; ++r) {
      const float theta = 2.f * static_cast<float>(M_PI) *
                          static_cast<float>(r) / static_cast<float>(rings);
      verts.push_back({{sin_phi * std::cos(theta), cos_phi,
                        sin_phi * std::sin(theta)},
                       {}, {}, {}, {}});
    }
  }

  std::vector<uint16_t> idx;
  idx.reserve(num_tris * 3);
  for (int s = 0; s < stacks; ++s) {
    for (int r = 0; r < rings; ++r) {
      const uint16_t a = static_cast<uint16_t>(s * (rings + 1) + r);
      const uint16_t b = static_cast<uint16_t>(a + 1);
      const uint16_t c = static_cast<uint16_t>(a + rings + 1);
      const uint16_t d = static_cast<uint16_t>(c + 1);
      idx.insert(idx.end(), {a, c, b, b, c, d});
    }
  }

  return std::make_unique<GeometryData>(
      video, num_verts, verts.data(), num_tris, idx.data());
}

std::unique_ptr<GeometryData> CreateCone(abstract::VideoDevice* video, int n) {
  const int num_verts = n + 2;  // apex + N base + base center
  const int num_tris  = n * 2;

  std::vector<core::Vertex3D> verts;
  verts.reserve(num_verts);

  verts.push_back({{0.f, 0.f, 0.f}, {}, {}, {}, {}});  // apex
  for (int i = 0; i < n; ++i) {
    const float a = 2.f * static_cast<float>(M_PI) * static_cast<float>(i) /
                    static_cast<float>(n);
    verts.push_back({{std::cos(a), std::sin(a), 1.f}, {}, {}, {}, {}});
  }
  verts.push_back({{0.f, 0.f, 1.f}, {}, {}, {}, {}});  // base center

  std::vector<uint16_t> idx;
  idx.reserve(num_tris * 3);
  for (int i = 0; i < n; ++i) {
    const uint16_t cur  = static_cast<uint16_t>(1 + i);
    const uint16_t next = static_cast<uint16_t>(1 + (i + 1) % n);
    idx.insert(idx.end(), {0, cur, next});                                   // side
    idx.insert(idx.end(), {static_cast<uint16_t>(n + 1), next, cur});        // base cap
  }

  return std::make_unique<GeometryData>(
      video, num_verts, verts.data(), num_tris, idx.data());
}

std::unique_ptr<GeometryData> CreatePyramid(abstract::VideoDevice* video) {
  const core::Vertex3D verts[5] = {
    {{ 0.f,   0.f,  0.f}, {}, {}, {}, {}},  // 0: apex
    {{-0.5f, -0.5f, 1.f}, {}, {}, {}, {}},  // 1: base BL
    {{ 0.5f, -0.5f, 1.f}, {}, {}, {}, {}},  // 2: base BR
    {{ 0.5f,  0.5f, 1.f}, {}, {}, {}, {}},  // 3: base TR
    {{-0.5f,  0.5f, 1.f}, {}, {}, {}, {}},  // 4: base TL
  };
  const uint16_t idx[18] = {
    0, 1, 2,  // side bottom
    0, 2, 3,  // side right
    0, 3, 4,  // side top
    0, 4, 1,  // side left
    1, 3, 2,  // base tri 0
    1, 4, 3,  // base tri 1
  };
  return std::make_unique<GeometryData>(video, 5, verts, 6, idx);
}

}  // namespace renderer
