#include "physics/PhysicsShapeCache.h"

#include <cinttypes>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include <loguru.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Core/Core.h>
#include <Jolt/Core/StreamWrapper.h>
#include <Jolt/Physics/Collision/PhysicsMaterial.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

namespace physics {

namespace {

// FNV-1a 64-bit hash constants.
constexpr uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr uint64_t kFnvPrime  = 1099511628211ULL;

}  // namespace

uint64_t HashBytes(const void* data, size_t byte_count) {
    uint64_t hash = kFnvOffset;
    const auto* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < byte_count; ++i) {
        hash ^= p[i];
        hash *= kFnvPrime;
    }
    return hash;
}

std::string ShapeCachePath(const std::string& cache_dir,
                            const std::string& subdir,
                            uint64_t           content_hash) {
    char name[64];
    std::snprintf(name, sizeof(name), "%016" PRIx64 "_jolt%d.%d.bin",
                  content_hash,
                  JPH_VERSION_MAJOR, JPH_VERSION_MINOR);
    namespace fs = std::filesystem;
    return (fs::path(cache_dir) / subdir / name).string();
}

JPH::ShapeRefC TryLoadShape(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open())
        return nullptr;

    JPH::StreamInWrapper jolt_stream(stream);
    JPH::Shape::IDToShapeMap    shape_map;
    JPH::Shape::IDToMaterialMap mat_map;
    JPH::Shape::ShapeResult result =
        JPH::Shape::sRestoreWithChildren(jolt_stream, shape_map, mat_map);
    if (result.HasError()) {
        LOG_F(WARNING, "PhysicsShapeCache: failed to restore '%s': %s",
              path.c_str(), result.GetError().c_str());
        return nullptr;
    }
    LOG_F(INFO, "PhysicsShapeCache: hit  %s", path.c_str());
    return result.Get();
}

void SaveShape(const JPH::ShapeRefC& shape,
               const std::string&    path,
               bool                  cleanup_siblings) {
    namespace fs = std::filesystem;

    // Ensure parent directory exists.
    const fs::path file_path(path);
    std::error_code ec;
    fs::create_directories(file_path.parent_path(), ec);
    if (ec) {
        LOG_F(ERROR, "PhysicsShapeCache: cannot create dir '%s': %s",
              file_path.parent_path().string().c_str(), ec.message().c_str());
        return;
    }

    // Delete stale siblings (only for terrain: one file per terrain dir).
    if (cleanup_siblings) {
        for (const auto& entry :
             fs::directory_iterator(file_path.parent_path(), ec)) {
            if (ec) break;
            if (entry.path().extension() == ".bin" && entry.path() != file_path) {
                fs::remove(entry.path(), ec);
                if (!ec)
                    LOG_F(INFO, "PhysicsShapeCache: removed stale %s",
                          entry.path().string().c_str());
            }
        }
    }

    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream.is_open()) {
        LOG_F(ERROR, "PhysicsShapeCache: cannot open '%s' for writing",
              path.c_str());
        return;
    }

    JPH::StreamOutWrapper jolt_stream(stream);
    JPH::Shape::ShapeToIDMap    shape_map;
    JPH::Shape::MaterialToIDMap mat_map;
    shape->SaveWithChildren(jolt_stream, shape_map, mat_map);

    if (stream.fail()) {
        LOG_F(ERROR, "PhysicsShapeCache: write error on '%s'", path.c_str());
        fs::remove(file_path, ec);  // don't leave a corrupt file
        return;
    }
    LOG_F(INFO, "PhysicsShapeCache: saved %s", path.c_str());
}

}  // namespace physics
