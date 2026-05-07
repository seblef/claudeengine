#pragma once

#include <cassert>

namespace core {

// CRTP singleton base with new/delete lifecycle.
//
// Derive from Singleton<T>; the instance registers itself on construction
// and clears on destruction. Create with new, destroy with Shutdown().
//
//   class MySystem : public Singleton<MySystem> {
//    public:
//     explicit MySystem(int n);
//     void DoWork();
//   };
//
//   new MySystem(42);               // registers instance
//   MySystem::Instance().DoWork();
//   MySystem::Shutdown();           // deletes instance
//
// Create singletons in dependency order; Shutdown in reverse order.
template <typename T>
class Singleton {
 public:
  Singleton() {
    assert(!instance_ && "Singleton already instanced");
    instance_ = static_cast<T*>(this);
  }

  ~Singleton() {
    assert(instance_);
    instance_ = nullptr;
  }

  [[nodiscard]] static T& Instance() {
    assert(instance_ && "Singleton not instanced");
    return *instance_;
  }

  // Deletes the instance. The destructor clears the internal pointer.
  static void Shutdown() { delete instance_; }

  [[nodiscard]] static bool IsInstanced() { return instance_ != nullptr; }

  Singleton(const Singleton&)            = delete;
  Singleton& operator=(const Singleton&) = delete;

 private:
  static T* instance_;
};

template <typename T>
T* Singleton<T>::instance_ = nullptr;

}  // namespace core
