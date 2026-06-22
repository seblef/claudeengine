#include "editor/ResourceBrowser.h"

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <string>

#include <imgui.h>
#include <loguru.hpp>

#include "core/Config.h"
#include "editor/ResourcePanelRegistry.h"

namespace editor {

ResourceBrowser::ResourceBrowser(ResourcePanelRegistry* registry)
    : registry_(registry) {
  Scan();
}

void ResourceBrowser::Render() {
  if (open_dirty_modal_) {
    ImGui::OpenPopup("Unsaved Changes##res_browser");
    open_dirty_modal_ = false;
  }
  if (open_new_modal_) {
    ImGui::OpenPopup("New Resource##res_browser");
    open_new_modal_ = false;
  }
  RenderDirtyCloseModal();
  RenderNewFileModal();

  if (ImGui::Button("Refresh"))
    Scan();

  ImGui::Separator();

  RenderTree();

  ImGui::Spacing();

  RenderPanelTabs();
}

void ResourceBrowser::Scan() {
  file_map_.clear();

  const std::filesystem::path data_dir = core::Config::GetDataFolder();
  if (!std::filesystem::exists(data_dir)) return;

  std::error_code ec;
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(data_dir, ec)) {
    if (!entry.is_regular_file()) continue;
    if (!registry_->CanOpen(entry.path())) continue;

    // Determine the matching extension suffix via the registry.
    for (const std::string& ext : registry_->GetExtensions()) {
      const std::string filename = entry.path().filename().string();
      if (filename.size() >= ext.size() &&
          filename.compare(filename.size() - ext.size(), ext.size(), ext) == 0) {
        file_map_[ext].push_back(entry.path());
        break;
      }
    }
  }

  // Sort entries within each section for stable display order.
  for (auto& [ext, paths] : file_map_) {
    std::sort(paths.begin(), paths.end());
  }
}

int ResourceBrowser::FindOpenPanel(
    const std::filesystem::path& canonical_path) const {
  const auto it = std::find_if(
      open_panels_.begin(), open_panels_.end(),
      [&canonical_path](const OpenPanel& op) {
        return op.canonical_path == canonical_path;
      });
  return it == open_panels_.end()
      ? -1
      : static_cast<int>(std::distance(open_panels_.begin(), it));
}

void ResourceBrowser::OpenOrFocus(const std::filesystem::path& path) {
  std::error_code ec;
  const std::filesystem::path canonical = std::filesystem::canonical(path, ec);
  if (ec) {
    LOG_F(WARNING, "ResourceBrowser: cannot resolve canonical path for '%s'",
          path.string().c_str());
    return;
  }

  const int existing = FindOpenPanel(canonical);
  if (existing >= 0) {
    active_tab_ = existing;
    return;
  }

  auto panel = registry_->Open(path);
  if (!panel) return;

  // Build a display stem by stripping the matching registered extension suffix.
  std::string display = path.filename().string();
  const auto& exts = registry_->GetExtensions();
  const auto ext_it = std::find_if(
      exts.begin(), exts.end(),
      [&display](const std::string& ext) {
        return display.size() >= ext.size() &&
               display.compare(display.size() - ext.size(), ext.size(), ext) == 0;
      });
  if (ext_it != exts.end())
    display.resize(display.size() - ext_it->size());

  active_tab_ = static_cast<int>(open_panels_.size());
  open_panels_.push_back({canonical, display, std::move(panel), false});
}

void ResourceBrowser::RenderTree() {
  for (const std::string& ext : registry_->GetExtensions()) {
    // Section label: strip leading dot from the extension suffix.
    const std::string label = ext.size() > 1 ? ext.substr(1) : ext;

    // AllowOverlap lets the "New" SmallButton overlaid on the right side of the
    // header still receive clicks even though the header is full-width.
    const ImGuiTreeNodeFlags header_flags =
        ImGuiTreeNodeFlags_DefaultOpen |
        (registry_->CanCreate(ext) ? ImGuiTreeNodeFlags_AllowOverlap : 0);
    const bool open = ImGui::CollapsingHeader(label.c_str(), header_flags);

    // "New" button overlaid on the right edge of the section header.
    // SameLine() alone would place it beyond the content region (CollapsingHeader
    // fills the full width), so we pass an explicit X offset instead.
    if (registry_->CanCreate(ext)) {
      const std::string btn_label = "New##new_" + ext;
      const float btn_w = ImGui::CalcTextSize("New").x +
                          ImGui::GetStyle().FramePadding.x * 2.f;
      ImGui::SameLine(ImGui::GetContentRegionMax().x - btn_w);
      if (ImGui::SmallButton(btn_label.c_str())) {
        pending_new_ext_ = ext;
        new_name_buf_[0] = '\0';
        new_error_msg_.clear();
        open_new_modal_  = true;
      }
    }

    if (!open) continue;

    auto it = file_map_.find(ext);
    if (it == file_map_.end() || it->second.empty()) {
      ImGui::TextDisabled("(none)");
      continue;
    }

    for (const std::filesystem::path& path : it->second) {
      std::string stem = path.filename().string();
      // Strip the extension suffix to get a clean display name.
      if (stem.size() >= ext.size())
        stem.resize(stem.size() - ext.size());

      ImGui::Selectable(stem.c_str(), false);
      if (ImGui::IsItemHovered() &&
          ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        OpenOrFocus(path);
      }
    }
  }
}

void ResourceBrowser::RenderPanelTabs() {
  if (open_panels_.empty()) return;

  // Ctrl+S saves the active panel.
  if (active_tab_ >= 0 &&
      active_tab_ < static_cast<int>(open_panels_.size()) &&
      !ImGui::GetIO().WantCaptureKeyboard &&
      ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)) {
    open_panels_[active_tab_].panel->Save();
  }

  constexpr ImGuiTabBarFlags kTabFlags =
      ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs;

  if (!ImGui::BeginTabBar("##res_panels", kTabFlags)) return;

  for (int i = 0; i < static_cast<int>(open_panels_.size()); ) {
    OpenPanel& op = open_panels_[i];

    bool tab_open = true;
    const ImGuiTabItemFlags item_flags =
        (active_tab_ == i) ? ImGuiTabItemFlags_SetSelected : 0;

    const std::string tab_label =
        op.display_name + (op.panel->IsDirty() ? " *" : "") + "##res" +
        std::to_string(i);

    if (ImGui::BeginTabItem(tab_label.c_str(), &tab_open, item_flags)) {
      if (ImGui::IsItemActivated()) active_tab_ = i;
      op.panel->Draw();
      ImGui::EndTabItem();
    } else if (ImGui::IsItemActivated()) {
      active_tab_ = i;
    }

    if (!tab_open) {
      if (op.panel->IsDirty()) {
        pending_close_idx_ = i;
        open_dirty_modal_  = true;
        // Don't erase yet; the modal will decide.
        ++i;
      } else {
        open_panels_.erase(open_panels_.begin() + i);
        if (active_tab_ >= i) active_tab_ = std::max(0, active_tab_ - 1);
        if (open_panels_.empty()) active_tab_ = -1;
        // Don't increment i; erased this slot.
      }
      continue;
    }

    ++i;
  }

  ImGui::EndTabBar();
}

void ResourceBrowser::RenderDirtyCloseModal() {
  if (!ImGui::BeginPopupModal("Unsaved Changes##res_browser", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }

  ImGui::TextUnformatted("You have unsaved changes. What would you like to do?");
  ImGui::Spacing();

  if (ImGui::Button("Save")) {
    if (pending_close_idx_ >= 0 &&
        pending_close_idx_ < static_cast<int>(open_panels_.size())) {
      open_panels_[pending_close_idx_].panel->Save();
      open_panels_.erase(open_panels_.begin() + pending_close_idx_);
      if (active_tab_ >= pending_close_idx_)
        active_tab_ = std::max(0, active_tab_ - 1);
      if (open_panels_.empty()) active_tab_ = -1;
    }
    pending_close_idx_ = -1;
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (ImGui::Button("Discard")) {
    if (pending_close_idx_ >= 0 &&
        pending_close_idx_ < static_cast<int>(open_panels_.size())) {
      open_panels_.erase(open_panels_.begin() + pending_close_idx_);
      if (active_tab_ >= pending_close_idx_)
        active_tab_ = std::max(0, active_tab_ - 1);
      if (open_panels_.empty()) active_tab_ = -1;
    }
    pending_close_idx_ = -1;
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (ImGui::Button("Cancel")) {
    pending_close_idx_ = -1;
    ImGui::CloseCurrentPopup();
  }

  ImGui::EndPopup();
}

void ResourceBrowser::RenderNewFileModal() {
  if (!ImGui::BeginPopupModal("New Resource##res_browser", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }

  ImGui::Text("Create new %s",
              pending_new_ext_.size() > 1
                  ? pending_new_ext_.substr(1).c_str()
                  : pending_new_ext_.c_str());
  ImGui::Spacing();
  ImGui::TextUnformatted("Name:");
  ImGui::SameLine();

  // Pressing Enter confirms creation.
  const bool enter_pressed =
      ImGui::InputText("##new_name", new_name_buf_, sizeof(new_name_buf_),
                       ImGuiInputTextFlags_EnterReturnsTrue);
  ImGui::SetItemDefaultFocus();

  if (!new_error_msg_.empty()) {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "%s",
                       new_error_msg_.c_str());
  }

  ImGui::Spacing();

  const bool name_valid = new_name_buf_[0] != '\0';
  if (!name_valid) ImGui::BeginDisabled();
  const bool confirmed = ImGui::Button("Create") || (enter_pressed && name_valid);
  if (!name_valid) ImGui::EndDisabled();

  if (confirmed) {
    const std::filesystem::path created =
        registry_->Create(pending_new_ext_, new_name_buf_);
    if (created.empty()) {
      new_error_msg_ = "Failed to create file. Check the name and try again.";
    } else {
      Scan();
      ImGui::CloseCurrentPopup();
      OpenOrFocus(created);
    }
  }

  ImGui::SameLine();
  if (ImGui::Button("Cancel"))
    ImGui::CloseCurrentPopup();

  ImGui::EndPopup();
}

}  // namespace editor
