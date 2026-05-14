#include "editor/EditorSystem.h"

#include "core/EventManager.h"
#include "core/EventType.h"

namespace editor {

EditorSystem::EditorSystem(abstract::Devices* devices) : devices_(devices) {}

void EditorSystem::Run() {
  while (running_) {
    devices_->Update();

    while (core::EventManager::Instance().HasEvents()) {
      core::Event e = core::EventManager::Instance().Consume();
      if (e.type == core::EventType::kWindowClose) {
        running_ = false;
      }
    }
  }
}

void EditorSystem::Stop() {
  running_ = false;
}

}  // namespace editor
