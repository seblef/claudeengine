#pragma once

#include <queue>

#include "core/Event.h"
#include "core/Singleton.h"

namespace core {

// Singleton FIFO event queue.
//
// Platform drivers call Publish() to enqueue events; game systems call
// HasEvents() / Consume() to drain them one at a time in arrival order.
//
// Usage:
//   EventManager::Instance().Publish(Event::MouseMoved(x, y));
//
//   while (EventManager::Instance().HasEvents()) {
//     Event e = EventManager::Instance().Consume();
//     // handle e
//   }
class EventManager : public Singleton<EventManager> {
  friend class Singleton<EventManager>;

 public:
  // Appends event to the back of the queue.
  inline void Publish(const Event& event) { events_.push(event); }

  // Returns true if at least one event is waiting to be consumed.
  [[nodiscard]] inline bool HasEvents() const { return !events_.empty(); }

  // Removes and returns the oldest event.
  // Precondition: HasEvents() must be true.
  inline Event Consume() {
    Event e = events_.front();
    events_.pop();
    return e;
  }

 private:
  EventManager() = default;
  std::queue<Event> events_;
};

}  // namespace core
