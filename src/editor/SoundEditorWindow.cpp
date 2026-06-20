#include "editor/SoundEditorWindow.h"

#include <filesystem>
#include <fstream>
#include <string>

#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <loguru.hpp>
#include <nfd.h>
#include <yaml-cpp/yaml.h>

#include "audio/RolloffType.h"
#include "core/Config.h"

namespace editor {

namespace {

constexpr const char* kSoundFilter   = "sound.yaml";
constexpr const char* kAudioFilter   = "wav,mp3,flac,ogg,opus";
constexpr const char* kRolloffNames[] = {"Linear", "Inverse", "Exponential"};

audio::RolloffType ParseRolloff(const std::string& s) {
  if (s == "linear")      return audio::RolloffType::kLinear;
  if (s == "exponential") return audio::RolloffType::kExponential;
  return audio::RolloffType::kInverse;
}

const char* RolloffToString(audio::RolloffType r) {
  switch (r) {
    case audio::RolloffType::kLinear:      return "linear";
    case audio::RolloffType::kExponential: return "exponential";
    default:                               return "inverse";
  }
}

int RolloffToIndex(audio::RolloffType r) {
  switch (r) {
    case audio::RolloffType::kLinear:      return 0;
    case audio::RolloffType::kExponential: return 2;
    default:                               return 1;
  }
}

audio::RolloffType IndexToRolloff(int i) {
  switch (i) {
    case 0: return audio::RolloffType::kLinear;
    case 2: return audio::RolloffType::kExponential;
    default: return audio::RolloffType::kInverse;
  }
}

}  // namespace

void SoundEditorWindow::Open() {
  open_ = true;
}

void SoundEditorWindow::OpenTemplate(const std::string& name) {
  const auto path =
      core::Config::GetDataFolder() / "sounds" / (name + ".sound.yaml");
  if (LoadFromPath(path)) {
    current_path_ = path;
    unsaved_      = false;
    open_         = true;
  } else {
    LOG_F(ERROR, "Sound editor: failed to load '%s'", path.string().c_str());
  }
}

void SoundEditorWindow::Render() {
  if (!open_) return;

  ImGui::SetNextWindowSize({480.f, 300.f}, ImGuiCond_FirstUseEver);
  const std::string title = std::string(ICON_FA_MUSIC) + " Sound Editor";
  if (!ImGui::Begin(title.c_str(), &open_)) {
    ImGui::End();
    return;
  }

  RenderToolbar();
  ImGui::Separator();
  RenderProperties();

  ImGui::End();
}

void SoundEditorWindow::RenderToolbar() {
  if (ImGui::Button("New"))      NewFile();
  ImGui::SameLine();
  if (ImGui::Button("Load"))     LoadFromFile();
  ImGui::SameLine();
  if (ImGui::Button("Save"))     Save();
  ImGui::SameLine();
  if (ImGui::Button("Save As"))  SaveAs();

  ImGui::SameLine();
  const std::string label = current_path_.empty()
      ? "untitled"
      : current_path_.filename().string();
  ImGui::TextDisabled("%s%s", label.c_str(), unsaved_ ? " *" : "");
}

void SoundEditorWindow::RenderProperties() {
  // Audio file — button showing the current path; click opens a file dialog.
  {
    constexpr float kLabelW = 160.f;
    const float btn_w = ImGui::GetContentRegionAvail().x - kLabelW
                        - ImGui::GetStyle().ItemSpacing.x;
    const std::string disp =
        desc_.file.empty() ? "(none)" : desc_.file;
    if (ImGui::Button(disp.c_str(), ImVec2(btn_w, 0.f))) {
      const auto data_dir = core::Config::GetDataFolder();
      nfdu8char_t* path   = nullptr;
      const nfdu8filteritem_t filter = {"Audio files", kAudioFilter};
      const nfdresult_t res =
          NFD_OpenDialogU8(&path, &filter, 1, data_dir.string().c_str());
      if (res == NFD_OKAY) {
        desc_.file = std::filesystem::relative(
            std::filesystem::path(path), data_dir).generic_string();
        NFD_FreePathU8(path);
        unsaved_ = true;
      } else if (res == NFD_ERROR) {
        LOG_F(ERROR, "Sound editor: NFD error opening audio file dialog");
      }
    }
    ImGui::SetItemTooltip(
        "Path to the audio file, relative to the data folder.\n"
        "Supported formats: .wav, .mp3, .flac, .ogg, .opus");
    ImGui::SameLine();
    ImGui::TextUnformatted("Audio file");
  }

  ImGui::PushItemWidth(-160.f);

  if (ImGui::Checkbox("Loop", &desc_.loop))         unsaved_ = true;
  ImGui::SetItemTooltip("Loop the sound indefinitely when played.");

  if (ImGui::InputInt("Priority", &desc_.priority)) unsaved_ = true;
  ImGui::SetItemTooltip(
      "Scheduling priority. Higher values are preferred when audio\n"
      "sources are scarce and the backend must drop sounds.");

  // Rolloff combo.
  int rolloff_idx = RolloffToIndex(desc_.rolloff);
  if (ImGui::Combo("Rolloff", &rolloff_idx, kRolloffNames, 3)) {
    desc_.rolloff = IndexToRolloff(rolloff_idx);
    unsaved_      = true;
  }
  ImGui::SetItemTooltip("Distance-attenuation model.");

  if (ImGui::DragFloat("Min distance", &desc_.min_distance,
                        0.1f, 0.01f, desc_.max_distance, "%.2f")) {
    unsaved_ = true;
  }
  ImGui::SetItemTooltip(
      "Reference distance at which gain equals the volume value.");

  if (ImGui::DragFloat("Max distance", &desc_.max_distance,
                        1.f, desc_.min_distance, 10000.f, "%.2f")) {
    unsaved_ = true;
  }
  ImGui::SetItemTooltip("Distance at which gain reaches its minimum.");

  if (ImGui::SliderFloat("Volume", &desc_.volume, 0.f, 1.f, "%.2f"))
    unsaved_ = true;
  ImGui::SetItemTooltip("Default playback gain [0, 1]. 1.0 = full volume.");

  if (ImGui::SliderFloat("Pitch", &desc_.pitch, 0.1f, 4.f, "%.2f"))
    unsaved_ = true;
  ImGui::SetItemTooltip(
      "Default pitch multiplier. 1.0 = original pitch.");

  ImGui::PopItemWidth();
}

void SoundEditorWindow::NewFile() {
  desc_ = audio::SoundDesc{};
  current_path_.clear();
  unsaved_ = false;
}

void SoundEditorWindow::LoadFromFile() {
  const auto sounds_dir = core::Config::GetDataFolder() / "sounds";
  nfdu8char_t* out_path = nullptr;
  const nfdu8filteritem_t filter = {"Sound template", kSoundFilter};
  const nfdresult_t result =
      NFD_OpenDialogU8(&out_path, &filter, 1,
                       sounds_dir.string().c_str());
  if (result == NFD_OKAY) {
    const std::filesystem::path path(out_path);
    NFD_FreePathU8(out_path);
    if (LoadFromPath(path)) {
      current_path_ = path;
      unsaved_      = false;
    }
  } else if (result == NFD_ERROR) {
    LOG_F(ERROR, "Sound editor: NFD error opening load dialog");
  }
}

void SoundEditorWindow::Save() {
  if (current_path_.empty()) {
    SaveAs();
    return;
  }
  if (SerializeToFile(current_path_)) {
    unsaved_ = false;
    LOG_F(INFO, "Sound editor: saved '%s'", current_path_.string().c_str());
  }
}

void SoundEditorWindow::SaveAs() {
  const auto sounds_dir = core::Config::GetDataFolder() / "sounds";
  nfdu8char_t* out_path = nullptr;
  const nfdu8filteritem_t filter = {"Sound template", kSoundFilter};

  const std::string default_name = current_path_.empty()
      ? "new_sound.sound.yaml"
      : current_path_.filename().string();

  const nfdresult_t result =
      NFD_SaveDialogU8(&out_path, &filter, 1,
                       sounds_dir.string().c_str(),
                       default_name.c_str());
  if (result != NFD_OKAY) {
    if (result == NFD_ERROR)
      LOG_F(ERROR, "Sound editor: NFD error opening save dialog");
    return;
  }

  std::filesystem::path path(out_path);
  NFD_FreePathU8(out_path);

  // Ensure the file has the correct double extension.
  if (path.extension() != ".yaml" || path.stem().extension() != ".sound")
    path += ".sound.yaml";

  if (SerializeToFile(path)) {
    current_path_ = path;
    unsaved_      = false;
    LOG_F(INFO, "Sound editor: saved '%s'", path.string().c_str());
  }
}

bool SoundEditorWindow::SerializeToFile(const std::filesystem::path& path) {
  YAML::Node node;
  node["file"]         = desc_.file;
  node["loop"]         = desc_.loop;
  node["priority"]     = desc_.priority;
  node["rolloff"]      = std::string(RolloffToString(desc_.rolloff));
  node["min_distance"] = desc_.min_distance;
  node["max_distance"] = desc_.max_distance;
  node["volume"]       = desc_.volume;
  node["pitch"]        = desc_.pitch;

  std::ofstream out(path);
  if (!out) {
    LOG_F(ERROR, "Sound editor: cannot open '%s' for writing",
          path.string().c_str());
    return false;
  }
  out << node;
  return true;
}

bool SoundEditorWindow::LoadFromPath(const std::filesystem::path& path) {
  try {
    const YAML::Node root = YAML::LoadFile(path.string());
    audio::SoundDesc d;
    if (root["file"])         d.file         = root["file"].as<std::string>();
    if (root["loop"])         d.loop         = root["loop"].as<bool>();
    if (root["priority"])     d.priority     = root["priority"].as<int>();
    if (root["rolloff"])      d.rolloff      = ParseRolloff(root["rolloff"].as<std::string>());
    if (root["min_distance"]) d.min_distance = root["min_distance"].as<float>();
    if (root["max_distance"]) d.max_distance = root["max_distance"].as<float>();
    if (root["volume"])       d.volume       = root["volume"].as<float>();
    if (root["pitch"])        d.pitch        = root["pitch"].as<float>();
    desc_ = d;
    return true;
  } catch (const std::exception& e) {
    LOG_F(ERROR, "Sound editor: YAML load error from '%s': %s",
          path.string().c_str(), e.what());
    return false;
  }
}

}  // namespace editor
