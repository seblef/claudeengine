#include "renderer/MeshLoader.h"

#include <algorithm>
#include <string>
#include <vector>

#include "mesh/EmeshReader.h"
#include "mesh/FbxImporter.h"
#include "mesh/LodData.h"
#include "mesh/MeshData.h"
#include "mesh/ObjImporter.h"

#include <loguru.hpp>

namespace renderer {

namespace {

std::string Extension(const std::string& path) {
  const auto dot = path.rfind('.');
  if (dot == std::string::npos) return {};
  std::string ext = path.substr(dot);
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return ext;
}

std::unique_ptr<GeometryData> MakeGeometry(abstract::VideoDevice* video,
                                           const mesh::LodData& lod) {
  return std::make_unique<GeometryData>(
      video,
      static_cast<int>(lod.vertices.size()), lod.vertices.data(),
      static_cast<int>(lod.indices.size()) / 3, lod.indices.data(),
      lod.submeshes);
}

}  // namespace

std::optional<MeshLoadResult> MeshLoader::Load(const std::string& path,
                                                abstract::VideoDevice* video) {
  const std::string ext = Extension(path);
  mesh::MeshData data;
  bool ok = false;

  if (ext == ".obj") {
    ok = mesh::ObjImporter{}.Import(path, &data);
  } else if (ext == ".fbx") {
    ok = mesh::FbxImporter{}.Import(path, &data);
  } else if (ext == ".emesh") {
    ok = mesh::EmeshReader{}.Read(path, &data);
  } else {
    LOG_F(ERROR, "MeshLoader: unsupported extension '%s' for '%s'",
          ext.c_str(), path.c_str());
    return std::nullopt;
  }

  if (!ok) return std::nullopt;

  MeshLoadResult result;
  result.geometry = MakeGeometry(video, data.lod);
  result.cpu.indices = data.lod.indices;
  result.cpu.positions.reserve(data.lod.vertices.size());
  for (const auto& v : data.lod.vertices)
    result.cpu.positions.push_back(v.position);

  return result;
}

std::unique_ptr<GeometryData> MeshLoader::LoadGeometry(const std::string& path,
                                                        abstract::VideoDevice* video) {
  auto r = Load(path, video);
  return r ? std::move(r->geometry) : nullptr;
}

}  // namespace renderer
