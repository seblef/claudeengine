#include "editor/commands/MaterialPropertyCommand.h"

#include <loguru.hpp>

#include "abstract/Texture.h"
#include "abstract/VideoDevice.h"
#include "game/GameMaterial.h"
#include "renderer/Material.h"
#include "renderer/TextureSlot.h"

namespace editor {

bool MaterialSnapshot::operator==(const MaterialSnapshot& o) const {
  return diffuse_color  == o.diffuse_color
      && emissive_color == o.emissive_color
      && ambient_color  == o.ambient_color
      && shininess      == o.shininess
      && specular       == o.specular
      && alpha_mask     == o.alpha_mask
      && texture_slots  == o.texture_slots;
}

MaterialSnapshot CaptureSnapshot(const game::GameMaterial* game_mat) {
  const renderer::Material* mat = game_mat->GetMaterial();
  MaterialSnapshot s;
  s.diffuse_color  = mat->GetDiffuseColor();
  s.emissive_color = mat->GetEmissiveColor();
  s.ambient_color  = mat->GetAmbientColor();
  s.shininess      = mat->GetShininess();
  s.specular       = mat->GetSpecular();
  s.alpha_mask     = mat->GetAlphaMask();
  s.texture_slots.resize(renderer::kTextureSlotCount);
  for (int i = 0; i < renderer::kTextureSlotCount; ++i) {
    const auto* tex = mat->GetTexture(static_cast<renderer::TextureSlot>(i));
    s.texture_slots[i] = tex ? tex->GetId() : "";
  }
  return s;
}

// cppcheck-suppress constParameterPointer
void ApplySnapshot(game::GameMaterial* game_mat, const MaterialSnapshot& s,
                   abstract::VideoDevice* video) {
  renderer::Material* mat = game_mat->GetMaterial();
  mat->SetDiffuseColor(s.diffuse_color);
  mat->SetEmissiveColor(s.emissive_color);
  mat->SetAmbientColor(s.ambient_color);
  mat->SetShininess(s.shininess);
  mat->SetSpecular(s.specular);
  mat->SetAlphaMask(s.alpha_mask);
  for (int i = 0; i < renderer::kTextureSlotCount; ++i) {
    const std::string& id = s.texture_slots[i];
    if (id.empty()) continue;
    abstract::Texture* tex = video->CreateTexture(id);
    if (tex) {
      mat->SetTexture(static_cast<renderer::TextureSlot>(i), tex);
      tex->Release();
    }
  }
}

MaterialPropertyCommand::MaterialPropertyCommand(game::GameMaterial*    mat,
                                                 abstract::VideoDevice* video,
                                                 MaterialSnapshot       before,
                                                 MaterialSnapshot       after)
    : mat_(mat),
      video_(video),
      before_(std::move(before)),
      after_(std::move(after)) {}

void MaterialPropertyCommand::Execute() {
  ApplySnapshot(mat_, after_, video_);
}

void MaterialPropertyCommand::Undo() {
  ApplySnapshot(mat_, before_, video_);
}

void MaterialPropertyCommand::Redo() {
  ApplySnapshot(mat_, after_, video_);
}

std::string_view MaterialPropertyCommand::GetDescription() const {
  return "Edit Material Property";
}

}  // namespace editor
