#pragma once

#include <map>

namespace core {

// Generic reference-counted resource base.
//
// Derive concrete resource types (shaders, textures, materials, …) from
// Resource<TId>, where TId is any type satisfying std::map key requirements
// (operator< must be defined).
//
// Lifecycle:
//   - The constructor registers the instance in a per-TId static map.
//     The initial reference count is 1 (the creator owns a reference).
//   - AddRef() increments the count when an additional owner takes the pointer.
//   - Release() decrements the count. When it reaches 0 the resource removes
//     itself from the map and destroys itself (delete this).
//
// Initialization:
//   Derived constructors set initialized_ = true once platform-specific work
//   (GPU upload, file parse, etc.) succeeds. Use IsInitialized() to check.
template <typename TId>
class Resource {
 public:
  Resource(const Resource&) = delete;
  Resource& operator=(const Resource&) = delete;

  // ---- Reference counting --------------------------------------------------

  // Increments the reference count.
  inline void AddRef() { ++ref_count_; }

  // Decrements the reference count. Removes from the registry and deletes
  // this object when the count reaches zero.
  inline void Release() {
    if (--ref_count_ == 0) {
      resources_.erase(id_);
      delete this;
    }
  }

  // ---- Registry lookup -----------------------------------------------------

  // Returns the resource registered under id, or nullptr if absent.
  [[nodiscard]] static Resource<TId>* Get(const TId& id) {
    auto it = resources_.find(id);
    return (it != resources_.end()) ? it->second : nullptr;
  }

  // ---- Accessors -----------------------------------------------------------

  // Returns this resource's ID.
  [[nodiscard]] const TId& GetId() const { return id_; }

  // Returns true if the resource completed its initialisation successfully.
  [[nodiscard]] bool IsInitialized() const { return initialized_; }

 protected:
  // Registers the resource in the static map with ref_count = 1.
  explicit Resource(const TId& id) : id_(id) { resources_[id] = this; }

  virtual ~Resource() = default;

  // Set to true by derived constructors on successful initialisation.
  bool initialized_ = false;

 private:
  TId id_;
  int ref_count_ = 1;

  static std::map<TId, Resource<TId>*> resources_;
};

// Out-of-class static member definition (one per TId instantiation).
template <typename TId>
std::map<TId, Resource<TId>*> Resource<TId>::resources_;

}  // namespace core
