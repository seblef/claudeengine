#include "renderer/RenderableMesh.h"

namespace renderer {

void RenderableMesh::AddSubmesh(std::unique_ptr<GeometryData> geo,
                                std::unique_ptr<Material> mat) {
  const core::BBox3& sub_bbox = geo->GetBBox();
  if (!bbox_initialized_) {
    local_bbox_       = sub_bbox;
    bbox_initialized_ = true;
  } else {
    local_bbox_ << sub_bbox;
  }
  submeshes_.push_back(std::make_unique<Mesh>(geo.get(), mat.get()));
  geometries_.push_back(std::move(geo));
  materials_.push_back(std::move(mat));
}

size_t RenderableMesh::GetSubmeshCount() const { return submeshes_.size(); }

Mesh* RenderableMesh::GetSubmesh(size_t idx) const {
  return submeshes_[idx].get();
}

const core::BBox3& RenderableMesh::GetLocalBBox() const { return local_bbox_; }

}  // namespace renderer
