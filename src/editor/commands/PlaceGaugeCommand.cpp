#include "editor/commands/PlaceGaugeCommand.h"

namespace editor {

PlaceGaugeCommand::PlaceGaugeCommand(EditorScene* scene, EditorGauge gauge)
    : scene_(scene), gauge_(gauge) {}

void PlaceGaugeCommand::Execute() {
  scene_->GetGauges().push_back(gauge_);
}

void PlaceGaugeCommand::Undo() {
  if (!scene_->GetGauges().empty())
    scene_->GetGauges().pop_back();
}

std::string_view PlaceGaugeCommand::GetDescription() const {
  return "Place Gauge";
}

}  // namespace editor
