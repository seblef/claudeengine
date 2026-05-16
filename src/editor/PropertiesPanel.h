#pragma once

namespace game {
class GameObject;
class GameLight;
class GameMesh;
}  // namespace game

namespace editor {

// Right panel "Properties": editable fields for the currently selected object.
//
// Render() must be called inside an open ImGui::Begin("Properties") window.
// Changes are applied immediately to the underlying renderer objects with no
// Apply button.
class PropertiesPanel {
 public:
  PropertiesPanel() = default;

  // Renders the properties UI inside the current ImGui window.
  // obj may be nullptr (no selection).
  void Render(game::GameObject* obj);

 private:
  void RenderLightProperties(const game::GameLight* light);
  void RenderMeshProperties(const game::GameMesh* mesh);
};

}  // namespace editor
