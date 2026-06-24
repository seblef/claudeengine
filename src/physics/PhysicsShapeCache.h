// Private header — only #include'd by PhysicsSystem.cpp. Not part of the
// public physics API; Jolt types must not escape into public headers.
#pragma once

#include <cstdint>
#include <string>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

namespace physics {

/// 64-bit FNV-1a hash of an arbitrary byte range.
uint64_t HashBytes(const void* data, size_t byte_count);

/// Returns the canonical cache file path:
///   "<cache_dir>/<subdir>/<hex_hash>_jolt<MAJOR>.<MINOR>.bin"
/// @param subdir  "terrain" or "meshes"
std::string ShapeCachePath(const std::string& cache_dir,
                            const std::string& subdir,
                            uint64_t           content_hash);

/// Tries to deserialise a shape from a binary cache file.
/// @returns nullptr on cache miss or any read/parse error.
JPH::ShapeRefC TryLoadShape(const std::string& path);

/// Serialises a built Jolt shape to a binary cache file.
/// If cleanup_siblings is true, all other "*.bin" files in the same directory
/// as path are deleted first (used for terrain: one file per terrain).
void SaveShape(const JPH::ShapeRefC& shape,
               const std::string&    path,
               bool                  cleanup_siblings);

}  // namespace physics
