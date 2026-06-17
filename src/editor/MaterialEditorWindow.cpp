#include "editor/MaterialEditorWindow.h"

#include <filesystem>
#include <fstream>

#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <loguru.hpp>
#include <nfd.h>
#include <yaml-cpp/yaml.h>

#include "abstract/Texture.h"
#include "core/Color.h"
#include "core/Config.h"
#include "editor/EditorCommandHistory.h"
#include "editor/EditorScene.h"
#include "editor/commands/MaterialAssignCommand.h"
#include "editor/commands/MaterialPropertyCommand.h"
#include "game/GameMaterial.h"
#include "game/GameMesh.h"
#include "game/GameObjectType.h"
#include "game/MeshTemplate.h"
#include "renderer/GeometryUtils.h"
#include "renderer/Material.h"
#include "renderer/MaterialDesc.h"
#include "renderer/TextureSlot.h"

namespace editor {

namespace {

constexpr int   kPreviewSize    = 300;
constexpr float kLabelColWidth  = 74.f;
constexpr float kClearBtnWidth  = 22.f;

// Parallel arrays indexed by TextureSlot integer value.
constexpr const char* kSlotNames[renderer::kTextureSlotCount] = {
    "diffuse", "normal", "specular", "emissive", "environment",
};
constexpr const char* kSlotLabels[renderer::kTextureSlotCount] = {
    "Diffuse", "Normal", "Specular", "Emissive", "Environ.",
};
constexpr const char* kDefaultTextures[renderer::kTextureSlotCount] = {
    "default/diffuse.dds",
    "default/normal.dds",
    "default/specular.dds",
    "default/emissive.dds",
    "default/environment.dds",
};

bool IsDefaultTexture(const abstract::Texture* tex) {
  return !tex || tex->GetId().rfind("default/", 0) == 0;
}

}  // namespace

MaterialEditorWindow::MaterialEditorWindow(abstract::VideoDevice* video)
    : video_(video),
      preview_(video, kPreviewSize, kPreviewSize) {
  auto* def_mat = new game::GameMaterial(
      "__proc_preview_default",
      renderer::MaterialDesc().SetDiffuseColor(core::Color(0.7f, 0.7f, 0.7f)),
      video);
  cube_tmpl_ = new game::MeshTemplate(
      "__proc_preview_cube", renderer::CreateCubeMesh(video), def_mat);
  sphere_tmpl_ = new game::MeshTemplate(
      "__proc_preview_sphere",
      renderer::CreateSphere(video, /*stacks=*/32, /*rings=*/64), def_mat);
  def_mat->Release();  // cube_tmpl_ and sphere_tmpl_ each hold a ref

  preview_.SetTemplate(cube_tmpl_);
}

MaterialEditorWindow::~MaterialEditorWindow() {
  // Clear the preview instance before releasing templates so MeshInstance
  // is destroyed while the Mesh it references is still alive.
  preview_.SetTemplate(nullptr);
  if (cube_tmpl_)   cube_tmpl_->Release();
  if (sphere_tmpl_) sphere_tmpl_->Release();
  if (desc_material_owned_ && material_) material_->Release();
}

void MaterialEditorWindow::Open(game::GameMaterial* mat) {
  // Release any previously owned material from OpenWithDesc.
  if (desc_material_owned_ && material_) {
    material_->Release();
    desc_material_owned_ = false;
  }

  material_   = mat;
  open_       = (mat != nullptr);
  editing_    = false;
  on_saved_   = nullptr;
  hint_paths_ = {};
  if (!mat) return;

  cube_tmpl_->SetMaterial(mat);
  sphere_tmpl_->SetMaterial(mat);
  preview_.SetTemplate(
      preview_geo_ == PreviewGeometry::kCube ? cube_tmpl_ : sphere_tmpl_);
}

void MaterialEditorWindow::OpenExisting(
    game::GameMaterial* mat,
    std::function<void(const std::string&)> on_saved) {
  if (desc_material_owned_ && material_)
    material_->Release();

  material_            = mat;
  open_                = (mat != nullptr);
  editing_             = false;
  on_saved_            = on_saved;
  hint_paths_          = {};
  desc_material_owned_ = true;
  if (!mat) return;

  cube_tmpl_->SetMaterial(mat);
  sphere_tmpl_->SetMaterial(mat);
  preview_.SetTemplate(
      preview_geo_ == PreviewGeometry::kCube ? cube_tmpl_ : sphere_tmpl_);
}

void MaterialEditorWindow::OpenWithDesc(const ImportedMaterialDesc& desc) {
  // Release any previously owned material.
  if (desc_material_owned_ && material_) {
    material_->Release();
    desc_material_owned_ = false;
  }

  // Create a fresh GameMaterial keyed by slot name.
  // Use a unique procedural id to avoid registry collisions with file-backed
  // materials; the name is set from the slot name for the Save() path logic.
  game::GameMaterial* mat = new game::GameMaterial(
      desc.slot_name, renderer::MaterialDesc(), video_);

  // Pre-fill texture slots from the resolved paths.
  auto* rmat = mat->GetMaterial();
  for (int i = 0; i < renderer::kTextureSlotCount; ++i) {
    if (!desc.texture_paths[i].empty()) {
      abstract::Texture* tex = video_->CreateTexture(desc.texture_paths[i]);
      if (tex) {
        rmat->SetTexture(static_cast<renderer::TextureSlot>(i), tex);
        tex->Release();
      }
    }
  }

  // Update preview templates to use this material.
  cube_tmpl_->SetMaterial(mat);
  sphere_tmpl_->SetMaterial(mat);
  preview_.SetTemplate(
      preview_geo_ == PreviewGeometry::kCube ? cube_tmpl_ : sphere_tmpl_);

  material_            = mat;
  open_                = true;
  editing_             = false;
  on_saved_            = desc.on_saved;
  hint_paths_          = desc.hint_paths;
  desc_material_owned_ = true;  // we own this ref; Release on close or next Open
}

void MaterialEditorWindow::Render(const EditorScene& scene) {
  // Release owned material when the window is closed.
  if (!open_ && desc_material_owned_ && material_) {
    material_->Release();
    material_            = nullptr;
    desc_material_owned_ = false;
    on_saved_            = nullptr;
  }
  if (!open_ || !material_) return;

  const std::string title = "Material: " + material_->GetId();
  ImGui::SetNextWindowSize(ImVec2(720.f, 520.f), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin(title.c_str(), &open_)) {
    ImGui::End();
    return;
  }

  const float bot_h =
      ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y * 2.f;

  if (ImGui::BeginChild("##mat_content", ImVec2(0.f, -bot_h))) {
    constexpr ImGuiTableFlags kTableFlags = ImGuiTableFlags_None;
    if (ImGui::BeginTable("##mat_cols", 2, kTableFlags)) {
      ImGui::TableSetupColumn("##left",
                               ImGuiTableColumnFlags_WidthFixed,
                               static_cast<float>(kPreviewSize) + 8.f);
      ImGui::TableSetupColumn("##right",
                               ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      RenderPreviewColumn();
      ImGui::TableNextColumn();
      RenderPropertiesColumn();
      ImGui::EndTable();
    }
  }
  ImGui::EndChild();

  ImGui::Separator();
  RenderBottomBar(scene);

  ImGui::End();
}

void MaterialEditorWindow::RenderPreviewColumn() {
  preview_.Render(static_cast<float>(ImGui::GetTime()));

  const float btn_w =
      (static_cast<float>(kPreviewSize) - ImGui::GetStyle().ItemSpacing.x) *
      0.5f;

  const bool cube_on = (preview_geo_ == PreviewGeometry::kCube);
  if (cube_on)
    ImGui::PushStyleColor(ImGuiCol_Button,
                           ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
  if (ImGui::Button(ICON_FA_CUBE " Cube", ImVec2(btn_w, 0.f))) {
    preview_geo_ = PreviewGeometry::kCube;
    preview_.SetTemplate(cube_tmpl_);
  }
  if (cube_on) ImGui::PopStyleColor();

  ImGui::SameLine();

  const bool sphere_on = (preview_geo_ == PreviewGeometry::kSphere);
  if (sphere_on)
    ImGui::PushStyleColor(ImGuiCol_Button,
                           ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
  if (ImGui::Button(ICON_FA_CIRCLE " Sphere", ImVec2(btn_w, 0.f))) {
    preview_geo_ = PreviewGeometry::kSphere;
    preview_.SetTemplate(sphere_tmpl_);
  }
  if (sphere_on) ImGui::PopStyleColor();
}

void MaterialEditorWindow::RenderPropertiesColumn() {
  if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
    RenderRenderingSection();
  }
  if (ImGui::CollapsingHeader("Sound")) {
    ImGui::TextDisabled("(no sound properties yet)");
  }
  if (ImGui::CollapsingHeader("Physics")) {
    ImGui::TextDisabled("(no physics properties yet)");
  }
}

void MaterialEditorWindow::RenderRenderingSection() {
  auto* mat = material_->GetMaterial();

  // ColorEdit4 uses BeginGroup/EndGroup internally. EndGroup() only propagates
  // IsItemDeactivatedAfterEdit() when the deactivated sub-item is re-submitted
  // that same frame, which never happens when the color picker popup closes.
  // Instead, track editing state via IsItemEdited():
  //   - Capture before_snapshot_ on the first frame any widget reports edited.
  //   - Push a command the first frame that no widget is edited (editing stopped).
  // frame_start is captured before any SetDiffuseColor/SetShininess calls this
  // frame, so it correctly represents the state before the first interaction.
  const MaterialSnapshot frame_start =
      history_ ? CaptureSnapshot(material_) : MaterialSnapshot{};
  bool any_edited = false;

  auto track = [&]() {
    if (history_ && ImGui::IsItemEdited())
      any_edited = true;
  };

  ImGui::SeparatorText("Textures");
  constexpr ImGuiTableFlags kTexFlags =
      ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;
  if (ImGui::BeginTable("##tex_slots", 3, kTexFlags)) {
    ImGui::TableSetupColumn("##tex_label", ImGuiTableColumnFlags_WidthFixed,
                             kLabelColWidth);
    ImGui::TableSetupColumn("##tex_btn", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("##tex_clear", ImGuiTableColumnFlags_WidthFixed,
                             kClearBtnWidth);
    for (int i = 0; i < renderer::kTextureSlotCount; ++i) {
      RenderTextureSlot(static_cast<renderer::TextureSlot>(i), kSlotLabels[i]);
    }
    ImGui::EndTable();
  }

  ImGui::SeparatorText("Colors");

  {
    core::Color dc = mat->GetDiffuseColor();
    float c[4] = {dc.r, dc.g, dc.b, dc.a};
    if (ImGui::ColorEdit4("Diffuse color", c))
      mat->SetDiffuseColor({c[0], c[1], c[2], c[3]});
    track();
  }
  {
    core::Color ec = mat->GetEmissiveColor();
    float c[4] = {ec.r, ec.g, ec.b, ec.a};
    if (ImGui::ColorEdit4("Emissive color", c))
      mat->SetEmissiveColor({c[0], c[1], c[2], c[3]});
    track();
  }
  {
    core::Color ac = mat->GetAmbientColor();
    float c[4] = {ac.r, ac.g, ac.b, ac.a};
    if (ImGui::ColorEdit4("Ambient color", c))
      mat->SetAmbientColor({c[0], c[1], c[2], c[3]});
    track();
  }

  float shine = mat->GetShininess();
  if (ImGui::SliderFloat("Shininess", &shine, 1.f, 256.f))
    mat->SetShininess(shine);
  track();

  float specular = mat->GetSpecular();
  if (ImGui::SliderFloat("Specular", &specular, 0.f, 1.f))
    mat->SetSpecular(specular);
  track();

  ImGui::SeparatorText("Flags");

  // Alpha mask is an instant toggle — push a command immediately on change,
  // same pattern as texture slot changes (not the drag state machine above).
  bool alpha_mask = mat->GetAlphaMask();
  if (ImGui::Checkbox("Alpha mask", &alpha_mask)) {
    if (history_) before_snapshot_ = CaptureSnapshot(material_);
    mat->SetAlphaMask(alpha_mask);
    if (history_) {
      MaterialSnapshot after = CaptureSnapshot(material_);
      if (after != before_snapshot_)
        history_->Push(std::make_unique<MaterialPropertyCommand>(
            material_, video_, before_snapshot_, after));
    }
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Discard pixels with diffuse alpha < 0.5 (cutout transparency)");

  // State machine: detect edit-start and edit-end across frames.
  if (history_) {
    if (any_edited && !editing_) {
      before_snapshot_ = frame_start;
      editing_         = true;
    }
    if (!any_edited && editing_) {
      MaterialSnapshot after = CaptureSnapshot(material_);
      if (after != before_snapshot_)
        history_->Push(std::make_unique<MaterialPropertyCommand>(
            material_, video_, before_snapshot_, after));
      editing_ = false;
    }
  }
}

void MaterialEditorWindow::RenderTextureSlot(renderer::TextureSlot slot,
                                             const char* label) {
  auto* mat              = material_->GetMaterial();
  const auto* tex        = mat->GetTexture(slot);
  const bool def         = IsDefaultTexture(tex);
  const int  slot_idx    = static_cast<int>(slot);
  const std::string& hint = hint_paths_[slot_idx];
  const char* disp = def ? (hint.empty() ? "None" : hint.c_str())
                         : tex->GetId().c_str();

  ImGui::TableNextRow();

  ImGui::TableNextColumn();
  ImGui::TextUnformatted(label);

  ImGui::TableNextColumn();
  ImGui::PushID(slot_idx);
  const float btn_w = ImGui::GetContentRegionAvail().x;
  const bool is_hint = def && !hint.empty();
  if (is_hint)
    ImGui::PushStyleColor(ImGuiCol_Text,
                          ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
  if (ImGui::Button(disp, ImVec2(btn_w, 0.f))) {
    const auto tex_dir = core::Config::GetDataFolder() / "textures";
    nfdu8char_t* path  = nullptr;
    const nfdu8filteritem_t filters[] = {{"Image files", "png,jpg,jpeg,tga,dds"}};
    const nfdresult_t res =
        NFD_OpenDialogU8(&path, filters, 1, tex_dir.c_str());
    if (res == NFD_OKAY) {
      const auto rel = std::filesystem::relative(
          std::filesystem::path(path), tex_dir);
      abstract::Texture* new_tex =
          video_->CreateTexture(rel.generic_string());
      if (new_tex) {
        if (history_) before_snapshot_ = CaptureSnapshot(material_);
        mat->SetTexture(slot, new_tex);
        new_tex->Release();
        if (history_) {
          MaterialSnapshot after = CaptureSnapshot(material_);
          if (after != before_snapshot_)
            history_->Push(std::make_unique<MaterialPropertyCommand>(
                material_, video_, before_snapshot_, after));
        }
      }
      NFD_FreePathU8(path);
    } else if (res == NFD_ERROR) {
      LOG_F(ERROR, "NFD error opening texture dialog");
    }
  }
  if (is_hint) ImGui::PopStyleColor();

  ImGui::TableNextColumn();
  if (ImGui::Button("x", ImVec2(kClearBtnWidth, 0.f))) {
    const int idx = static_cast<int>(slot);
    abstract::Texture* def_tex =
        video_->CreateTexture(kDefaultTextures[idx]);
    if (def_tex) {
      if (history_) before_snapshot_ = CaptureSnapshot(material_);
      mat->SetTexture(slot, def_tex);
      def_tex->Release();
      if (history_) {
        MaterialSnapshot after = CaptureSnapshot(material_);
        if (after != before_snapshot_)
          history_->Push(std::make_unique<MaterialPropertyCommand>(
              material_, video_, before_snapshot_, after));
      }
    }
  }
  ImGui::PopID();
}

void MaterialEditorWindow::RenderBottomBar(const EditorScene& scene) {
  if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save")) {
    Save();
  }
  ImGui::SameLine();

  const game::GameObject* sel = scene.GetSelectedObject();
  const bool can_apply = (sel != nullptr &&
                           sel->GetType() == game::GameObjectType::kMesh);
  if (!can_apply) ImGui::BeginDisabled();
  if (ImGui::Button(ICON_FA_FILL_DRIP " Apply to selection")) {
    ApplyToSelection(scene);
  }
  if (!can_apply) ImGui::EndDisabled();
}

void MaterialEditorWindow::Save() {
  if (!material_) return;
  const auto* mat = material_->GetMaterial();

  YAML::Emitter out;
  out << YAML::BeginMap;
  out << YAML::Key << "rendering" << YAML::Value << YAML::BeginMap;

  // Emit only non-default texture slots.
  bool any_tex = false;
  for (int i = 0; i < renderer::kTextureSlotCount; ++i) {
    const auto* tex =
        mat->GetTexture(static_cast<renderer::TextureSlot>(i));
    if (!IsDefaultTexture(tex)) {
      if (!any_tex) {
        out << YAML::Key << "textures" << YAML::Value << YAML::BeginMap;
        any_tex = true;
      }
      out << YAML::Key << kSlotNames[i] << YAML::Value << tex->GetId();
    }
  }
  if (any_tex) out << YAML::EndMap;

  auto write_color = [&](const char* key, core::Color c) {
    out << YAML::Key << key << YAML::Value
        << YAML::Flow << YAML::BeginSeq
        << c.r << c.g << c.b << c.a
        << YAML::EndSeq;
  };
  write_color("diffuse_color",  mat->GetDiffuseColor());
  write_color("emissive_color", mat->GetEmissiveColor());
  write_color("ambient_color",  mat->GetAmbientColor());
  out << YAML::Key << "shininess" << YAML::Value << mat->GetShininess();
  out << YAML::Key << "specular"  << YAML::Value << mat->GetSpecular();
  if (mat->GetAlphaMask())
    out << YAML::Key << "alpha_mask" << YAML::Value << true;

  out << YAML::EndMap;  // rendering
  out << YAML::EndMap;  // root

  const auto path = core::Config::GetDataFolder() / "materials" /
                    (material_->GetId() + ".yaml");
  std::ofstream file(path);
  if (file) {
    file << out.c_str();
    LOG_F(INFO, "Saved material '%s'", material_->GetId().c_str());
    if (on_saved_) {
      on_saved_(material_->GetId());
      on_saved_ = nullptr;
    }
  } else {
    LOG_F(ERROR, "Failed to save material '%s' to '%s'",
          material_->GetId().c_str(), path.c_str());
  }
}

void MaterialEditorWindow::ApplyToSelection(const EditorScene& scene) {
  game::GameObject* sel = scene.GetSelectedObject();
  if (!sel || sel->GetType() != game::GameObjectType::kMesh) return;
  auto* mesh = static_cast<game::GameMesh*>(sel);
  game::GameMaterial* before = mesh->GetTemplate()->GetMaterial();
  if (history_) {
    history_->Push(std::make_unique<MaterialAssignCommand>(
        mesh, before, material_));
  } else {
    mesh->GetTemplate()->SetMaterial(material_);
  }
  LOG_F(INFO, "Applied material '%s' to '%s'",
        material_->GetId().c_str(), sel->GetName().c_str());
}

}  // namespace editor
