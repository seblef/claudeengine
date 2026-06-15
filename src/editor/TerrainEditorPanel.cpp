#include "editor/TerrainEditorPanel.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>
#include <loguru.hpp>
#include <nfd.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include <tinyexr.h>

#include "abstract/VideoDevice.h"
#include "core/Config.h"
#include "editor/EditorCommandHistory.h"
#include "editor/TerrainPainterWindow.h"
#include "editor/tools/TerrainSculptTool.h"
#include "game/GameTerrain.h"
#include "renderer/FoliageRenderer.h"
#include "renderer/Renderer.h"
#include "terrain/FoliageLayer.h"
#include "terrain/TerrainData.h"
#include "terrain/TerrainGenerator.h"
#include "terrain/TerrainMaterial.h"
#include "terrain/TerrainRenderer.h"

namespace {
// Predefined swatch colours for the four paint layers (RGBA float).
// Used as a visual hint when the actual albedo texture colour is unavailable.
constexpr ImVec4 kLayerSwatchColors[terrain::kMaxTerrainLayers] = {
    {0.30f, 0.65f, 0.25f, 1.f},   // layer 0 — green  (grass)
    {0.50f, 0.45f, 0.35f, 1.f},   // layer 1 — brown  (rock/earth)
    {0.80f, 0.75f, 0.55f, 1.f},   // layer 2 — tan    (sand)
    {0.90f, 0.92f, 0.95f, 1.f},   // layer 3 — white  (snow)
};
}  // namespace

namespace editor {

TerrainEditorPanel::TerrainEditorPanel() = default;
TerrainEditorPanel::~TerrainEditorPanel() = default;

void TerrainEditorPanel::SetContext(terrain::TerrainData* data,
                                    terrain::TerrainMaterial* material,
                                    abstract::VideoDevice* video,
                                    EditorCommandHistory* history,
                                    game::GameTerrain* terrain_obj) {
  data_         = data;
  material_     = material;
  video_        = video;
  history_      = history;
  terrain_obj_  = terrain_obj;

  sculpt_tool_.reset();
  if (data) {
    sculpt_tool_ = std::make_unique<TerrainSculptTool>(
        data, material, video, history,
        [this](float wx, float wz, bool first, float dt) {
          if (!terrain_obj_ || !data_) return;
          terrain::FoliageLayer* layer =
              terrain_obj_->GetFoliageLayer(foliage_active_layer_);
          if (!layer) return;
          if (foliage_brush_mode_ == FoliageBrushMode::kPaint)
            layer->PaintBrush(wx, wz, foliage_radius_,
                               foliage_strength_ * dt * 5.f, *data_);
          else
            layer->EraseBrush(wx, wz, foliage_radius_,
                               foliage_strength_ * dt * 5.f, *data_);
        },
        [this]() {
          if (!terrain_obj_ || !data_) return;
          terrain::FoliageLayer* layer =
              terrain_obj_->GetFoliageLayer(foliage_active_layer_);
          if (layer) {
            layer->RebuildInstances(*data_);
            if (renderer::FoliageRenderer::IsInstanced() &&
                renderer::FoliageRenderer::Instance().IsReady())
              renderer::FoliageRenderer::Instance().RebuildDirtyLayers();
          }
          if (on_foliage_modified_) on_foliage_modified_();
        });
  }

  if (!painter_window_)
    painter_window_ = std::make_unique<TerrainPainterWindow>();
  painter_window_->SetContext(data, material, video, history);
}

EditorToolBase* TerrainEditorPanel::GetSculptTool() {
  return sculpt_tool_.get();
}

// ---- Render -----------------------------------------------------------------

void TerrainEditorPanel::Render() {
  if (!data_) {
    ImGui::TextDisabled("No terrain in scene.");
    return;
  }

  if (ImGui::BeginTabBar("##terrain_tabs")) {
    if (ImGui::BeginTabItem("Sculpt")) {
      active_tab_ = ActiveTab::kSculpt;
      if (sculpt_tool_) sculpt_tool_->SetMode(TerrainSculptTool::Mode::kSculpt);
      RenderSculptTab();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Paint")) {
      active_tab_ = ActiveTab::kPaint;
      if (sculpt_tool_) sculpt_tool_->SetMode(TerrainSculptTool::Mode::kPaint);
      RenderPaintTab();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Material")) {
      active_tab_ = ActiveTab::kMaterial;
      RenderMaterialTab();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Properties")) {
      active_tab_ = ActiveTab::kProperties;
      RenderPropertiesTab();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Export")) {
      active_tab_ = ActiveTab::kExport;
      RenderExportTab();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Foliage")) {
      active_tab_ = ActiveTab::kFoliage;
      if (sculpt_tool_) sculpt_tool_->SetMode(TerrainSculptTool::Mode::kFoliage);
      RenderFoliageTab();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
}

void TerrainEditorPanel::RenderSculptTab() {
  if (!sculpt_tool_) return;

  const auto cur_tool = sculpt_tool_->GetTool();
  ImGui::TextUnformatted("Tool");
  ImGui::SameLine();
  const auto tool_button = [&](const char* label, TerrainSculptTool::Tool t) {
    if (cur_tool == t) {
      ImGui::PushStyleColor(ImGuiCol_Button,
                            ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    }
    if (ImGui::Button(label)) sculpt_tool_->SetTool(t);
    if (cur_tool == t) ImGui::PopStyleColor();
    ImGui::SameLine();
  };
  tool_button("Raise##tool",   TerrainSculptTool::Tool::kRaise);
  tool_button("Lower##tool",   TerrainSculptTool::Tool::kLower);
  tool_button("Smooth##tool",  TerrainSculptTool::Tool::kSmooth);
  tool_button("Flatten##tool", TerrainSculptTool::Tool::kFlatten);
  ImGui::NewLine();

  float radius = sculpt_tool_->GetRadius();
  if (ImGui::SliderFloat("Radius (m)", &radius, 0.5f, 200.f, "%.1f"))
    sculpt_tool_->SetRadius(radius);

  float strength = sculpt_tool_->GetStrength();
  if (ImGui::SliderFloat("Strength", &strength, 0.01f, 1.f, "%.2f"))
    sculpt_tool_->SetStrength(strength);

  const auto cur_falloff = sculpt_tool_->GetFalloff();
  ImGui::TextUnformatted("Falloff");
  ImGui::SameLine();
  if (ImGui::RadioButton("Linear##falloff",
                          cur_falloff == TerrainSculptTool::Falloff::kLinear))
    sculpt_tool_->SetFalloff(TerrainSculptTool::Falloff::kLinear);
  ImGui::SameLine();
  if (ImGui::RadioButton("Smooth##falloff",
                          cur_falloff == TerrainSculptTool::Falloff::kSmooth))
    sculpt_tool_->SetFalloff(TerrainSculptTool::Falloff::kSmooth);

  ImGui::Spacing();
  ImGui::TextDisabled("Left-drag in viewport to sculpt.");
}

void TerrainEditorPanel::RenderPaintTab() {
  if (!material_ || material_->GetLayerCount() == 0 || !sculpt_tool_) {
    ImGui::TextDisabled("No terrain material layers defined.");
    return;
  }

  const int active_layer = sculpt_tool_->GetActiveLayer();
  ImGui::TextUnformatted("Layer");
  ImGui::SameLine();

  const int layer_count = material_->GetLayerCount();
  for (int i = 0; i < layer_count; ++i) {
    const ImVec4& col = kLayerSwatchColors[i];
    if (active_layer == i) {
      ImGui::PushStyleColor(ImGuiCol_Button,         col);
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  col);
      ImGui::PushStyleColor(ImGuiCol_ButtonActive,   col);
      ImGui::PushStyleColor(ImGuiCol_Border,
                            ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));
      const float prev_border = ImGui::GetStyle().FrameBorderSize;
      ImGui::GetStyle().FrameBorderSize = 2.f;
      char label[32];
      snprintf(label, sizeof(label), "%d##layer", i);
      ImGui::Button(label, {28.f, 28.f});  // already active; click is a no-op
      ImGui::GetStyle().FrameBorderSize = prev_border;
      ImGui::PopStyleColor(4);
    } else {
      ImGui::PushStyleColor(ImGuiCol_Button,        col);
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col);
      ImGui::PushStyleColor(ImGuiCol_ButtonActive,  col);
      char label[32];
      snprintf(label, sizeof(label), "%d##layer", i);
      if (ImGui::Button(label, {28.f, 28.f})) sculpt_tool_->SetActiveLayer(i);
      ImGui::PopStyleColor(3);
    }
    if (i < layer_count - 1) ImGui::SameLine();
  }
  ImGui::NewLine();

  float opacity = sculpt_tool_->GetPaintOpacity();
  if (ImGui::SliderFloat("Opacity", &opacity, 0.f, 1.f, "%.2f"))
    sculpt_tool_->SetPaintOpacity(opacity);

  float radius = sculpt_tool_->GetRadius();
  if (ImGui::SliderFloat("Radius (m)", &radius, 0.5f, 200.f, "%.1f"))
    sculpt_tool_->SetRadius(radius);

  const auto cur_falloff = sculpt_tool_->GetFalloff();
  ImGui::TextUnformatted("Falloff");
  ImGui::SameLine();
  if (ImGui::RadioButton("Linear##pfalloff",
                          cur_falloff == TerrainSculptTool::Falloff::kLinear))
    sculpt_tool_->SetFalloff(TerrainSculptTool::Falloff::kLinear);
  ImGui::SameLine();
  if (ImGui::RadioButton("Smooth##pfalloff",
                          cur_falloff == TerrainSculptTool::Falloff::kSmooth))
    sculpt_tool_->SetFalloff(TerrainSculptTool::Falloff::kSmooth);

  ImGui::Spacing();
  ImGui::TextDisabled("Left-drag in viewport to paint.");

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  if (ImGui::Button("Auto-Paint by Height...")) {
    if (!painter_window_)
      painter_window_ = std::make_unique<TerrainPainterWindow>();
    painter_window_->SetContext(data_, material_, video_, history_);
    painter_window_->Open();
  }
  ImGui::SameLine();
  ImGui::TextDisabled("Paint whole terrain by height range.");
}

void TerrainEditorPanel::RenderPainterWindow() {
  if (painter_window_)
    painter_window_->Render();
}

// ---- Material tab -----------------------------------------------------------

std::string TerrainEditorPanel::BrowseTexture() {
  const std::filesystem::path textures_root =
      core::Config::GetDataFolder() / "textures";
  const std::string root_str = textures_root.string();

  nfdu8char_t* out_path = nullptr;
  const nfdu8filteritem_t filter = {"Images", "png,jpg,jpeg,tga,bmp"};
  const nfdresult_t result = NFD_OpenDialogU8(&out_path, &filter, 1,
                                               root_str.c_str());
  if (result != NFD_OKAY) {
    if (result == NFD_ERROR)
      LOG_F(ERROR, "TerrainEditorPanel::BrowseTexture — NFD error");
    return {};
  }

  const std::filesystem::path abs(out_path);
  NFD_FreePathU8(out_path);

  std::error_code ec;
  const auto rel = std::filesystem::relative(abs, textures_root, ec);
  if (ec || rel.string().substr(0, 2) == "..") {
    LOG_F(WARNING, "TerrainEditorPanel::BrowseTexture — file outside "
          "data/textures/: %s", abs.string().c_str());
    return {};
  }
  return rel.string();
}

void TerrainEditorPanel::RenderMaterialTab() {
  if (!material_) {
    ImGui::TextDisabled("No terrain material.");
    return;
  }

  const int layer_count = material_->GetLayerCount();

  for (int i = 0; i < layer_count; ++i) {
    ImGui::PushID(i);
    char header[32];
    snprintf(header, sizeof(header), "Layer %d", i);
    if (ImGui::CollapsingHeader(header, ImGuiTreeNodeFlags_DefaultOpen)) {
      const terrain::TerrainMaterialLayer& layer = material_->GetLayer(i);

      // Albedo row.
      ImGui::TextUnformatted("Albedo");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(-80.f);
      char albedo_buf[512];
      snprintf(albedo_buf, sizeof(albedo_buf), "%s",
               layer.albedo_path.c_str());
      ImGui::InputText("##albedo", albedo_buf, sizeof(albedo_buf),
                       ImGuiInputTextFlags_ReadOnly);
      ImGui::SameLine();
      if (ImGui::Button("Browse##albedo")) {
        const std::string path = BrowseTexture();
        if (!path.empty())
          material_->SetLayerAlbedo(i, path, video_);
      }

      // Normal row.
      ImGui::TextUnformatted("Normal");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(-80.f);
      char normal_buf[512];
      snprintf(normal_buf, sizeof(normal_buf), "%s",
               layer.normal_path.c_str());
      ImGui::InputText("##normal", normal_buf, sizeof(normal_buf),
                       ImGuiInputTextFlags_ReadOnly);
      ImGui::SameLine();
      if (ImGui::Button("Browse##normal")) {
        const std::string path = BrowseTexture();
        if (!path.empty())
          material_->SetLayerNormal(i, path, video_);
      }

      // Tiling row.
      float tiling = layer.tiling;
      if (ImGui::DragFloat("Tiling", &tiling, 0.1f, 0.01f, 256.f, "%.2f"))
        material_->SetLayerTiling(i, tiling);
    }
    ImGui::PopID();
  }

  ImGui::Spacing();
  if (layer_count < terrain::kMaxTerrainLayers) {
    if (ImGui::Button("+ Add Layer"))
      material_->AddLayer();
  } else {
    ImGui::TextDisabled("Maximum of %d layers reached.", terrain::kMaxTerrainLayers);
  }
}

// ---- Properties tab ---------------------------------------------------------

void TerrainEditorPanel::RenderPropertiesTab() {
  if (!data_) return;

  ImGui::SeparatorText("Height Range");
  ImGui::TextWrapped("Changing the height range reinterprets the existing "
                     "heightmap and rebuilds the normal map.");
  ImGui::Spacing();

  float min_h = data_->GetMinHeight();
  float max_h = data_->GetMaxHeight();

  const bool min_changed = ImGui::DragFloat("Min Height (m)", &min_h,
                                             0.5f, -10000.f, max_h - 0.1f, "%.1f");
  ImGui::IsItemDeactivatedAfterEdit();

  const bool max_changed = ImGui::DragFloat("Max Height (m)", &max_h,
                                             0.5f, min_h + 0.1f, 10000.f, "%.1f");
  ImGui::IsItemDeactivatedAfterEdit();

  if (min_changed || max_changed) {
    max_h = std::max(max_h, min_h + 0.1f);
    data_->SetMinHeight(min_h);
    data_->SetMaxHeight(max_h);
    if (terrain::TerrainRenderer::IsInstanced())
      terrain::TerrainRenderer::Instance().SetHeightRange(min_h, max_h);
  }

  ImGui::Spacing();
  ImGui::SeparatorText("Info");
  ImGui::LabelText("Width (texels)",  "%d", data_->GetTexelWidth());
  ImGui::LabelText("Height (texels)", "%d", data_->GetTexelHeight());
  ImGui::LabelText("Resolution (m/texel)", "%.3f", data_->GetMetersPerTexel());
  ImGui::LabelText("World size (m)",
                   "%.1f × %.1f",
                   data_->GetWorldWidth(), data_->GetWorldHeight());
}

// ---- Import window ----------------------------------------------------------

void TerrainEditorPanel::SetOnCreateFromImport(
    std::function<void(std::vector<uint16_t>, int, int, float, float)> cb) {
  on_create_from_import_ = std::move(cb);
}

void TerrainEditorPanel::OpenImportWindow() {
  show_import_window_ = true;
  io_status_msg_.clear();
}

void TerrainEditorPanel::OpenGenerateWindow() {
  show_generate_window_ = true;
}

void TerrainEditorPanel::RenderGenerateWindow() {
  if (!show_generate_window_) return;

  ImGui::SetNextWindowSize({420.f, 0.f}, ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Generate Terrain", &show_generate_window_,
                    ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::End();
    return;
  }

  // ---- Terrain size ----------------------------------------------------------
  ImGui::SeparatorText("Terrain Size");
  ImGui::DragFloat("World width (m)",  &gen_world_width_,      1.f,  1.f, 100000.f, "%.1f");
  ImGui::DragFloat("World depth (m)",  &gen_world_depth_,      1.f,  1.f, 100000.f, "%.1f");
  ImGui::DragFloat("Metres per texel", &gen_meters_per_texel_, 0.1f, 0.1f,    10.f, "%.2f");
  gen_world_width_       = std::max(gen_world_width_,       1.f);
  gen_world_depth_       = std::max(gen_world_depth_,       1.f);
  gen_meters_per_texel_  = std::max(gen_meters_per_texel_,  0.1f);

  const int texel_w = static_cast<int>(gen_world_width_  / gen_meters_per_texel_);
  const int texel_h = static_cast<int>(gen_world_depth_ / gen_meters_per_texel_);
  ImGui::TextDisabled("→  %d × %d texels", texel_w, texel_h);

  // ---- Height range ----------------------------------------------------------
  ImGui::SeparatorText("Height Range");
  ImGui::DragFloat("Min height (m)", &gen_min_h_, 0.5f, -10000.f,
                   gen_max_h_ - 0.1f, "%.1f");
  ImGui::DragFloat("Max height (m)", &gen_max_h_, 0.5f,
                   gen_min_h_ + 0.1f,  10000.f, "%.1f");
  gen_max_h_ = std::max(gen_max_h_, gen_min_h_ + 0.1f);

  // ---- Algorithm -------------------------------------------------------------
  ImGui::SeparatorText("Algorithm");
  const char* algo_names[] = {
      "fBm (Fractal Brownian Motion)",
      "Ridged Multi-fractal",
  };
  int algo_idx = static_cast<int>(gen_algorithm_);
  if (ImGui::BeginCombo("##algo", algo_names[algo_idx])) {
    for (int i = 0; i < 2; ++i) {
      const bool selected = (algo_idx == i);
      if (ImGui::Selectable(algo_names[i], selected))
        gen_algorithm_ = static_cast<GenAlgorithm>(i);
      if (selected)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }

  // ---- fBm parameters --------------------------------------------------------
  if (gen_algorithm_ == GenAlgorithm::kFbm) {
    ImGui::SeparatorText("fBm Parameters");
    ImGui::DragInt  ("Seed",        &gen_fbm_params_.seed,        1.f);
    ImGui::DragFloat("Scale (m)",   &gen_fbm_params_.scale,       1.f,   1.f, 10000.f, "%.1f");
    ImGui::DragInt  ("Octaves",     &gen_fbm_params_.octaves,     1.f,   1,       16);
    ImGui::DragFloat("Persistence", &gen_fbm_params_.persistence, 0.01f, 0.01f,   1.f, "%.2f");
    ImGui::DragFloat("Lacunarity",  &gen_fbm_params_.lacunarity,  0.01f, 1.f,     8.f, "%.2f");
    gen_fbm_params_.octaves     = std::max(1,     gen_fbm_params_.octaves);
    gen_fbm_params_.scale       = std::max(1.f,   gen_fbm_params_.scale);
    gen_fbm_params_.persistence = std::clamp(gen_fbm_params_.persistence, 0.01f, 1.f);
    gen_fbm_params_.lacunarity  = std::max(1.f,   gen_fbm_params_.lacunarity);

    ImGui::Spacing();
    if (ImGui::Button("Randomize seed")) {
      gen_fbm_params_.seed = std::rand();  // NOLINT(cert-msc50-cpp)
    }
    ImGui::SameLine();
    ImGui::TextDisabled("then hit Generate to audition");
  }

  // ---- Ridged parameters -----------------------------------------------------
  if (gen_algorithm_ == GenAlgorithm::kRidged) {
    ImGui::SeparatorText("Ridged Parameters");
    ImGui::DragInt  ("Seed",       &gen_ridged_params_.seed,       1.f);
    ImGui::DragFloat("Scale (m)",  &gen_ridged_params_.scale,      1.f,   1.f, 10000.f, "%.1f");
    ImGui::DragInt  ("Octaves",    &gen_ridged_params_.octaves,    1.f,   1,       16);
    ImGui::DragFloat("Lacunarity", &gen_ridged_params_.lacunarity, 0.01f, 1.f,     8.f, "%.2f");
    ImGui::DragFloat("Gain",       &gen_ridged_params_.gain,       0.01f, 0.01f,  10.f, "%.2f");
    ImGui::DragFloat("Offset",     &gen_ridged_params_.offset,     0.01f, 0.01f,   4.f, "%.2f");
    gen_ridged_params_.octaves    = std::max(1,    gen_ridged_params_.octaves);
    gen_ridged_params_.scale      = std::max(1.f,  gen_ridged_params_.scale);
    gen_ridged_params_.lacunarity = std::max(1.f,  gen_ridged_params_.lacunarity);
    gen_ridged_params_.gain       = std::max(0.01f, gen_ridged_params_.gain);
    gen_ridged_params_.offset     = std::max(0.01f, gen_ridged_params_.offset);

    ImGui::Spacing();
    if (ImGui::Button("Randomize seed")) {
      gen_ridged_params_.seed = std::rand();  // NOLINT(cert-msc50-cpp)
    }
    ImGui::SameLine();
    ImGui::TextDisabled("then hit Generate to audition");
  }

  // ---- Erosion ---------------------------------------------------------------
  ImGui::Spacing();
  if (ImGui::CollapsingHeader("Erosion")) {
    ImGui::Checkbox("Enable erosion", &gen_erosion_enabled_);

    ImGui::BeginDisabled(!gen_erosion_enabled_);
    ImGui::DragInt  ("Iterations##ero",       &gen_erosion_params_.iterations,
                     500.f, 1000, 500000);
    ImGui::DragInt  ("Seed##ero",             &gen_erosion_params_.seed,       1.f);
    ImGui::DragFloat("Inertia##ero",          &gen_erosion_params_.inertia,
                     0.005f, 0.f, 1.f, "%.3f");
    ImGui::DragFloat("Capacity##ero",         &gen_erosion_params_.capacity,
                     0.1f, 0.1f, 20.f, "%.2f");
    ImGui::DragFloat("Deposition rate##ero",  &gen_erosion_params_.deposition,
                     0.01f, 0.01f, 1.f, "%.2f");
    ImGui::DragFloat("Erosion rate##ero",     &gen_erosion_params_.erosion,
                     0.01f, 0.01f, 1.f, "%.2f");
    ImGui::DragFloat("Evaporation rate##ero", &gen_erosion_params_.evaporation,
                     0.001f, 0.001f, 0.1f, "%.3f");

    gen_erosion_params_.iterations  = std::max(1,    gen_erosion_params_.iterations);
    gen_erosion_params_.inertia     = std::clamp(gen_erosion_params_.inertia,    0.f, 1.f);
    gen_erosion_params_.capacity    = std::max(0.1f, gen_erosion_params_.capacity);
    gen_erosion_params_.deposition  = std::clamp(gen_erosion_params_.deposition, 0.01f, 1.f);
    gen_erosion_params_.erosion     = std::clamp(gen_erosion_params_.erosion,    0.01f, 1.f);
    gen_erosion_params_.evaporation = std::clamp(gen_erosion_params_.evaporation, 0.001f, 0.1f);
    ImGui::EndDisabled();
  }

  // ---- Generate --------------------------------------------------------------
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  const bool can_generate = (texel_w > 0 && texel_h > 0);
  ImGui::BeginDisabled(!can_generate);
  if (ImGui::Button("Generate", {-1.f, 0.f})) {
    std::vector<uint16_t> hmap;
    if (gen_algorithm_ == GenAlgorithm::kFbm) {
      hmap = terrain::TerrainGenerator::GenerateFbm(
          texel_w, texel_h,
          gen_meters_per_texel_,
          gen_min_h_, gen_max_h_,
          gen_fbm_params_);
    } else {
      hmap = terrain::TerrainGenerator::GenerateRidged(
          texel_w, texel_h,
          gen_meters_per_texel_,
          gen_min_h_, gen_max_h_,
          gen_ridged_params_);
    }

    if (gen_erosion_enabled_) {
      terrain::TerrainGenerator::Erode(hmap, texel_w, texel_h,
                                       gen_meters_per_texel_,
                                       gen_erosion_params_);
    }

    if (!data_) {
      if (on_create_from_import_)
        on_create_from_import_(std::move(hmap), texel_w, texel_h,
                               gen_min_h_, gen_max_h_);
    } else {
      const bool needs_resize =
          (texel_w != data_->GetTexelWidth() || texel_h != data_->GetTexelHeight());
      if (needs_resize) {
        io_pending_data_  = std::move(hmap);
        io_pending_w_     = texel_w;
        io_pending_h_     = texel_h;
        io_pending_min_h_ = gen_min_h_;
        io_pending_max_h_ = gen_max_h_;
        io_state_         = IoState::kConfirmResize;
      } else {
        ApplyImportedHeightmap(hmap, texel_w, texel_h, false,
                               gen_min_h_, gen_max_h_);
      }
    }
  }
  ImGui::EndDisabled();

  // Resize-confirmation modal is only shown when a terrain already exists.
  if (data_)
    RenderImportStatusAndModal();

  ImGui::End();
}

void TerrainEditorPanel::RenderImportStatusAndModal() {
  if (io_state_ == IoState::kConfirmResize) {
    ImGui::OpenPopup("Resize Terrain?##io");
  }
  if (ImGui::BeginPopupModal("Resize Terrain?##io", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextWrapped(
        "The imported heightmap is %d\xc3\x97%d but the terrain is %d\xc3\x97%d.\n"
        "Resizing will rebuild the CDLOD quadtree and reset the splatmap to default.\n"
        "Continue?",
        io_pending_w_, io_pending_h_,
        data_->GetTexelWidth(), data_->GetTexelHeight());
    ImGui::Spacing();
    if (ImGui::Button("Yes, resize", {120.f, 0.f})) {
      ApplyImportedHeightmap(std::move(io_pending_data_),
                             io_pending_w_, io_pending_h_, true,
                             io_pending_min_h_, io_pending_max_h_);
      io_state_ = IoState::kIdle;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", {120.f, 0.f})) {
      io_pending_data_.clear();
      io_state_ = IoState::kIdle;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (!io_status_msg_.empty()) {
    if (io_status_ok_)
      ImGui::TextColored({0.4f, 0.9f, 0.4f, 1.f}, "%s", io_status_msg_.c_str());
    else
      ImGui::TextColored({0.9f, 0.3f, 0.3f, 1.f}, "%s", io_status_msg_.c_str());
    ImGui::Spacing();
  }
}

void TerrainEditorPanel::RenderImportWindow() {
  if (!show_import_window_) return;

  ImGui::SetNextWindowSize({400.f, 0.f}, ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Terrain Import", &show_import_window_,
                   ImGuiWindowFlags_AlwaysAutoResize)) {
    // Resize-confirmation modal is only relevant when an existing terrain is
    // being replaced; show it (and the shared status) only in that case.
    if (data_) {
      RenderImportStatusAndModal();
    } else if (!io_status_msg_.empty()) {
      if (io_status_ok_)
        ImGui::TextColored({0.4f, 0.9f, 0.4f, 1.f}, "%s", io_status_msg_.c_str());
      else
        ImGui::TextColored({0.9f, 0.3f, 0.3f, 1.f}, "%s", io_status_msg_.c_str());
      ImGui::Spacing();
    }

    ImGui::TextWrapped(
        "PNG/HDR/EXR: pixel values are normalised to [0\xe2\x80\x93""1] "
        "(min pixel \xe2\x86\x92 0, max pixel \xe2\x86\x92 1), "
        "then mapped to height [min, max] (configurable).");
    ImGui::Spacing();

    ImGui::DragFloat("Min Height##import", &import_min_h_, 0.5f,
                     -10000.f, import_max_h_ - 0.1f, "%.1f m");
    ImGui::DragFloat("Max Height##import", &import_max_h_, 0.5f,
                     import_min_h_ + 0.1f, 10000.f, "%.1f m");
    import_max_h_ = std::max(import_max_h_, import_min_h_ + 0.1f);

    ImGui::Spacing();

    if (ImGui::Button("Import PNG...", {-1.f, 0.f})) {
      nfdu8char_t* out_path = nullptr;
      const nfdu8filteritem_t filter = {"PNG Heightmap", "png"};
      const nfdresult_t res =
          NFD_OpenDialogU8(&out_path, &filter, 1, nullptr);
      if (res == NFD_OKAY) {
        const std::string path(out_path);
        NFD_FreePathU8(out_path);
        LoadAndApplyPNG(path);
      } else if (res == NFD_ERROR) {
        LOG_F(ERROR, "TerrainEditorPanel: NFD error on PNG import");
      }
    }

    if (ImGui::Button("Import HDR / EXR...", {-1.f, 0.f})) {
      nfdu8char_t* out_path = nullptr;
      const nfdu8filteritem_t filter = {"HDR / EXR Heightmap", "hdr,exr"};
      const nfdresult_t res =
          NFD_OpenDialogU8(&out_path, &filter, 1, nullptr);
      if (res == NFD_OKAY) {
        const std::string path(out_path);
        NFD_FreePathU8(out_path);
        LoadAndApplyHDR(path);
      } else if (res == NFD_ERROR) {
        LOG_F(ERROR, "TerrainEditorPanel: NFD error on HDR import");
      }
    }
  }
  ImGui::End();
}

// ---- Export tab -------------------------------------------------------------

void TerrainEditorPanel::RenderExportTab() {
  if (!data_) return;

  if (!io_status_msg_.empty()) {
    if (io_status_ok_)
      ImGui::TextColored({0.4f, 0.9f, 0.4f, 1.f}, "%s", io_status_msg_.c_str());
    else
      ImGui::TextColored({0.9f, 0.3f, 0.3f, 1.f}, "%s", io_status_msg_.c_str());
    ImGui::Spacing();
  }

  ImGui::TextWrapped(
      "PNG: 8-bit grayscale, maps [min, max] height to [0\xe2\x80\x93""255].\n"
      "R16: raw uint16 binary (row-major, same format as map serialisation).");
  ImGui::Spacing();

  if (ImGui::Button("Export PNG...", {-1.f, 0.f})) {
    nfdu8char_t* out_path = nullptr;
    const nfdu8filteritem_t filter = {"PNG Heightmap", "png"};
    const nfdresult_t res =
        NFD_SaveDialogU8(&out_path, &filter, 1, nullptr, "heightmap.png");
    if (res == NFD_OKAY) {
      const std::string path(out_path);
      NFD_FreePathU8(out_path);
      DoExportPNG(path);
    } else if (res == NFD_ERROR) {
      LOG_F(ERROR, "TerrainEditorPanel: NFD error on PNG export");
    }
  }

  if (ImGui::Button("Export R16...", {-1.f, 0.f})) {
    nfdu8char_t* out_path = nullptr;
    const nfdu8filteritem_t filter = {"Raw 16-bit Heightmap", "r16"};
    const nfdresult_t res =
        NFD_SaveDialogU8(&out_path, &filter, 1, nullptr, "heightmap.r16");
    if (res == NFD_OKAY) {
      const std::string path(out_path);
      NFD_FreePathU8(out_path);
      DoExportR16(path);
    } else if (res == NFD_ERROR) {
      LOG_F(ERROR, "TerrainEditorPanel: NFD error on R16 export");
    }
  }
}

bool TerrainEditorPanel::LoadAndApplyPNG(const std::string& path) {
  io_status_msg_.clear();
  int w = 0, h = 0, channels = 0;
  uint8_t* pixels = stbi_load(path.c_str(), &w, &h, &channels, 1);
  if (!pixels) {
    io_status_msg_ = std::string("PNG load failed: ") + stbi_failure_reason();
    io_status_ok_  = false;
    LOG_F(ERROR, "TerrainEditorPanel: PNG load failed '%s': %s",
          path.c_str(), stbi_failure_reason());
    return false;
  }

  const std::size_t n = static_cast<std::size_t>(w) * h;
  std::vector<uint16_t> samples(n);

  // Normalise to [0, 1] before mapping to the editor height range.
  uint8_t min_px = pixels[0];
  uint8_t max_px = pixels[0];
  for (std::size_t i = 1; i < n; ++i) {
    if (pixels[i] < min_px) min_px = pixels[i];
    if (pixels[i] > max_px) max_px = pixels[i];
  }
  const float px_range = (max_px > min_px)
      ? static_cast<float>(max_px - min_px) : 1.f;

  for (std::size_t i = 0; i < n; ++i) {
    const float t = (pixels[i] - min_px) / px_range;
    samples[i] = static_cast<uint16_t>(t * 65535.f + 0.5f);
  }
  stbi_image_free(pixels);

  if (!data_) {
    if (on_create_from_import_)
      on_create_from_import_(std::move(samples), w, h, import_min_h_, import_max_h_);
    io_status_msg_ = "Import successful.";
    io_status_ok_  = true;
    return true;
  }

  const bool needs_resize =
      (w != data_->GetTexelWidth() || h != data_->GetTexelHeight());
  if (needs_resize) {
    io_pending_data_  = std::move(samples);
    io_pending_w_     = w;
    io_pending_h_     = h;
    io_pending_min_h_ = import_min_h_;
    io_pending_max_h_ = import_max_h_;
    io_state_         = IoState::kConfirmResize;
    return true;
  }

  ApplyImportedHeightmap(samples, w, h, false, import_min_h_, import_max_h_);
  return true;
}

bool TerrainEditorPanel::LoadAndApplyHDR(const std::string& path) {
  io_status_msg_.clear();

  int w = 0, h = 0;
  std::vector<float> pixels;

  const std::string ext = std::filesystem::path(path).extension().string();
  const bool is_exr = (ext == ".exr" || ext == ".EXR");

  if (is_exr) {
    const char* err = nullptr;
    float* exr_rgba = nullptr;
    const int ret = LoadEXR(&exr_rgba, &w, &h, path.c_str(), &err);
    if (ret != TINYEXR_SUCCESS) {
      const std::string reason = err ? err : "unknown EXR error";
      FreeEXRErrorMessage(err);
      io_status_msg_ = "EXR load failed: " + reason;
      io_status_ok_  = false;
      LOG_F(ERROR, "TerrainEditorPanel: EXR load failed '%s': %s",
            path.c_str(), reason.c_str());
      return false;
    }
    // EXR is loaded as RGBA floats; use the R channel as the height value.
    const std::size_t n = static_cast<std::size_t>(w) * h;
    pixels.resize(n);
    for (std::size_t i = 0; i < n; ++i)
      pixels[i] = exr_rgba[i * 4];
    free(exr_rgba);
  } else {
    int channels = 0;
    float* raw = stbi_loadf(path.c_str(), &w, &h, &channels, 1);
    if (!raw) {
      io_status_msg_ = std::string("HDR load failed: ") + stbi_failure_reason();
      io_status_ok_  = false;
      LOG_F(ERROR, "TerrainEditorPanel: HDR load failed '%s': %s",
            path.c_str(), stbi_failure_reason());
      return false;
    }
    pixels.assign(raw, raw + static_cast<std::size_t>(w) * h);
    stbi_image_free(raw);
  }

  // Normalise to [0, 1] before mapping to the editor height range.
  const std::size_t n = pixels.size();
  float min_val = pixels[0];
  float max_val = pixels[0];
  for (std::size_t i = 1; i < n; ++i) {
    if (pixels[i] < min_val) min_val = pixels[i];
    if (pixels[i] > max_val) max_val = pixels[i];
  }
  const float val_range = (max_val > min_val) ? (max_val - min_val) : 1.f;

  std::vector<uint16_t> samples(n);
  for (std::size_t i = 0; i < n; ++i) {
    const float t = (pixels[i] - min_val) / val_range;
    samples[i] = static_cast<uint16_t>(t * 65535.f + 0.5f);
  }

  if (!data_) {
    if (on_create_from_import_)
      on_create_from_import_(std::move(samples), w, h, import_min_h_, import_max_h_);
    io_status_msg_ = "Import successful.";
    io_status_ok_  = true;
    return true;
  }

  const bool needs_resize =
      (w != data_->GetTexelWidth() || h != data_->GetTexelHeight());
  if (needs_resize) {
    io_pending_data_  = std::move(samples);
    io_pending_w_     = w;
    io_pending_h_     = h;
    io_pending_min_h_ = import_min_h_;
    io_pending_max_h_ = import_max_h_;
    io_state_         = IoState::kConfirmResize;
    return true;
  }

  ApplyImportedHeightmap(samples, w, h, false, import_min_h_, import_max_h_);
  return true;
}

void TerrainEditorPanel::ApplyImportedHeightmap(const std::vector<uint16_t>& data,
                                                int w, int h,
                                                bool needs_resize,
                                                float min_h, float max_h) {
  data_->SetMinHeight(min_h);
  data_->SetMaxHeight(max_h);
  data_->ReplaceHeightmap(data.data(), w, h);

  if (needs_resize) {
    if (terrain::TerrainRenderer::IsInstanced())
      terrain::TerrainRenderer::Instance().Rebuild(*data_);
    if (material_)
      material_->ResetSplatmap(video_, w, h);
    LOG_F(INFO, "TerrainEditorPanel: heightmap imported and terrain resized to %dx%d",
          w, h);
  } else {
    if (terrain::TerrainRenderer::IsInstanced()) {
      terrain::TerrainRenderer::Instance().SetHeightRange(min_h, max_h);
      terrain::TerrainRenderer::Instance().UpdateHeightmapTile(
          0, 0, w, h, *data_);
    }
    LOG_F(INFO, "TerrainEditorPanel: heightmap imported %dx%d", w, h);
  }

  history_->Clear();
  io_status_msg_ = "Import successful.";
  io_status_ok_  = true;
}

void TerrainEditorPanel::DoExportPNG(const std::string& path) {
  io_status_msg_.clear();
  const int w = data_->GetTexelWidth();
  const int h = data_->GetTexelHeight();
  const std::size_t n = static_cast<std::size_t>(w) * h;

  std::vector<uint8_t> pixels(n);
  for (std::size_t i = 0; i < n; ++i)
    pixels[i] = static_cast<uint8_t>(
        (data_->GetRawData()[i] / 65535.f) * 255.f + 0.5f);

  const int ok = stbi_write_png(path.c_str(), w, h, 1, pixels.data(), w);
  if (!ok) {
    io_status_msg_ = "PNG export failed (check write permissions).";
    io_status_ok_  = false;
    LOG_F(ERROR, "TerrainEditorPanel: PNG export failed '%s'", path.c_str());
    return;
  }

  io_status_msg_ = "Export successful.";
  io_status_ok_  = true;
  LOG_F(INFO, "TerrainEditorPanel: heightmap exported to '%s'", path.c_str());
}

void TerrainEditorPanel::DoExportR16(const std::string& path) {
  io_status_msg_.clear();
  const int w = data_->GetTexelWidth();
  const int h = data_->GetTexelHeight();

  std::FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) {
    io_status_msg_ = "R16 export failed: cannot open file for writing.";
    io_status_ok_  = false;
    LOG_F(ERROR, "TerrainEditorPanel: R16 export failed '%s'", path.c_str());
    return;
  }

  std::fwrite(data_->GetRawData(), sizeof(uint16_t),
              static_cast<std::size_t>(w) * h, f);
  std::fclose(f);

  io_status_msg_ = "Export successful.";
  io_status_ok_  = true;
  LOG_F(INFO, "TerrainEditorPanel: R16 heightmap exported to '%s'", path.c_str());
}

// ---- Foliage tab ------------------------------------------------------------

std::string TerrainEditorPanel::BrowseMesh() {
  const std::filesystem::path data_root = core::Config::GetDataFolder();
  const std::string root_str = data_root.string();

  nfdu8char_t* out_path = nullptr;
  const nfdu8filteritem_t filter = {"Mesh files", "obj,fbx,emesh"};
  const nfdresult_t result =
      NFD_OpenDialogU8(&out_path, &filter, 1, root_str.c_str());
  if (result != NFD_OKAY) {
    if (result == NFD_ERROR)
      LOG_F(ERROR, "TerrainEditorPanel::BrowseMesh — NFD error");
    return {};
  }

  const std::filesystem::path abs(out_path);
  NFD_FreePathU8(out_path);

  std::error_code ec;
  const auto rel = std::filesystem::relative(abs, data_root, ec);
  if (ec || rel.string().substr(0, 2) == "..") {
    LOG_F(WARNING, "TerrainEditorPanel::BrowseMesh — file outside data/: %s",
          abs.string().c_str());
    return {};
  }
  return rel.string();
}

void TerrainEditorPanel::RenderFoliageTab() {
  if (!terrain_obj_) {
    ImGui::TextDisabled("No terrain context.");
    return;
  }

  const int layer_count = terrain_obj_->GetFoliageLayerCount();

  // ---- Layer list -----------------------------------------------------------
  ImGui::SeparatorText("Layers");
  for (int i = 0; i < layer_count; ++i) {
    terrain::FoliageLayer* layer = terrain_obj_->GetFoliageLayer(i);
    const terrain::FoliageLayerDesc& desc = layer->GetDesc();

    ImGui::PushID(i);
    const bool selected = (foliage_active_layer_ == i);
    const char* label = desc.name.empty() ? "(unnamed)" : desc.name.c_str();
    if (ImGui::Selectable(label, selected))
      foliage_active_layer_ = i;
    ImGui::PopID();
  }

  // ---- Add / Remove layer ---------------------------------------------------
  if (ImGui::Button("Add layer")) {
    if (layer_count < 8 && data_) {
      terrain::FoliageLayerDesc desc;
      desc.name = "Layer " + std::to_string(terrain_obj_->GetFoliageLayerCount());
      auto layer = std::make_unique<terrain::FoliageLayer>(
          data_->GetTexelWidth(), data_->GetTexelHeight(), desc);
      terrain_obj_->AddFoliageLayer(std::move(layer));
      foliage_active_layer_ = terrain_obj_->GetFoliageLayerCount() - 1;
      if (on_foliage_modified_) on_foliage_modified_();
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Remove layer") && layer_count > 0) {
    terrain_obj_->RemoveFoliageLayer(foliage_active_layer_);
    foliage_active_layer_ = std::max(0, foliage_active_layer_ - 1);
    if (on_foliage_modified_) on_foliage_modified_();
  }

  // ---- Active layer settings ------------------------------------------------
  if (layer_count == 0 || foliage_active_layer_ >= layer_count) return;

  terrain::FoliageLayer*     active = terrain_obj_->GetFoliageLayer(foliage_active_layer_);
  terrain::FoliageLayerDesc& desc   = active->GetDesc();

  ImGui::SeparatorText("Layer settings");

  // Editable layer name.
  char name_buf[256];
  std::snprintf(name_buf, sizeof(name_buf), "%s", desc.name.c_str());
  if (ImGui::InputText("Name", name_buf, sizeof(name_buf))) {
    desc.name = name_buf;
    if (on_foliage_modified_) on_foliage_modified_();
  }

  // Mesh — display current filename, button opens a file dialog.
  {
    const std::string mesh_label =
        desc.mesh_path.empty() ? "(none)" :
        std::filesystem::path(desc.mesh_path).filename().string();
    ImGui::Text("Mesh: %s", mesh_label.c_str());
    ImGui::SameLine();
    if (ImGui::Button("Browse##mesh")) {
      const std::string picked = BrowseMesh();
      if (!picked.empty()) {
        desc.mesh_path = picked;
        if (on_foliage_modified_) on_foliage_modified_();
      }
    }
  }

  // Texture — display current filename, button opens a file dialog.
  {
    const std::string tex_label =
        desc.texture_path.empty() ? "(none)" :
        std::filesystem::path(desc.texture_path).filename().string();
    ImGui::Text("Texture: %s", tex_label.c_str());
    ImGui::SameLine();
    if (ImGui::Button("Browse##tex")) {
      const std::string picked = BrowseTexture();
      if (!picked.empty()) {
        desc.texture_path = picked;
        if (on_foliage_modified_) on_foliage_modified_();
      }
    }
  }

  ImGui::DragFloat("Spacing min", &desc.spacing_min, 0.05f, 0.05f, 10.f);
  if (ImGui::IsItemDeactivatedAfterEdit() && on_foliage_modified_) on_foliage_modified_();
  ImGui::DragFloat("Spacing max", &desc.spacing_max, 0.05f, 0.05f, 10.f);
  if (ImGui::IsItemDeactivatedAfterEdit() && on_foliage_modified_) on_foliage_modified_();
  ImGui::DragFloat("Scale min",   &desc.scale_min,   0.01f, 0.01f, 10.f);
  if (ImGui::IsItemDeactivatedAfterEdit() && on_foliage_modified_) on_foliage_modified_();
  ImGui::DragFloat("Scale max",   &desc.scale_max,   0.01f, 0.01f, 10.f);
  if (ImGui::IsItemDeactivatedAfterEdit() && on_foliage_modified_) on_foliage_modified_();
  ImGui::DragFloat("Cull dist",      &desc.cull_distance,     1.f, 10.f, 500.f);
  if (ImGui::IsItemDeactivatedAfterEdit() && on_foliage_modified_) on_foliage_modified_();
  ImGui::DragFloat("Billboard dist", &desc.billboard_distance, 1.f,  5.f, 200.f);
  if (ImGui::IsItemDeactivatedAfterEdit() && on_foliage_modified_) on_foliage_modified_();

  // ---- Density brush --------------------------------------------------------
  ImGui::SeparatorText("Density brush");

  ImGui::RadioButton("Paint", reinterpret_cast<int*>(&foliage_brush_mode_),
                     static_cast<int>(FoliageBrushMode::kPaint));
  ImGui::SameLine();
  ImGui::RadioButton("Erase", reinterpret_cast<int*>(&foliage_brush_mode_),
                     static_cast<int>(FoliageBrushMode::kErase));

  ImGui::SliderFloat("Radius",   &foliage_radius_,   0.5f, 50.f);
  ImGui::SliderFloat("Strength", &foliage_strength_,  0.f,  1.f);

  // ---- Rebuild --------------------------------------------------------------
  ImGui::SeparatorText("Rebuild");
  if (ImGui::Button("Rebuild instances")) {
    if (data_) {
      active->RebuildInstances(*data_);
      if (renderer::FoliageRenderer::IsInstanced() &&
          renderer::FoliageRenderer::Instance().IsReady())
        renderer::FoliageRenderer::Instance().RebuildDirtyLayers();
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Build all")) {
    if (data_ && video_) {
      std::vector<terrain::FoliageLayer*> ptrs;
      for (int i = 0; i < terrain_obj_->GetFoliageLayerCount(); ++i)
        ptrs.push_back(terrain_obj_->GetFoliageLayer(i));
      renderer::FoliageRenderer::Instance().Build(video_, *data_, ptrs);
      renderer::Renderer::Instance().SetFoliageEnabled(true);
    }
  }
}

}  // namespace editor
