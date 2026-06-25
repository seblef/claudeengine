#pragma once

#include <string_view>

#include "editor/EditorCommand.h"
#include "editor/EditorScene.h"

namespace editor {

// Command that appends a single EditorGauge to the scene, with undo/redo support.
//
// Commands are undone in LIFO order, so Execute() always pushes to the back and
// Undo() always pops from the back — no index bookkeeping is needed.
class PlaceGaugeCommand : public EditorCommand {
 public:
  PlaceGaugeCommand(EditorScene* scene, EditorGauge gauge);

  void Execute() override;
  void Undo()    override;
  std::string_view GetDescription() const override;

 private:
  // cppcheck-suppress unusedStructMember
  EditorScene* scene_;
  // cppcheck-suppress unusedStructMember
  EditorGauge  gauge_;
};

}  // namespace editor
