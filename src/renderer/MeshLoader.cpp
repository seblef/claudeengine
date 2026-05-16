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
  if (lod.vertices.size() > 65535u) {
    LOG_F(ERROR, "MeshLoader: mesh exceeds 16-bit index limit (%zu vertices)",
          lod.vertices.size());
    return nullptr;
  }
  std::vector<uint16_t> idx16(lod.indices.size());
  std::transform(lod.indices.begin(), lod.indices.end(), idx16.begin(),
                 [](uint32_t i) { return static_cast<uint16_t>(i); });
  return std::make_unique<GeometryData>(
      video,
      static_cast<int>(lod.vertices.size()), lod.vertices.data(),
      static_cast<int>(lod.indices.size()) / 3, idx16.data());
}

}  // namespace

std::unique_ptr<GeometryData> MeshLoader::LoadGeometry(const std::string& path,
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
    return nullptr;
  }

  if (!ok) return nullptr;
  return MakeGeometry(video, data.lod);
}

}  // namespace renderer
