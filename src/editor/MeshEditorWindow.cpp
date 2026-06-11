#include "editor/MeshEditorWindow.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <loguru.hpp>

#include "abstract/VideoDevice.h"
#include "core/BBox3.h"
#include "core/Config.h"
#include "core/Mat3f.h"
#include "core/MathUtils.h"
#include "core/Vertex3D.h"
#include "editor/EditorScene.h"
#include "editor/MaterialEditorWindow.h"
#include "game/GameMaterial.h"
#include "game/MeshTemplate.h"
#include "mesh/EmeshReader.h"
#include "mesh/EmeshWriter.h"
#include "mesh/FbxImporter.h"
#include "mesh/LodData.h"
#include "mesh/ObjImporter.h"
#include "mesh/SubMeshRange.h"
#include "renderer/GeometryData.h"
#include "renderer/MaterialDesc.h"
#include "renderer/TextureSlot.h"

namespace editor {

namespace {

// Returns the lowercase extension of a path (e.g. ".fbx").
std::string Extension(const std::filesystem::path& p) {
  std::string ext = p.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return ext;
}

// Creates a GeometryData from LodData.
std::unique_ptr<renderer::GeometryData> MakeGeometry(
    abstract::VideoDevice* video, const mesh::LodData& lod) {
  return std::make_unique<renderer::GeometryData>(
      video,
      static_cast<int>(lod.vertices.size()), lod.vertices.data(),
      static_cast<int>(lod.indices.size()) / 3, lod.indices.data(),
      lod.submeshes);
}

}  // namespace

MeshEditorWindow::MeshEditorWindow(abstract::VideoDevice* video,
                                   MaterialEditorWindow* material_editor)
    : video_(video),
      material_editor_(material_editor),
      preview_(video, 360, 360) {}

MeshEditorWindow::~MeshEditorWindow() {
  ClearPreview();
}

void MeshEditorWindow::ClearPreview() {
  preview_.SetTemplate(nullptr);
  if (preview_tmpl_) {
    preview_tmpl_->Release();
    preview_tmpl_ = nullptr;
  }
  for (auto* mat : preview_mats_) mat->Release();
  preview_mats_.clear();
}

void MeshEditorWindow::OpenImport(const std::filesystem::path& source_path) {
  source_path_ = source_path;
  mode_        = Mode::kImport;
  scale_       = 1.0f;
  rotation_    = core::Mat3f::kIdentity;
  original_mesh_ = mesh::MeshData{};
  mat_slots_.clear();

  // Default mesh name from filename stem.
  const std::string stem = source_path.stem().string();
  std::strncpy(mesh_name_buf_, stem.c_str(), sizeof(mesh_name_buf_) - 1);
  mesh_name_buf_[sizeof(mesh_name_buf_) - 1] = '\0';

  if (!LoadSourceFile()) {
    LOG_F(ERROR, "MeshEditorWindow: failed to import '%s'",
          source_path.string().c_str());
    return;
  }

  // Pre-fill scale from FBX unit metadata.
  if (original_mesh_.unit_meters != 1.0f && original_mesh_.unit_meters > 0.f)
    scale_ = original_mesh_.unit_meters;

  ResolveAllTextures();
  preview_dirty_ = true;
  open_          = true;
}

void MeshEditorWindow::OpenEdit(const std::filesystem::path& emesh_path) {
  source_path_ = emesh_path;
  mode_        = Mode::kEdit;
  scale_       = 1.0f;
  rotation_    = core::Mat3f::kIdentity;
  original_mesh_ = mesh::MeshData{};
  mat_slots_.clear();

  const std::string stem = emesh_path.stem().string();
  std::strncpy(mesh_name_buf_, stem.c_str(), sizeof(mesh_name_buf_) - 1);
  mesh_name_buf_[sizeof(mesh_name_buf_) - 1] = '\0';

  if (!LoadEmeshFile()) {
    LOG_F(ERROR, "MeshEditorWindow: failed to read '%s'",
          emesh_path.string().c_str());
    return;
  }

  ResolveAllTextures();
  preview_dirty_ = true;
  open_          = true;
}

bool MeshEditorWindow::LoadSourceFile() {
  const std::string ext = Extension(source_path_);
  bool ok = false;
  if (ext == ".fbx") {
    ok = mesh::FbxImporter{}.Import(source_path_.string(), &original_mesh_);
  } else if (ext == ".obj") {
    ok = mesh::ObjImporter{}.Import(source_path_.string(), &original_mesh_);
  }
  if (!ok) return false;

  // Build material slot list from submesh ranges.
  for (int i = 0; i < static_cast<int>(original_mesh_.lod.submeshes.size()); ++i) {
    const auto& sub = original_mesh_.lod.submeshes[i];
    MatSlot slot;
    slot.original_name = sub.material_name;
    slot.saved_material_path = sub.material_name.empty()
        ? "" : ("materials/" + sub.material_name + ".yaml");
    const std::string init_name = sub.material_name.empty()
        ? ("slot_" + std::to_string(i)) : sub.material_name;
    std::strncpy(slot.name_buf, init_name.c_str(), sizeof(slot.name_buf) - 1);
    mat_slots_.push_back(std::move(slot));
  }
  return true;
}

bool MeshEditorWindow::LoadEmeshFile() {
  bool ok = mesh::EmeshReader{}.Read(source_path_.string(), &original_mesh_);
  if (!ok) return false;

  for (int i = 0; i < static_cast<int>(original_mesh_.lod.submeshes.size()); ++i) {
    const auto& sub = original_mesh_.lod.submeshes[i];
    MatSlot slot;
    slot.original_name       = sub.material_name;
    slot.saved_material_path = sub.material_name.empty()
        ? "" : ("materials/" + sub.material_name + ".yaml");
    const std::string init_name = sub.material_name.empty()
        ? ("slot_" + std::to_string(i)) : sub.material_name;
    std::strncpy(slot.name_buf, init_name.c_str(), sizeof(slot.name_buf) - 1);
    mat_slots_.push_back(std::move(slot));
  }
  return true;
}

// cppcheck-suppress functionStatic
std::string MeshEditorWindow::ResolveTexture(const std::string& name) const {
  if (name.empty()) return {};
  const std::filesystem::path stem =
      std::filesystem::path(name).stem();
  const auto tex_root = core::Config::GetDataFolder() / "textures";
  std::error_code ec;
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(tex_root, ec)) {
    if (!entry.is_regular_file()) continue;
    if (entry.path().stem() == stem) {
      // Return relative to data/
      const auto rel = std::filesystem::relative(entry.path(),
                                                 core::Config::GetDataFolder(), ec);
      if (!ec) return rel.generic_string();
    }
  }
  return {};
}

void MeshEditorWindow::ResolveAllTextures() {
  for (auto& slot : mat_slots_) {
    // Try to resolve a diffuse texture by material name.
    const std::string resolved = ResolveTexture(slot.original_name);
    if (!resolved.empty()) {
      slot.hint_textures[static_cast<int>(renderer::TextureSlot::kDiffuse)] =
          resolved;
    }
  }
}

mesh::MeshData MeshEditorWindow::BuildTransformedMesh() const {
  mesh::MeshData result = original_mesh_;
  const float s = scale_;

  for (auto& v : result.lod.vertices) {
    // Scale + rotate position, normal, binormal, tangent.
    v.position = (v.position * rotation_) * s;
    v.normal   = (v.normal   * rotation_);
    v.binormal = (v.binormal * rotation_);
    v.tangent  = (v.tangent  * rotation_);
  }

  // Update submesh material paths from saved slots.
  for (int i = 0; i < static_cast<int>(result.lod.submeshes.size()); ++i) {
    if (i < static_cast<int>(mat_slots_.size())) {
      result.lod.submeshes[i].material_name =
          std::filesystem::path(mat_slots_[i].saved_material_path).stem().string();
    }
  }

  // Recompute AABB.
  if (!result.lod.vertices.empty()) {
    result.lod.aabb = core::BBox3{result.lod.vertices[0].position,
                                   result.lod.vertices[0].position};
    for (const auto& v : result.lod.vertices) result.lod.aabb << v.position;
  }

  return result;
}

void MeshEditorWindow::RebuildPreview() {
  ClearPreview();

  if (original_mesh_.lod.vertices.empty()) return;

  mesh::MeshData transformed = BuildTransformedMesh();

  // Create per-slot procedural GameMaterials for the preview.
  const int slot_count = static_cast<int>(transformed.lod.submeshes.size());
  const int mat_count  = slot_count > 0 ? slot_count : 1;
  preview_mats_.reserve(mat_count);

  if (slot_count > 0) {
    for (int i = 0; i < slot_count; ++i) {
      const auto& slot = (i < static_cast<int>(mat_slots_.size()))
          ? mat_slots_[i] : MatSlot{};
      // Use saved material if available, otherwise create a default preview mat.
      game::GameMaterial* mat = nullptr;
      if (!slot.saved_material_path.empty()) {
        // Extract material name (stem of path without extension pairs).
        const std::string mat_name =
            std::filesystem::path(slot.saved_material_path).stem().string();
        mat = game::GameMaterial::GetOrLoad(mat_name, video_);
      }
      if (!mat) {
        const std::string id = "__proc_mesh_preview_slot_" + std::to_string(i);
        mat = new game::GameMaterial(id, renderer::MaterialDesc(), video_);
      }
      preview_mats_.push_back(mat);
    }
  } else {
    auto* mat = new game::GameMaterial("__proc_mesh_preview_default",
                                       renderer::MaterialDesc(), video_);
    preview_mats_.push_back(mat);
  }

  // Build GeometryData.
  auto geo = MakeGeometry(video_, transformed.lod);

  // Use the first material for the procedural template; SetMaterial per slot.
  preview_tmpl_ = new game::MeshTemplate(
      "__proc_mesh_editor_preview", std::move(geo), preview_mats_[0]);

  // Assign remaining material slots.
  for (int i = 1; i < static_cast<int>(preview_mats_.size()); ++i)
    preview_tmpl_->SetMaterial(i, preview_mats_[i]);

  preview_.SetTemplate(preview_tmpl_);
  preview_dirty_ = false;
}

void MeshEditorWindow::Render(EditorScene* scene) {
  if (!open_) return;
  if (preview_dirty_) RebuildPreview();

  const std::string title = (mode_ == Mode::kImport)
      ? ("Import: " + source_path_.filename().string())
      : ("Edit: "   + source_path_.filename().string());

  ImGui::SetNextWindowSize(ImVec2(860.f, 560.f), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin(title.c_str(), &open_)) {
    ImGui::End();
    return;
  }

  // Mesh name row (import mode) / filename (edit mode).
  if (mode_ == Mode::kImport) {
    ImGui::SetNextItemWidth(260.f);
    ImGui::InputText("Mesh name", mesh_name_buf_, sizeof(mesh_name_buf_));
  } else {
    ImGui::TextUnformatted(("File: " + source_path_.string()).c_str());
  }

  ImGui::Separator();

  // Two-column layout: preview | right panel.
  constexpr float kPreviewW = 368.f;
  if (ImGui::BeginTable("##mesh_ed_cols", 2)) {
    ImGui::TableSetupColumn("##left",  ImGuiTableColumnFlags_WidthFixed, kPreviewW);
    ImGui::TableSetupColumn("##right", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableNextRow();

    // Left: preview.
    ImGui::TableNextColumn();
    preview_.Render(static_cast<float>(ImGui::GetTime()));

    // Right: materials + controls.
    ImGui::TableNextColumn();
    RenderMaterialSlots();
    ImGui::Spacing();
    RenderTransformControls();

    ImGui::EndTable();
  }

  ImGui::Separator();
  RenderBottomBar(scene);

  ImGui::End();
}

void MeshEditorWindow::RenderMaterialSlots() {
  ImGui::SeparatorText("Materials");

  if (mat_slots_.empty()) {
    ImGui::TextDisabled("(no submesh ranges)");
    return;
  }

  constexpr ImGuiTableFlags kFlags =
      ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV;
  if (!ImGui::BeginTable("##mat_slots", 2, kFlags)) return;

  ImGui::TableSetupColumn("Name",   ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 48.f);
  ImGui::TableHeadersRow();

  for (int i = 0; i < static_cast<int>(mat_slots_.size()); ++i) {
    auto& slot = mat_slots_[i];
    ImGui::PushID(i);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    // Inline-editable material name; updates the saved path immediately.
    ImGui::SetNextItemWidth(-1.f);
    if (ImGui::InputText("##name", slot.name_buf, sizeof(slot.name_buf))) {
      const std::string new_name(slot.name_buf);
      if (!new_name.empty()) {
        slot.saved_material_path = "materials/" + new_name + ".yaml";
        preview_dirty_ = true;
      }
    }

    ImGui::TableNextColumn();
    if (ImGui::SmallButton(ICON_FA_PEN " Edit")) {
      ImportedMaterialDesc desc;
      const std::string current_name(slot.name_buf);
      desc.slot_name     = current_name.empty()
          ? ("slot_" + std::to_string(i))
          : current_name;
      desc.texture_paths = slot.hint_textures;
      // For each unresolved slot, pass the original name as a dim hint.
      for (int ti = 0; ti < renderer::kTextureSlotCount; ++ti) {
        if (desc.texture_paths[ti].empty() && !slot.original_name.empty())
          desc.hint_paths[ti] = slot.original_name;
      }
      desc.on_saved = [this, i](const std::string& mat_name) {
        if (i < static_cast<int>(mat_slots_.size())) {
          mat_slots_[i].saved_material_path = "materials/" + mat_name + ".yaml";
          // Keep name_buf in sync with the actually saved material name.
          std::strncpy(mat_slots_[i].name_buf, mat_name.c_str(),
                       sizeof(mat_slots_[i].name_buf) - 1);
          preview_dirty_ = true;
        }
      };
      material_editor_->OpenWithDesc(desc);
    }
    ImGui::PopID();
  }

  ImGui::EndTable();
}

void MeshEditorWindow::RenderTransformControls() {
  ImGui::SeparatorText("Transform");

  const std::string unit_label = (original_mesh_.unit_meters != 1.0f &&
                                   original_mesh_.unit_meters > 0.f)
      ? ("Scale (" + std::to_string(original_mesh_.unit_meters) + " m/unit)")
      : "Scale";
  ImGui::SetNextItemWidth(120.f);
  if (ImGui::InputFloat(unit_label.c_str(), &scale_, 0.f, 0.f, "%.4f")) {
    if (scale_ <= 0.f) scale_ = 1e-4f;
    preview_dirty_ = true;
  }

  ImGui::Spacing();
  ImGui::TextUnformatted("Rotate");
  ImGui::SameLine();

  constexpr float kHalfPi = core::kPi * 0.5f;
  auto rot_btn = [&](const char* label, const core::Mat3f& delta) {
    if (ImGui::Button(label)) {
      rotation_     = delta * rotation_;
      preview_dirty_ = true;
    }
    ImGui::SameLine();
  };

  rot_btn("-X", core::Mat3f::RotationX(-kHalfPi));
  rot_btn("+X", core::Mat3f::RotationX( kHalfPi));
  rot_btn("-Y", core::Mat3f::RotationY(-kHalfPi));
  rot_btn("+Y", core::Mat3f::RotationY( kHalfPi));
  rot_btn("-Z", core::Mat3f::RotationZ(-kHalfPi));
  rot_btn("+Z", core::Mat3f::RotationZ( kHalfPi));
  ImGui::NewLine();
}

void MeshEditorWindow::RenderBottomBar(EditorScene* scene) {
  // Spacer to push buttons to the right.
  const float avail     = ImGui::GetContentRegionAvail().x;
  const float btn_w     = 120.f;
  const float spacing   = ImGui::GetStyle().ItemSpacing.x;
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - btn_w * 2.f - spacing);

  if (ImGui::Button("Cancel", ImVec2(btn_w, 0.f))) {
    open_ = false;
  }
  ImGui::SameLine();

  const char* action_label =
      (mode_ == Mode::kImport) ? "Import##action" : "Save##action";
  if (ImGui::Button(action_label, ImVec2(btn_w, 0.f))) {
    Commit(scene);
    open_ = false;
  }
}

void MeshEditorWindow::Commit(EditorScene* scene) {
  const mesh::MeshData transformed = BuildTransformedMesh();

  // Determine output path.
  std::filesystem::path out_path;
  if (mode_ == Mode::kImport) {
    const std::string name(mesh_name_buf_);
    out_path = core::Config::GetDataFolder() / "meshes" / (name + ".emesh");
  } else {
    out_path = source_path_;
  }

  if (!mesh::EmeshWriter{}.Write(transformed, out_path.string())) {
    LOG_F(ERROR, "MeshEditorWindow: failed to write '%s'",
          out_path.string().c_str());
    return;
  }
  LOG_F(INFO, "MeshEditorWindow: wrote '%s'", out_path.string().c_str());

  if (mode_ == Mode::kImport && scene) {
    game::MeshTemplate* tmpl =
        game::MeshTemplate::GetOrLoad(out_path.string(), video_);
    if (tmpl) {
      scene->AddMeshTemplate(tmpl);
      LOG_F(INFO, "MeshEditorWindow: added MeshTemplate '%s' to scene",
            out_path.filename().string().c_str());
    } else {
      LOG_F(ERROR, "MeshEditorWindow: failed to load template from '%s'",
            out_path.string().c_str());
    }
  }
}

}  // namespace editor
