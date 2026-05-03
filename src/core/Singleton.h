#pragma once

namespace core {

// CRTP singleton base. Derive from Singleton<T>, declare the constructor
// private, and add `friend class Singleton<T>` so Instance() can construct it.
//
//   class MySystem : public Singleton<MySystem> {
//     friend class Singleton<MySystem>;
//     MySystem() = default;
//    public:
//     void DoWork();
//   };
//
//   MySystem::Instance().DoWork();
//
// Instance() initialises T on first call and is thread-safe since C++11.
template <typename T>
class Singleton {
 public:
  [[nodiscard]] static T& Instance() {
    static T instance;
    return instance;
  }

  Singleton(const Singleton&) = delete;
  Singleton& operator=(const Singleton&) = delete;

 protected:
  Singleton() = default;
};

}  // namespace core
