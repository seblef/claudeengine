#include "editor/EditorWindow.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include <imgui.h>
#include <ImGuizmo.h>
#include <loguru.hpp>
#include <nfd.h>
#include <yaml-cpp/yaml.h>

#include "abstract/VideoDevice.h"
#include "abstract/ISoundSystem.h"
#include "core/AppConfig.h"
#include "core/BBox3.h"
#include "core/Config.h"
#include "core/Event.h"
#include "core/Vec3f.h"
#include "core/YamlSerialiser.h"
#include "editor/EditorScene.h"
#include "editor/EditorSystem.h"
#include "editor/tools/SelectionTool.h"
#include "editor/tools/TerrainSculptTool.h"
#include "editor/EditorToolbar.h"
#include "editor/commands/GroupUnderPivotCommand.h"
#include "editor/commands/PlaceObjectCommand.h"
#include "game/GameGauge.h"
#include "editor/commands/TransformCommand.h"
#include "editor/commands/UngroupPivotCommand.h"
#include "editor/EditorViewport.h"
#include "editor/LogPanel.h"
#include "editor/ProfilerPanel.h"
#include "editor/MapPropertiesWindow.h"
#include "editor/MapSerializer.h"
#include "editor/MaterialEditorWindow.h"
#include "editor/MeshEditorWindow.h"
#include "editor/ParticleEditorWindow.h"
#include "editor/SoundEditorWindow.h"
#include "editor/MeshSelectionModal.h"
#include "editor/ParticleSystemSelectionModal.h"
#include "editor/SoundEmitterSelectionModal.h"
#include "editor/VehicleSelectionModal.h"
#include "editor/ObjectNamingUtils.h"
#include "editor/ObjectsPanel.h"
#include "editor/EmeshInfoPanel.h"
#include "editor/ResourceBrowser.h"
#include "editor/ResourcePanelRegistry.h"
#include "editor/VehicleEditorWindow.h"
#include "editor/OutlinerPanel.h"
#include "editor/PropertiesPanel.h"
#include "editor/ResourcesPanel.h"
#include "game/GameLight.h"
#include "game/GameLightDesc.h"
#include "game/GameMaterial.h"
#include "game/GameMesh.h"
#include "game/GameObjectType.h"
#include "game/GameParticleSystem.h"
#include "game/GamePivot.h"
#include "game/GamePlayerStart.h"
#include "game/GameSoundEmitter.h"
#include "game/GameTerrain.h"
#include "game/GameVehicle.h"
#include "game/MeshTemplate.h"
#include "game/VehicleTemplate.h"
#include "particles/ParticleSystemTemplate.h"
#include "renderer/Light.h"
#include "renderer/MaterialDesc.h"
#include "renderer/PostProcessInfos.h"
#include "renderer/Renderer.h"
#include "terrain/TerrainData.h"
#include "terrain/TerrainMaterial.h"
#include "terrain/TerrainRenderer.h"

namespace editor {

namespace {

// Maps light-creation tools to their renderer::LightType.
std::optional<renderer::LightType> ToolToLightType(EditorTool tool) {
  switch (tool) {
    case EditorTool::kCreateOmniLight:  return renderer::LightType::kOmni;
    case EditorTool::kCreateCircleSpot: return renderer::LightType::kCircleSpot;
    case EditorTool::kCreateRectSpot:   return renderer::LightType::kRectSpot;
    default:                            return std::nullopt;
  }
}

// Returns the base name used to generate a unique object name for a light type.
const char* LightBaseName(renderer::LightType type) {
  switch (type) {
    case renderer::LightType::kOmni:       return "omni";
    case renderer::LightType::kCircleSpot: return "cspot";
    case renderer::LightType::kRectSpot:   return "rspot";
    case renderer::LightType::kGlobal:     return "global";
  }
  return "light";
}

constexpr const char* kMapFilter = "map.yaml";

}  // namespace

EditorWindow::EditorWindow(abstract::VideoDevice* video)
    : video_(video),
      scene_(std::make_unique<EditorScene>(video)),
      map_properties_(std::make_unique<MapPropertiesWindow>(scene_.get())),
      toolbar_(std::make_unique<EditorToolbar>()),
      viewport_(std::make_unique<EditorViewport>(video)),
      selection_tool_(std::make_unique<SelectionTool>()),
      translate_tool_(std::make_unique<TransformTool>(ImGuizmo::TRANSLATE)),
      rotate_tool_(std::make_unique<TransformTool>(ImGuizmo::ROTATE)),
      scale_tool_(std::make_unique<TransformTool>(ImGuizmo::SCALE)),
      material_editor_(std::make_unique<MaterialEditorWindow>(video)),
      mesh_editor_(std::make_unique<MeshEditorWindow>(video,
                                                       material_editor_.get())),
      particle_editor_(std::make_unique<ParticleEditorWindow>(video)),
      sound_editor_(std::make_unique<SoundEditorWindow>()),
      vehicle_editor_(std::make_unique<VehicleEditorWindow>(video)),
      mesh_modal_(std::make_unique<MeshSelectionModal>()),
      particle_modal_(std::make_unique<ParticleSystemSelectionModal>()),
      sound_modal_(std::make_unique<SoundEmitterSelectionModal>()),
      vehicle_modal_(std::make_unique<VehicleSelectionModal>()),
      play_vehicle_modal_(std::make_unique<VehicleSelectionModal>("Select Play Vehicle")),
      properties_panel_(std::make_unique<PropertiesPanel>()),
      resources_panel_(std::make_unique<ResourcesPanel>()),
      objects_panel_(std::make_unique<ObjectsPanel>()),
      outliner_panel_(std::make_unique<OutlinerPanel>()),
      log_panel_(std::make_unique<LogPanel>()),
      profiler_panel_(std::make_unique<ProfilerPanel>()),
      road_tool_(std::make_unique<RoadTool>()),
      align_tool_(std::make_unique<AlignTool>()) {
  play_mode_ = std::make_unique<PlayModeManager>(
      scene_.get(), toolbar_.get(), viewport_.get(), video_);
  play_mode_->SetOnStatusMessage([this](std::string msg) {
    play_status_msg_   = std::move(msg);
    play_status_timer_ = 5.f;
  });
  play_mode_->SetOnExit(
      [this](std::filesystem::path path,
             const EditorCameraController::CameraState& cam) {
        // Destroy the old scene FIRST so GameTerrain::OnRemovedFromScene()
        // shuts down FoliageRenderer before the new EditorScene is created.
        scene_.reset();
        auto result = MapSerializer::Load(path, video_);
        if (!result) {
          LOG_F(ERROR, "Play mode exit: failed to reload map '%s'",
                path.c_str());
          return;
        }
        scene_ = std::move(result->scene);
        viewport_->SetScene(scene_.get());
        viewport_->SetCameraState(cam);
        map_properties_ = std::make_unique<MapPropertiesWindow>(scene_.get());
        history_.Clear();
        WireTerrainPanel();
        environment_panel_.SetContext(scene_.get(), video_);
        scene_dirty_ = false;
        LOG_F(INFO, "Scene reloaded after play mode exit");
      });

  toolbar_->SetCommandHistory(&history_);
  toolbar_->SetOnPlay([this]{
    play_vehicle_modal_->Open();
  });
  toolbar_->SetOnStop([this]{
    play_mode_->Exit();
    show_profiler_panel_ = false;
  });
  toolbar_->SetOnSave([this]{ SaveCurrent(); });
  toolbar_->SetOnSoundToggle([this](bool enabled) {
    if (enabled) EnableSceneSound();
    else         DisableSceneSound();
  });
  toolbar_->SetOnCopy([this]{ CopySelectedObject(); });
  toolbar_->SetOnPaste([this]{ PasteObject(); });
  toolbar_->SetOnFallToTerrain([this]{ FallToTerrain(); });
  toolbar_->SetOnCenterOnObject([this]{ CenterCameraOnObject(); });
  toolbar_->SetOnGroupObjects([this]{ GroupUnderPivot(); });
  toolbar_->SetOnUngroupObjects([this]{ UngroupSelectedPivot(); });
  toolbar_->SetOnPlaceGauge([this]{ PlaceGauge(); });
  viewport_->SetScene(scene_.get());
  viewport_->SetCommandHistory(&history_);
  viewport_->SetActiveTool(selection_tool_.get());
  viewport_->SetRenderingSettingsPanel(&rendering_settings_panel_);
  properties_panel_->SetCommandHistory(&history_);
  properties_panel_->SetVideoDevice(video_);
  material_editor_->SetCommandHistory(&history_);
  objects_panel_->SetCommandHistory(&history_);
  outliner_panel_->SetCommandHistory(&history_);
  history_.SetOnDirty([this]{ scene_dirty_ = true; });
  road_tool_->SetOnDirty([this]{ scene_dirty_ = true; });
  align_tool_->SetOnDone([this]{
    EditorToolBase* prev = align_prev_base_tool_;
    align_prev_base_tool_ = nullptr;
    viewport_->SetActiveTool(prev ? prev : selection_tool_.get());
  });
  toolbar_->SetOnAlign([this]{ ActivateAlignTool(); });
  properties_panel_->SetOnRoadWidthChanged([this](game::GameRoad* road) {
    const terrain::TerrainData* td = viewport_->GetTerrainData();
    if (td) {
      road->RegenerateMesh([td](float x, float z) { return td->GetHeight(x, z); });
    } else {
      road->RegenerateMesh(nullptr);
    }
    scene_dirty_ = true;
  });
  resources_panel_->SetOnMaterialOpen(
      [this](game::GameMaterial* mat) { material_editor_->Open(mat); });
  resources_panel_->SetOnImportMaterial([this] { ImportMaterial(); });
  resources_panel_->SetOnImportMesh([this] { ImportMesh(); });
  resources_panel_->SetOnMeshEdit([this](const game::MeshTemplate* tmpl) {
    if (tmpl) mesh_editor_->OpenEdit(tmpl->GetId());
  });
  resources_panel_->SetOnParticleOpen([this](const std::string& name) {
    show_particle_editor_ = true;
    particle_editor_->OpenTemplate(name);
  });
  resources_panel_->SetOnSoundOpen([this](const std::string& name) {
    show_sound_editor_ = true;
    sound_editor_->OpenTemplate(name);
  });
  resources_panel_->SetOnNewSound([this](std::string_view name) {
    const auto path = core::Config::GetDataFolder() / "sounds" /
                      (std::string(name) + ".sound.yaml");
    // Write a default .sound.yaml so the file exists before opening the editor.
    if (!std::filesystem::exists(path)) {
      std::ofstream out(path);
      out << "file: sounds/" << name << ".wav\n"
          << "loop: false\npriority: 0\nrolloff: inverse\n"
          << "min_distance: 1.0\nmax_distance: 100.0\n"
          << "volume: 1.0\npitch: 1.0\n";
    }
    show_sound_editor_ = true;
    sound_editor_->OpenTemplate(std::string(name));
    LOG_F(INFO, "Created sound template '%.*s'",
          static_cast<int>(name.size()), name.data());
  });
  properties_panel_->SetOnOpenParticleEditor([this](const std::string& name) {
    show_particle_editor_ = true;
    particle_editor_->OpenTemplate(name);
  });
  properties_panel_->SetOnPlaySoundOnce(
      [this](const std::string& sound_name, float volume_scale) {
        sound_preview_.Play(sound_name, volume_scale);
      });
  particle_editor_->SetOnTemplateSaved([this](const std::string& name) {
    particles::ParticleSystemTemplate* tmpl =
        particles::ParticleSystemTemplate::Get(name);
    if (!tmpl) return;
    tmpl->Reload();
    for (game::GameObject* obj : scene_->GetObjects()) {
      if (obj->GetType() != game::GameObjectType::kParticleSystem) continue;
      auto* ps = static_cast<game::GameParticleSystem*>(obj);
      if (ps->GetTemplate() == tmpl) ps->ReloadFromTemplate();
    }
  });
  properties_panel_->SetOnTransformChanged([this](game::GameObject* obj) {
    viewport_->UpdateMovedObject(obj);
  });
  resources_panel_->SetOnNewMaterial([this](std::string_view name) {
    auto* mat = new game::GameMaterial(std::string(name),
                                       renderer::MaterialDesc(), video_);
    scene_->AddGameMaterial(mat);
    material_editor_->Open(mat);
  });
  environment_panel_.SetContext(scene_.get(), video_);
  terrain_panel_.SetOnCreateFromImport(
      [this](std::vector<uint16_t> s, int w, int h, float mn, float mx) {
        CreateTerrainFromImport(std::move(s), w, h, mn, mx);
      });

  // Initialise the editor 3D audio (sound-enable toggle).
  // Reuse SoundPreviewPlayer's ISoundSystem: OpenAL's alcMakeContextCurrent is
  // global per-thread, so two concurrent contexts cause sources from one to be
  // silently ignored when the other's context is current.
  if (abstract::ISoundSystem* sys = sound_preview_.GetSoundSystem()) {
    editor_sound_manager_   = std::make_unique<audio::SoundManager>(sys);
    editor_sound_resources_ = std::make_unique<audio::ResourceManager>(sys);
    LOG_F(INFO, "Editor 3D audio ready (shared OpenAL context)");
  } else {
    LOG_F(WARNING, "Editor 3D audio unavailable — sound toggle will be a no-op");
  }

  // .emesh files open in a read-only info panel tab (no "New" button).
  resource_panel_registry_.Register(
      ".emesh",
      [](std::filesystem::path path) -> std::unique_ptr<IResourcePanel> {
        return std::make_unique<EmeshInfoPanel>(std::move(path));
      });

  // Wire vehicle editor: open in dedicated window, "New" button creates skeleton.
  vehicle_editor_->SetOnPlaceInScene([this](const std::filesystem::path& vpath) {
    const std::filesystem::path data_dir = core::Config::GetDataFolder();
    const std::string desc_rel =
        std::filesystem::relative(vpath, data_dir).string();
    game::VehicleTemplate* tmpl =
        game::VehicleTemplate::GetOrLoad(desc_rel, video_);
    if (!tmpl) {
      LOG_F(WARNING, "VehicleEditorWindow: failed to load template '%s'",
            desc_rel.c_str());
      return;
    }
    auto vehicle = std::make_unique<game::GameVehicle>(tmpl);
    tmpl->Release();
    vehicle->SetName(GenerateObjectName(*scene_, "vehicle"));
    const EditorCameraController::CameraState cam = viewport_->GetCameraState();
    vehicle->SetWorldTransform(core::Mat4f::Translation(cam.focus));
    history_.Push(
        std::make_unique<PlaceObjectCommand>(scene_.get(), std::move(vehicle)));
  });
  resource_panel_registry_.RegisterOpenCallback(
      ".vehicle.yaml",
      [this](const std::filesystem::path& path) {
        vehicle_editor_->Open(path);
      });
  resource_panel_registry_.RegisterNew(
      ".vehicle.yaml",
      [](std::string_view name) -> std::filesystem::path {
        const std::filesystem::path dir =
            core::Config::GetDataFolder() / "vehicles";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        const std::filesystem::path path =
            dir / (std::string(name) + ".vehicle.yaml");
        if (std::filesystem::exists(path)) return path;
        std::ofstream out(path);
        if (!out) return {};
        out << "body_mesh: \"\"\n"
               "front_wheel_mesh: \"\"\n"
               "rear_wheel_mesh: \"\"\n"
               "physics:\n"
               "  mass: 1200\n"
               "  max_engine_torque: 300\n"
               "  max_steer_angle: 0.5\n"
               "  brake_torque: 1500\n"
               "  handbrake_torque: 3000\n"
               "  com_offset: [0, -0.3, 0]\n"
               "  suspension:\n"
               "    front: {min_length: 0.1, max_length: 0.5, stiffness: 1500, damping: 200}\n"
               "    rear: {min_length: 0.1, max_length: 0.5, stiffness: 1500, damping: 200}\n"
               "wheels:\n"
               "  front_left: {position: [-0.85, 0, 1.4]}\n"
               "  front_right: {position: [0.85, 0, 1.4]}\n"
               "  rear_left: {position: [-0.85, 0, -1.4]}\n"
               "  rear_right: {position: [0.85, 0, -1.4]}\n";
        LOG_F(INFO, "EditorWindow: created vehicle '%s'", path.string().c_str());
        return path;
      });

  // ResourceBrowser is constructed after the registry is populated with all
  // known resource types.
  resource_browser_ = std::make_unique<ResourceBrowser>(&resource_panel_registry_);

  loguru::add_callback("editor_log", &LogPanel::LogCallback,
                       log_panel_.get(), loguru::Verbosity_INFO);

  const auto config_path = core::Config::GetDataFolder() / "config.yaml";
  YAML::Node root = core::LoadYamlFile(config_path);
  if (auto editor = root["editor"]) {
    if (auto interval = editor["autosave_interval_seconds"])
      autosave_interval_ = interval.as<float>();
    if (auto recent = editor["recent_maps"]) {
      std::transform(recent.begin(), recent.end(),
                     std::back_inserter(recent_maps_),
                     [](const YAML::Node& n) { return n.as<std::string>(); });
    }
    if (auto pd = editor["physics_debug"]) {
      if (auto v = pd["shapes"])
        rendering_settings_panel_.SetPhysicsDebugShapesEnabled(v.as<bool>());
      if (auto v = pd["constraints"])
        rendering_settings_panel_.SetPhysicsDebugConstraintsEnabled(v.as<bool>());
      if (auto v = pd["contact_points"])
        rendering_settings_panel_.SetPhysicsDebugContactPointsEnabled(v.as<bool>());
      if (auto v = pd["broadphase"])
        rendering_settings_panel_.SetPhysicsDebugBroadPhaseEnabled(v.as<bool>());
      if (auto v = pd["body_mode"]) {
        if (v.as<std::string>() == "all_bodies")
          rendering_settings_panel_.SetPhysicsDebugBodyMode(
              RenderingSettingsPanel::PhysicsDebugBodyMode::kAllBodies);
      }
    }
    if (auto ov = editor["overlay"]) {
      if (auto v = ov["lights"])
        rendering_settings_panel_.SetOverlayLightsEnabled(v.as<bool>());
      if (auto v = ov["sounds"])
        rendering_settings_panel_.SetOverlaySoundsEnabled(v.as<bool>());
      if (auto v = ov["particles"])
        rendering_settings_panel_.SetOverlayParticlesEnabled(v.as<bool>());
      if (auto v = ov["player_starts"])
        rendering_settings_panel_.SetOverlayPlayerStartsEnabled(v.as<bool>());
    }
  }
}

EditorWindow::~EditorWindow() {
  // Silence and detach all emitters before the scene is destroyed so that
  // GameSoundEmitter::OnRemovedFromScene() does not use dangling manager pointers.
  DisableSceneSound();
  SavePhysicsDebugSettings();
  SaveOverlaySettings();
  loguru::remove_callback("editor_log");
}

void EditorWindow::OnEvent(const core::Event& event) {
  if (play_mode_ && play_mode_->IsPlaying()) {
    play_mode_->OnEvent(event);
  } else {
    viewport_->OnEvent(event);
  }
}

void EditorWindow::Render() {
  TickAutosave();
  sound_preview_.Update();

  const float dt = ImGui::GetIO().DeltaTime;

  if (toolbar_->IsSoundEnabled() && editor_sound_manager_) {
    editor_sound_manager_->SetListenerTransform(
        viewport_->GetCameraWorldTransform());
    editor_sound_manager_->Update(dt);
  }

  environment_panel_.Tick(dt);
  scene_->Update(static_cast<float>(ImGui::GetTime()), dt);

  // Advance play mode physics and FPS camera before the viewport renders so the
  // renderer sees the up-to-date camera transform for this frame.
  if (play_mode_->IsPlaying())
    play_mode_->Tick(dt);

  // Decay play mode status bar message timer.
  if (play_status_timer_ > 0.f) play_status_timer_ -= dt;

  // 1. Full-screen DockSpace — all panels dock into it.
  ImGui::DockSpaceOverViewport();

  // 2. Main menu bar.
  RenderMenuBar();

  // 3. Toolbar — update dirty state and copy/paste availability before rendering.
  toolbar_->SetDirty(scene_dirty_);
  toolbar_->SetPlayEnabled(!scene_->GetFilePath().empty() &&
                           !play_mode_->IsPlaying());
  {
    const auto& sel = scene_->GetSelection();
    const bool can_copy = std::any_of(sel.begin(), sel.end(),
        [](const game::GameObject* o) {
          return o->GetType() == game::GameObjectType::kMesh  ||
                 o->GetType() == game::GameObjectType::kLight ||
                 o->GetType() == game::GameObjectType::kPivot ||
                 o->GetType() == game::GameObjectType::kSoundEmitter;
        });
    toolbar_->SetCanCopy(can_copy);
    toolbar_->SetCanPaste(!clipboard_.empty());
    const bool can_fall = terrain_panel_.IsActive() &&
        !sel.empty() &&
        std::none_of(sel.begin(), sel.end(), [](const game::GameObject* o) {
          return o->GetType() == game::GameObjectType::kTerrain;
        });
    toolbar_->SetCanFallToTerrain(can_fall);
    const bool can_center = !sel.empty() &&
        std::none_of(sel.begin(), sel.end(), [](const game::GameObject* o) {
          return o->GetType() == game::GameObjectType::kTerrain;
        });
    toolbar_->SetCanCenterOnObject(can_center);

    toolbar_->SetCanAlign(!sel.empty());

    // Group: >=2 root-level objects (no parent pivot) in the selection.
    const bool can_group = sel.size() >= 2 && std::all_of(sel.begin(), sel.end(),
        [](const game::GameObject* o) { return o->GetParent() == nullptr; });
    toolbar_->SetCanGroup(can_group);
    // Ungroup: exactly one pivot with children selected.
    const bool can_ungroup = sel.size() == 1 &&
        sel[0]->GetType() == game::GameObjectType::kPivot &&
        !static_cast<const game::GamePivot*>(sel[0])->GetChildren().empty();
    toolbar_->SetCanUngroup(can_ungroup);
  }
  toolbar_->Render();
  const EditorTool active_tool = toolbar_->GetActiveTool();

  // When the active tool changes, deactivate any running placement tool and
  // activate the appropriate base tool. Skipped while tools are frozen (play
  // mode) to prevent tool transitions triggered by toolbar keyboard shortcuts.
  if (!play_mode_->AreToolsFrozen() && active_tool != prev_tool_) {
    if (placement_tool_) {
      // Deactivate placement tool before resetting it.
      viewport_->SetActiveTool(selection_tool_.get());
      placement_tool_.reset();
    }

    if (active_tool == EditorTool::kSelection)
      viewport_->SetActiveTool(selection_tool_.get());
    else if (active_tool == EditorTool::kTranslate)
      viewport_->SetActiveTool(translate_tool_.get());
    else if (active_tool == EditorTool::kRotate)
      viewport_->SetActiveTool(rotate_tool_.get());
    else if (active_tool == EditorTool::kScale)
      viewport_->SetActiveTool(scale_tool_.get());
    else if (active_tool == EditorTool::kRoad)
      viewport_->SetActiveTool(road_tool_.get());
    else if (!IsTransformTool(active_tool))
      viewport_->SetActiveTool(selection_tool_.get());

    // Detect transitions into creation tools.
    if (IsCreationTool(active_tool)) {
      if (active_tool == EditorTool::kCreateMesh) {
        mesh_modal_->Open();
      } else if (const auto light_type = ToolToLightType(active_tool)) {
        auto light = std::make_unique<game::GameLight>(*light_type);
        light->SetName(GenerateObjectName(*scene_, LightBaseName(*light_type)));
        placement_tool_ = std::make_unique<PlacementTool>(
            std::move(light), 10.f,
            ImGuiMouseCursor_ResizeAll,
            [this]{ toolbar_->SetActiveTool(EditorTool::kSelection); });
        viewport_->SetActiveTool(placement_tool_.get());
      } else if (active_tool == EditorTool::kCreatePivot) {
        auto pivot = std::make_unique<game::GamePivot>();
        pivot->SetName(GenerateObjectName(*scene_, "pivot"));
        placement_tool_ = std::make_unique<PlacementTool>(
            std::move(pivot), 0.f,
            ImGuiMouseCursor_ResizeAll,
            [this]{ toolbar_->SetActiveTool(EditorTool::kSelection); });
        viewport_->SetActiveTool(placement_tool_.get());
      } else if (active_tool == EditorTool::kCreatePlayerStart) {
        auto ps = std::make_unique<game::GamePlayerStart>();
        ps->SetName(GenerateObjectName(*scene_, "player_start"));
        placement_tool_ = std::make_unique<PlacementTool>(
            std::move(ps), 0.f,
            ImGuiMouseCursor_ResizeAll,
            [this]{ toolbar_->SetActiveTool(EditorTool::kSelection); });
        viewport_->SetActiveTool(placement_tool_.get());
      } else if (active_tool == EditorTool::kCreateParticleSystem) {
        particle_modal_->Open();
      } else if (active_tool == EditorTool::kCreateSoundEmitter) {
        sound_modal_->Open();
      } else if (active_tool == EditorTool::kCreateRoad) {
        road_tool_->OnActivateCreation();
        viewport_->SetActiveTool(road_tool_.get());
      } else if (active_tool == EditorTool::kCreateVehicle) {
        vehicle_modal_->Open();
      }
    }
  }
  prev_tool_ = active_tool;

  // Mesh selection modal — open when kCreateMesh is activated.
  if (game::MeshTemplate* tmpl = mesh_modal_->Render()) {
    auto mesh = std::make_unique<game::GameMesh>(tmpl);
    const std::string stem =
        std::filesystem::path(tmpl->GetId()).stem().string();
    mesh->SetName(GenerateObjectName(*scene_, stem));
    placement_tool_ = std::make_unique<PlacementTool>(
        std::move(mesh), 0.f,
        ImGuiMouseCursor_None,
        [this]{ toolbar_->SetActiveTool(EditorTool::kSelection); });
    viewport_->SetActiveTool(placement_tool_.get());
  }

  // Particle system selection modal — open when kCreateParticleSystem activated.
  if (const std::string ps_name = particle_modal_->Render(); !ps_name.empty()) {
    particles::ParticleSystemTemplate* tmpl =
        particles::ParticleSystemTemplate::GetOrLoad(ps_name, video_);
    if (tmpl) {
      auto ps = std::make_unique<game::GameParticleSystem>(tmpl, video_);
      tmpl->Release();  // GameParticleSystem holds the AddRef'd reference
      ps->SetName(GenerateObjectName(*scene_, ps_name));
      placement_tool_ = std::make_unique<PlacementTool>(
          std::move(ps), 0.f,
          ImGuiMouseCursor_ResizeAll,
          [this]{ toolbar_->SetActiveTool(EditorTool::kSelection); });
      viewport_->SetActiveTool(placement_tool_.get());
    }
  }

  // Sound emitter selection modal — open when kCreateSoundEmitter activated.
  if (const std::string snd_name = sound_modal_->Render(); !snd_name.empty()) {
    const bool use_audio = toolbar_->IsSoundEnabled() && editor_sound_manager_;
    auto emitter = std::make_unique<game::GameSoundEmitter>(
        snd_name,
        use_audio ? editor_sound_manager_.get()  : nullptr,
        use_audio ? editor_sound_resources_.get() : nullptr);
    emitter->SetName(GenerateObjectName(*scene_, snd_name));
    placement_tool_ = std::make_unique<PlacementTool>(
        std::move(emitter), 0.f,
        ImGuiMouseCursor_ResizeAll,
        [this]{ toolbar_->SetActiveTool(EditorTool::kSelection); });
    viewport_->SetActiveTool(placement_tool_.get());
  }

  // Play mode vehicle selection modal — open when the Play button is clicked.
  if (const std::string play_v_name = play_vehicle_modal_->Render(); !play_v_name.empty()) {
    play_mode_->Enter(play_v_name);
    toolbar_->SetInPlayMode(play_mode_->IsPlaying());
    if (play_mode_->IsPlaying()) show_profiler_panel_ = true;
  }

  // Vehicle selection modal — open when kCreateVehicle activated.
  if (const std::string v_name = vehicle_modal_->Render(); !v_name.empty()) {
    const std::string desc_rel = "vehicles/" + v_name + ".vehicle.yaml";
    game::VehicleTemplate* tmpl =
        game::VehicleTemplate::GetOrLoad(desc_rel, video_);
    if (tmpl) {
      auto vehicle = std::make_unique<game::GameVehicle>(tmpl);
      tmpl->Release();
      vehicle->SetName(GenerateObjectName(*scene_, v_name));
      placement_tool_ = std::make_unique<PlacementTool>(
          std::move(vehicle), 0.f,
          ImGuiMouseCursor_ResizeAll,
          [this]{ toolbar_->SetActiveTool(EditorTool::kSelection); });
      viewport_->SetActiveTool(placement_tool_.get());
    } else {
      LOG_F(WARNING, "EditorWindow: failed to load vehicle template '%s'",
            desc_rel.c_str());
      toolbar_->SetActiveTool(EditorTool::kSelection);
    }
  }

  // 4. Viewport panel.
  constexpr ImGuiWindowFlags kViewportFlags =
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
  if (ImGui::Begin("Viewport", nullptr, kViewportFlags)) {
    viewport_->Render();
  }
  ImGui::End();

  // 5–7. Editor side panels — hidden while in play mode so the viewport fills
  //       the screen and the user experiences the game without UI distractions.
  if (!play_mode_->ArePanelsHidden()) {
    // 5. Left panel — Resources/Objects tabs (wired in issues #175/#176).
    if (ImGui::Begin("Scene")) {
      if (ImGui::BeginTabBar("##scene_tabs")) {
        if (ImGui::BeginTabItem("Resources")) {
          resources_panel_->Render();
          ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Outliner")) {
          outliner_panel_->Render(*scene_);
          ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Browser")) {
          resource_browser_->Render();
          ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
      }
    }
    ImGui::End();

    // 6. Right panel — Properties.
    if (ImGui::Begin("Properties")) {
      properties_panel_->Render(scene_->GetSelectedObject());
    }
    ImGui::End();

    // 7. Bottom panel — Logs (wired in issue #178).
    if (ImGui::Begin("Logs")) {
      log_panel_->Render();
    }
    ImGui::End();
  }

  // 8. Material editor — floating window, shown when a material is open.
  material_editor_->Render(*scene_);

  // 8a. Mesh editor — floating window, shown when a mesh import/edit is active.
  mesh_editor_->Render(scene_.get());

  // 8b. Particle editor — floating window, toggled via the Effects menu.
  if (show_particle_editor_) {
    particle_editor_->Render(
        static_cast<float>(ImGui::GetTime()),
        ImGui::GetIO().DeltaTime);
    show_particle_editor_ = particle_editor_->IsOpen();
  }

  // 8c. Sound editor — floating window, toggled via the Audio menu.
  if (show_sound_editor_) {
    sound_editor_->Render();
    show_sound_editor_ = sound_editor_->IsOpen();
  }

  // 8d. Vehicle editor — dedicated floating window, opened from ResourceBrowser.
  vehicle_editor_->Render();

  // 9. Map properties panel — toggled via the Map menu.
  if (show_map_props_) {
    if (ImGui::Begin("Map Properties", &show_map_props_)) {
      if (map_properties_->RenderPanel())
        scene_dirty_ = true;
    }
    ImGui::End();
  }

  // 10. New Map modal — OpenPopup must be called outside any Begin/End pair.
  if (new_map_pending_) {
    ImGui::OpenPopup("New Map##modal");
    new_map_pending_ = false;
  }
  if (map_properties_->RenderModal()) {
    const std::string         new_name  = map_properties_->GetNewMapName();
    const float               new_size  = map_properties_->GetNewMapSize();
    const game::GameLightDesc new_light = map_properties_->GetNewMapLightDesc();
    scene_.reset();
    renderer::Renderer::Instance().InitVisibilitySystems(new_size);
    scene_ = std::make_unique<EditorScene>(video_, new_name, new_size,
                                           new_light);
    map_properties_ = std::make_unique<MapPropertiesWindow>(scene_.get());
    viewport_->SetScene(scene_.get());
    history_.Clear();
    WireTerrainPanel();
    environment_panel_.SetContext(scene_.get(), video_);
    scene_dirty_ = false;
    LOG_F(INFO, "New map '%s' created (size %.1f)", new_name.c_str(),
          new_size);
  }

  // 11. Terrain creation dialog.
  if (terrain_dialog_.Render())
    CreateTerrain();

  // 11a. Terrain import window — floating, opened via Terrain > Import.
  terrain_panel_.RenderImportWindow();

  // 11a''. Terrain generate window — floating, opened via Terrain > Generate.
  terrain_panel_.RenderGenerateWindow();

  // 11a'. Terrain auto-paint window — floating, opened from the Paint tab.
  terrain_panel_.RenderPainterWindow();

  // 11b. Confirm remove terrain modal — triggered by Terrain > Remove.
  if (confirm_remove_terrain_) {
    ImGui::OpenPopup("Remove Terrain?##modal");
    confirm_remove_terrain_ = false;
  }
  if (ImGui::BeginPopupModal("Remove Terrain?##modal", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted("Remove the terrain from this scene?");
    ImGui::TextWrapped("This action cannot be undone via Ctrl+Z.");
    ImGui::Spacing();
    if (ImGui::Button("Remove", {120.f, 0.f})) {
      RemoveTerrain();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", {120.f, 0.f}))
      ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }

  // 11d. Terrain editor panel — dockable, shown via Terrain menu.
  if (show_terrain_panel_) {
    if (ImGui::Begin("Terrain", &show_terrain_panel_)) {
      terrain_panel_.Render();
    }
    ImGui::End();
    // Activate sculpt tool while the panel is open and a terrain context is set.
    const bool want_sculpt = terrain_panel_.IsActive() && sculpt_tool_;
    if (want_sculpt) {
      if (!sculpt_tool_active_) {
        sculpt_tool_active_ = true;
        viewport_->SetActiveTool(sculpt_tool_.get());
      }
    } else if (sculpt_tool_active_) {
      sculpt_tool_active_ = false;
      viewport_->SetActiveTool(selection_tool_.get());
    }
  } else if (sculpt_tool_active_) {
    sculpt_tool_active_ = false;
    viewport_->SetActiveTool(selection_tool_.get());
  }

  // 11e. Auto-activate road tool when a GameRoad is selected.
  // Suppressed while RoadTool is in creation mode so that selecting the
  // newly-created road mid-flow does not prematurely switch to editing mode.
  // Extends to kCreateRoad so Escape (cancel) drives the toolbar to kSelection.
  if (!play_mode_->AreToolsFrozen() && !road_tool_->IsCreating()) {
    const game::GameObject* sel = scene_->GetSelectedObject();
    const bool road_selected =
        sel && sel->GetType() == game::GameObjectType::kRoad;
    if (road_selected && active_tool != EditorTool::kRoad) {
      toolbar_->SetActiveTool(EditorTool::kRoad);
      viewport_->SetActiveTool(road_tool_.get());
    } else if (!road_selected &&
               (active_tool == EditorTool::kRoad ||
                active_tool == EditorTool::kCreateRoad)) {
      toolbar_->SetActiveTool(EditorTool::kSelection);
      viewport_->SetActiveTool(selection_tool_.get());
    }
  }

  // 11g. Environment editor panel — dockable, shown via Map > Environment.
  if (show_environment_panel_) {
    if (ImGui::Begin("Environment##panel", &show_environment_panel_)) {
      if (environment_panel_.Render()) scene_dirty_ = true;
    }
    ImGui::End();
  }

  // 11h. Rendering settings panel — dockable, shown via View > Rendering settings.
  if (show_rendering_settings_panel_) {
    if (ImGui::Begin("Rendering settings##panel", &show_rendering_settings_panel_))
      rendering_settings_panel_.Render();
    ImGui::End();
  }

  // 11i. Profiler panel — dockable, auto-shown on Play and via View > Profiler.
  if (show_profiler_panel_ && play_mode_->IsPlaying()) {
    if (ImGui::Begin("Profiler##panel", &show_profiler_panel_))
      profiler_panel_->Render();
    ImGui::End();
  }

  // 11j. Post-process panel — dockable, shown via View > Post-process.
  if (show_post_process_panel_) {
    if (ImGui::Begin("Post-process##panel", &show_post_process_panel_)) {
      renderer::PostProcessInfos& pp =
          renderer::Renderer::Instance().GetPostProcessInfos();
      const auto& cfg = core::AppConfig::GetPostProcess();
      if (ImGui::CollapsingHeader("Post-process", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (!cfg.IsEyeAdaptationEnabled()) {
          ImGui::SliderFloat("Exposure", &pp.exposure, 0.1f, 10.f, "%.2f");
        }
        if (cfg.IsBloomEnabled()) {
          ImGui::SliderFloat("Bloom intensity",  &pp.bloom_intensity,  0.f, 2.f, "%.2f");
          ImGui::SliderFloat("Bloom threshold",  &pp.bloom_threshold,  0.f, 3.f, "%.2f");
        }
        if (cfg.IsEyeAdaptationEnabled()) {
          ImGui::SliderFloat("Adapt speed",    &pp.adapt_speed,      0.1f,  5.f,  "%.2f");
          ImGui::SliderFloat("Eye key",        &pp.eye_key,          0.01f, 1.f,  "%.3f");
          ImGui::SliderFloat("Min exposure",   &pp.eye_min_exposure, 0.01f, 1.f,  "%.3f");
          ImGui::SliderFloat("Max exposure",   &pp.eye_max_exposure, 0.1f,  10.f, "%.2f");
          if (ImGui::Button("Save##eye_adapt")) {
            const auto config_path = core::Config::GetDataFolder() / "config.yaml";
            core::AppConfig::GetMutablePostProcess().Save(
                config_path, pp.eye_key, pp.eye_min_exposure, pp.eye_max_exposure);
          }
        }
      }
    }
    ImGui::End();
  }

  // 12. Unsaved changes modal — OpenPopup is triggered by CheckDirtyThenRun().
  if (open_unsaved_changes_modal_) {
    ImGui::OpenPopup("Unsaved Changes##modal");
    open_unsaved_changes_modal_ = false;
  }
  RenderUnsavedChangesModal();

  // 13. Status bar — pinned to the bottom of the screen.
  const ImGuiViewport* vp = ImGui::GetMainViewport();
  constexpr float kStatusBarHeight = 22.0f;
  ImGui::SetNextWindowPos({vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - kStatusBarHeight});
  ImGui::SetNextWindowSize({vp->WorkSize.x, kStatusBarHeight});
  constexpr ImGuiWindowFlags kStatusBarFlags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar;
  ImGui::Begin("##statusbar", nullptr, kStatusBarFlags);
  ImGui::TextUnformatted(scene_dirty_ ? "● Unsaved changes" : "");
  if (autosave_msg_timer_ > 0.f) {
    ImGui::SameLine();
    ImGui::TextUnformatted(last_autosave_msg_.c_str());
  }
  if (play_status_timer_ > 0.f) {
    ImGui::SameLine();
    ImGui::TextUnformatted(play_status_msg_.c_str());
  }
  ImGui::End();
}

void EditorWindow::TickAutosave() {
  const float dt = ImGui::GetIO().DeltaTime;
  autosave_timer_ += dt;

  if (autosave_msg_timer_ > 0.f) autosave_msg_timer_ -= dt;

  if (!scene_dirty_) return;
  const std::string& map_name = scene_->GetMapName();
  if (map_name.empty() || map_name == "untitled") return;
  if (autosave_timer_ < autosave_interval_) return;

  autosave_timer_ = 0.f;
  const auto path = core::Config::GetDataFolder()
                    / "maps"
                    / (map_name + ".autosave.map.yaml");
  if (MapSerializer::Save(*scene_, viewport_->GetCameraState(), path)) {
    last_autosave_msg_ = "Autosaved";
    autosave_msg_timer_ = 3.f;
  } else {
    LOG_F(WARNING, "Autosave failed: %s", path.string().c_str());
  }
}

void EditorWindow::RenderMenuBar() {
  if (!ImGui::BeginMainMenuBar()) return;

  // Global keyboard shortcuts — skip when a text input widget has keyboard focus.
  if (!ImGui::GetIO().WantCaptureKeyboard) {
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)) SaveCurrent();
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_N))
      CheckDirtyThenRun([this]{ new_map_pending_ = true; });
  }

  if (ImGui::BeginMenu("File")) {
    if (ImGui::MenuItem("New", "Ctrl+N")) {
      CheckDirtyThenRun([this]{ new_map_pending_ = true; });
    }
    if (ImGui::MenuItem("Load...")) {
      CheckDirtyThenRun([this]{ LoadFromFile(); });
    }
    if (ImGui::BeginMenu("Recent", !recent_maps_.empty())) {
      for (int i = 0; i < static_cast<int>(recent_maps_.size()); ++i) {
        const std::string& p = recent_maps_[i];
        const std::string  label =
            std::filesystem::path(p).filename().string() +
            "##recent" + std::to_string(i);
        if (ImGui::MenuItem(label.c_str()))
          CheckDirtyThenRun([this, p]{ LoadMap(std::filesystem::path(p)); });
        ImGui::SetItemTooltip("%s", p.c_str());
      }
      ImGui::EndMenu();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Save", "Ctrl+S")) {
      SaveCurrent();
    }
    if (ImGui::MenuItem("Save As...")) {
      SaveAs();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Quit")) {
      EditorSystem::Instance().Stop();
    }
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Map")) {
    if (ImGui::MenuItem("Map Properties", nullptr, show_map_props_)) {
      show_map_props_ = !show_map_props_;
    }
    if (ImGui::MenuItem("Environment", nullptr, show_environment_panel_)) {
      show_environment_panel_ = !show_environment_panel_;
    }
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Terrain")) {
    const bool has_terrain = terrain_panel_.IsActive();

    if (!has_terrain) {
      if (ImGui::MenuItem("Add Terrain"))
        terrain_dialog_.Open();
    } else {
      ImGui::BeginDisabled();
      ImGui::MenuItem("Add Terrain");
      ImGui::EndDisabled();
      ImGui::SetItemTooltip("A terrain already exists in this scene");
    }

    ImGui::Separator();

    ImGui::BeginDisabled(!has_terrain);
    if (ImGui::MenuItem("Sculpt", nullptr, show_terrain_panel_, has_terrain))
      show_terrain_panel_ = !show_terrain_panel_;
    ImGui::EndDisabled();
    if (!has_terrain)
      ImGui::SetItemTooltip("Add a terrain first");

    if (ImGui::MenuItem("Import"))
      terrain_panel_.OpenImportWindow();
    if (ImGui::MenuItem("Generate..."))
      terrain_panel_.OpenGenerateWindow();

    ImGui::Separator();

    ImGui::BeginDisabled(!has_terrain);
    if (ImGui::MenuItem("Remove", nullptr, false, has_terrain))
      confirm_remove_terrain_ = true;
    ImGui::EndDisabled();
    if (!has_terrain)
      ImGui::SetItemTooltip("No terrain to remove");

    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Import")) {
    if (ImGui::MenuItem("Material")) {
      ImportMaterial();
    }
    if (ImGui::MenuItem("Mesh")) {
      ImportMesh();
    }
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Effects")) {
    if (ImGui::MenuItem("Particle Editor", nullptr,
                         show_particle_editor_)) {
      show_particle_editor_ = !show_particle_editor_;
      if (show_particle_editor_) particle_editor_->Open();
    }
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Audio")) {
    if (ImGui::MenuItem("Sound Editor", nullptr, show_sound_editor_)) {
      show_sound_editor_ = !show_sound_editor_;
      if (show_sound_editor_) sound_editor_->Open();
    }
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("View")) {
    if (ImGui::MenuItem("Rendering settings", nullptr, show_rendering_settings_panel_))
      show_rendering_settings_panel_ = !show_rendering_settings_panel_;
    if (ImGui::MenuItem("Post-process", nullptr, show_post_process_panel_))
      show_post_process_panel_ = !show_post_process_panel_;
    ImGui::Separator();
    const bool playing = play_mode_->IsPlaying();
    ImGui::BeginDisabled(!playing);
    if (ImGui::MenuItem("Profiler", nullptr, show_profiler_panel_ && playing))
      show_profiler_panel_ = !show_profiler_panel_;
    ImGui::EndDisabled();
    if (!playing)
      ImGui::SetItemTooltip("Available during play mode only");
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Debug")) {
    if (ImGui::MenuItem("Terrain Wireframe", nullptr, terrain_wireframe_debug_)) {
      terrain_wireframe_debug_ = !terrain_wireframe_debug_;
      viewport_->SetTerrainWireframeDebugEnabled(terrain_wireframe_debug_);
    }
    ImGui::EndMenu();
  }

  ImGui::EndMainMenuBar();
}

void EditorWindow::SaveCurrent() {
  if (scene_->GetFilePath().empty()) {
    SaveAs();
    return;
  }
  if (!MapSerializer::Save(*scene_, viewport_->GetCameraState(),
                           scene_->GetFilePath())) {
    LOG_F(ERROR, "Failed to save map to '%s'",
          scene_->GetFilePath().string().c_str());
    return;
  }
  scene_dirty_ = false;
  LOG_F(INFO, "Map saved to '%s'", scene_->GetFilePath().string().c_str());
}

void EditorWindow::SaveAs() {
  const std::string default_name = scene_->GetMapName() + ".map.yaml";
  const std::string maps_dir = (core::Config::GetDataFolder() / "maps").string();
  nfdu8char_t* out_path = nullptr;
  const nfdu8filteritem_t filter = {"Map", kMapFilter};
  const nfdresult_t result =
      NFD_SaveDialogU8(&out_path, &filter, 1, maps_dir.c_str(), default_name.c_str());
  if (result != NFD_OKAY) {
    if (result == NFD_ERROR)
      LOG_F(ERROR, "NFD error opening save dialog");
    return;
  }

  std::filesystem::path path(out_path);
  NFD_FreePathU8(out_path);

  if (path.extension() != ".yaml" ||
      path.stem().extension() != ".map") {
    path += ".map.yaml";
  }

  if (!MapSerializer::Save(*scene_, viewport_->GetCameraState(), path)) {
    LOG_F(ERROR, "Failed to save map to '%s'", path.string().c_str());
    return;
  }
  scene_->SetFilePath(path);
  scene_dirty_ = false;
  AddToRecentMaps(path);
  LOG_F(INFO, "Map saved to '%s'", path.string().c_str());
}

void EditorWindow::LoadFromFile() {
  const std::string maps_dir = (core::Config::GetDataFolder() / "maps").string();
  nfdu8char_t* out_path = nullptr;
  const nfdu8filteritem_t filter = {"Map", kMapFilter};
  const nfdresult_t result =
      NFD_OpenDialogU8(&out_path, &filter, 1, maps_dir.c_str());
  if (result != NFD_OKAY) {
    if (result == NFD_ERROR)
      LOG_F(ERROR, "NFD error opening load dialog");
    return;
  }

  const std::filesystem::path path(out_path);
  NFD_FreePathU8(out_path);
  LoadMap(path);
}

void EditorWindow::LoadMap(const std::filesystem::path& path) {
  // Peek the map size so the octree can be correctly sized before objects are
  // inserted.  MapLoader will re-parse the full file; this first read is cheap.
  float map_size = 120.f;
  try {
    const YAML::Node root = YAML::LoadFile(path.string());
    map_size = root["map_size"].as<float>(120.f);
  } catch (...) {}

  // Destroy the old scene so the octree is empty before rebuilding it.
  scene_.reset();
  renderer::Renderer::Instance().InitVisibilitySystems(map_size);

  auto loaded = MapSerializer::Load(path, video_);
  if (!loaded) {
    LOG_F(ERROR, "Failed to load map from '%s'", path.string().c_str());
    scene_ = std::make_unique<EditorScene>(video_);
    map_properties_ = std::make_unique<MapPropertiesWindow>(scene_.get());
    viewport_->SetScene(scene_.get());
    return;
  }

  scene_          = std::move(loaded->scene);
  map_properties_ = std::make_unique<MapPropertiesWindow>(scene_.get());
  viewport_->SetScene(scene_.get());
  viewport_->SetCameraState(loaded->camera_state);
  if (scene_->GetSelectedObject())
    viewport_->SetSelectedObject(scene_->GetSelectedObject());
  history_.Clear();
  WireTerrainPanel();
  environment_panel_.SetContext(scene_.get(), video_);
  scene_dirty_ = false;
  if (toolbar_->IsSoundEnabled()) EnableSceneSound();
  AddToRecentMaps(path);
  LOG_F(INFO, "Map loaded from '%s'", path.string().c_str());
}

void EditorWindow::AddToRecentMaps(const std::filesystem::path& path) {
  const std::string path_str = path.string();
  auto it = std::find(recent_maps_.begin(), recent_maps_.end(), path_str);
  if (it != recent_maps_.end())
    recent_maps_.erase(it);
  recent_maps_.insert(recent_maps_.begin(), path_str);
  if (static_cast<int>(recent_maps_.size()) > kMaxRecentMaps)
    recent_maps_.resize(kMaxRecentMaps);
  SaveRecentMaps();
}

void EditorWindow::SaveRecentMaps() {
  const auto config_path = core::Config::GetDataFolder() / "config.yaml";
  YAML::Node root;
  try {
    root = core::LoadYamlFile(config_path);
  } catch (...) {}

  root["editor"]["recent_maps"] = recent_maps_;

  std::ofstream out(config_path);
  out << root;
}

void EditorWindow::SavePhysicsDebugSettings() {
  const auto config_path = core::Config::GetDataFolder() / "config.yaml";
  YAML::Node root;
  try {
    root = core::LoadYamlFile(config_path);
  } catch (...) {}

  root["editor"]["physics_debug"]["shapes"] =
      rendering_settings_panel_.IsPhysicsDebugShapesEnabled();
  root["editor"]["physics_debug"]["constraints"] =
      rendering_settings_panel_.IsPhysicsDebugConstraintsEnabled();
  root["editor"]["physics_debug"]["contact_points"] =
      rendering_settings_panel_.IsPhysicsDebugContactPointsEnabled();
  root["editor"]["physics_debug"]["broadphase"] =
      rendering_settings_panel_.IsPhysicsDebugBroadPhaseEnabled();
  root["editor"]["physics_debug"]["body_mode"] =
      (rendering_settings_panel_.GetPhysicsDebugBodyMode() ==
       RenderingSettingsPanel::PhysicsDebugBodyMode::kAllBodies)
          ? "all_bodies" : "selected_only";

  std::ofstream out(config_path);
  out << root;
}

void EditorWindow::SaveOverlaySettings() {
  const auto config_path = core::Config::GetDataFolder() / "config.yaml";
  YAML::Node root;
  try {
    root = core::LoadYamlFile(config_path);
  } catch (...) {}

  root["editor"]["overlay"]["lights"] =
      rendering_settings_panel_.IsOverlayLightsEnabled();
  root["editor"]["overlay"]["sounds"] =
      rendering_settings_panel_.IsOverlaySoundsEnabled();
  root["editor"]["overlay"]["particles"] =
      rendering_settings_panel_.IsOverlayParticlesEnabled();
  root["editor"]["overlay"]["player_starts"] =
      rendering_settings_panel_.IsOverlayPlayerStartsEnabled();

  std::ofstream out(config_path);
  out << root;
}

void EditorWindow::CheckDirtyThenRun(std::function<void()> on_proceed) {
  if (scene_dirty_) {
    pending_after_save_ = std::move(on_proceed);
    open_unsaved_changes_modal_ = true;
  } else {
    on_proceed();
  }
}

void EditorWindow::RenderUnsavedChangesModal() {
  if (!ImGui::BeginPopupModal("Unsaved Changes##modal", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }

  ImGui::TextUnformatted("You have unsaved changes. What would you like to do?");
  ImGui::Spacing();

  if (ImGui::Button("Save")) {
    SaveCurrent();
    if (pending_after_save_) pending_after_save_();
    pending_after_save_ = nullptr;
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (ImGui::Button("Discard")) {
    if (pending_after_save_) pending_after_save_();
    pending_after_save_ = nullptr;
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (ImGui::Button("Cancel")) {
    pending_after_save_ = nullptr;
    ImGui::CloseCurrentPopup();
  }

  ImGui::EndPopup();
}

void EditorWindow::ImportMaterial() {
  const std::string mats_dir =
      (core::Config::GetDataFolder() / "materials").string();
  nfdu8char_t* out_path = nullptr;
  const nfdu8filteritem_t filter = {"Material", "yaml"};
  const nfdresult_t result =
      NFD_OpenDialogU8(&out_path, &filter, 1, mats_dir.c_str());
  if (result == NFD_OKAY) {
    const std::string name =
        std::filesystem::path(out_path).stem().string();
    game::GameMaterial* mat = game::GameMaterial::GetOrLoad(name, video_);
    scene_->AddGameMaterial(mat);
    LOG_F(INFO, "Imported material '%s' from '%s'", name.c_str(), out_path);
    NFD_FreePathU8(out_path);
  } else if (result == NFD_ERROR) {
    LOG_F(ERROR, "NFD error opening material dialog");
  }
}

void EditorWindow::ImportMesh() {
  const std::string meshes_dir =
      (core::Config::GetDataFolder() / "meshes").string();
  nfdu8char_t* out_path = nullptr;
  const nfdu8filteritem_t filter = {"Mesh", "obj,fbx,emesh"};
  const nfdresult_t result =
      NFD_OpenDialogU8(&out_path, &filter, 1, meshes_dir.c_str());
  if (result != NFD_OKAY) {
    if (result == NFD_ERROR)
      LOG_F(ERROR, "NFD error opening mesh dialog");
    return;
  }

  const std::filesystem::path path(out_path);
  NFD_FreePathU8(out_path);

  const std::string ext = path.extension().string();
  // Lowercase extension.
  std::string ext_lower = ext;
  std::transform(ext_lower.begin(), ext_lower.end(), ext_lower.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  if (ext_lower == ".fbx" || ext_lower == ".obj") {
    mesh_editor_->OpenImport(path);
  } else if (ext_lower == ".emesh") {
    game::MeshTemplate* tmpl =
        game::MeshTemplate::GetOrLoad(path.string(), video_);
    if (tmpl) {
      scene_->AddMeshTemplate(tmpl);
      LOG_F(INFO, "Imported emesh '%s'", path.filename().string().c_str());
    } else {
      LOG_F(ERROR, "ImportMesh: failed to load emesh '%s'",
            path.string().c_str());
    }
  } else {
    LOG_F(ERROR, "ImportMesh: unsupported extension '%s'", ext.c_str());
  }
}

namespace {
// Returns the first GameTerrain object in the scene, or nullptr.
game::GameTerrain* FindTerrain(const EditorScene& scene) {
  const auto& objs = scene.GetObjects();
  const auto it = std::find_if(objs.begin(), objs.end(), [](const game::GameObject* o) {
    return o->GetType() == game::GameObjectType::kTerrain;
  });
  return it != objs.end() ? static_cast<game::GameTerrain*>(*it) : nullptr;
}
}  // namespace


void EditorWindow::CreateTerrain() {
  const TerrainCreationDialog::Params& p = terrain_dialog_.GetParams();

  const float safe_res = std::max(0.001f, p.resolution);
  const int   width    = std::max(2, static_cast<int>(
                             std::round(p.size_x / safe_res)));
  const int   height   = std::max(2, static_cast<int>(
                             std::round(p.size_z / safe_res)));

  // 1. Allocate TerrainData — all samples at min_height (uint16 = 0).
  std::vector<uint16_t> heights(static_cast<std::size_t>(width) * height, 0u);
  auto data = std::make_unique<terrain::TerrainData>(
      heights.data(), width, height, safe_res, p.min_height, p.max_height);

  // 2. Allocate TerrainMaterial with a single default layer.
  YAML::Node mat_node;
  YAML::Node layer0;
  layer0["albedo"] = std::string("default/diffuse.dds");
  layer0["normal"] = std::string("default/normal.dds");
  layer0["tiling"] = 8.f;
  mat_node["layers"].push_back(layer0);
  auto material = std::make_unique<terrain::TerrainMaterial>();
  material->Load(mat_node, video_, width, height);

  // 3. Construct GameTerrain and add it to the scene.
  //    OnAddedToScene() calls TerrainRenderer::Init() (heightmap GPU upload)
  //    and SetMaterial() (splatmap binding).
  auto terrain_obj = std::make_unique<game::GameTerrain>(
      std::move(data), std::move(material), video_);
  terrain_obj->SetName("terrain");
  scene_->AddDynamicObject(std::move(terrain_obj));

  // 5. Wire the terrain sculpt panel.
  WireTerrainPanel();

  scene_dirty_ = true;
  LOG_F(INFO, "Terrain created: %dx%d texels, %.2f m/texel, "
        "Y=[%.1f, %.1f]", width, height, safe_res, p.min_height, p.max_height);
}

void EditorWindow::CreateTerrainFromImport(std::vector<uint16_t> samples,
                                           int w, int h,
                                           float min_h, float max_h) {
  constexpr float kDefaultResolution = 1.f;

  auto data = std::make_unique<terrain::TerrainData>(
      samples.data(), w, h, kDefaultResolution, min_h, max_h);

  YAML::Node mat_node;
  YAML::Node layer0;
  layer0["albedo"] = std::string("default/diffuse.dds");
  layer0["normal"] = std::string("default/normal.dds");
  layer0["tiling"] = 8.f;
  mat_node["layers"].push_back(layer0);
  auto material = std::make_unique<terrain::TerrainMaterial>();
  material->Load(mat_node, video_, w, h);

  auto terrain_obj = std::make_unique<game::GameTerrain>(
      std::move(data), std::move(material), video_);
  terrain_obj->SetName("terrain");
  scene_->AddDynamicObject(std::move(terrain_obj));

  WireTerrainPanel();
  scene_dirty_ = true;
  LOG_F(INFO, "Terrain created from import: %dx%d texels, 1.0 m/texel, "
        "Y=[%.1f, %.1f]", w, h, min_h, max_h);
}

void EditorWindow::RemoveTerrain() {
  game::GameTerrain* gt = FindTerrain(*scene_);
  if (!gt) return;

  scene_->RemoveDynamicObject(gt);
  show_terrain_panel_ = false;
  history_.Clear();
  WireTerrainPanel();
  scene_dirty_ = true;
  LOG_F(INFO, "Terrain removed from scene");
}

void EditorWindow::CopySelectedObject() {
  clipboard_.clear();
  clipboard_parent_names_.clear();

  for (const game::GameObject* obj : scene_->GetSelection()) {
    const core::Mat4f& t = obj->GetWorldTransform();
    const core::Vec3f  pos(t(0, 3), t(1, 3), t(2, 3));
    if (obj->GetType() == game::GameObjectType::kPivot) {
      CopySubtree(obj, "");
    } else {
      auto clone = obj->Copy(pos);
      if (clone) {
        clipboard_parent_names_.push_back("");
        clipboard_.push_back(std::move(clone));
      }
    }
  }
}

void EditorWindow::CopySubtree(const game::GameObject* obj,
                               const std::string& parent_name) {
  const core::Mat4f& t = obj->GetWorldTransform();
  const core::Vec3f  pos(t(0, 3), t(1, 3), t(2, 3));
  auto clone = obj->Copy(pos);
  if (!clone) {
    LOG_F(WARNING, "CopySubtree: '%s' is not copyable, subtree branch skipped",
          obj->GetName().c_str());
    return;
  }
  const std::string clone_name = clone->GetName();
  clipboard_parent_names_.push_back(parent_name);
  clipboard_.push_back(std::move(clone));
  for (const game::GameObject* child : obj->GetChildren())
    CopySubtree(child, clone_name);
}

void EditorWindow::PasteObject() {
  if (clipboard_.empty()) return;
  scene_->ClearSelection();

  // First pass: place all objects and record name→raw pointer for parent linking.
  std::unordered_map<std::string, game::GameObject*> pasted_by_name;
  std::vector<std::pair<game::GameObject*, std::string>> pending_parent;

  for (std::size_t i = 0; i < clipboard_.size(); ++i) {
    const auto& src = clipboard_[i];
    const core::Mat4f& ct = src->GetWorldTransform();
    const core::Vec3f  paste_pos(ct(0, 3) + 1.f, ct(1, 3), ct(2, 3) + 1.f);
    auto clone = src->Copy(paste_pos);
    if (!clone) continue;
    const std::string src_name = src->GetName();
    clone->SetName(GenerateObjectName(*scene_, BaseNameOf(src_name)));
    game::GameObject* raw = clone.get();
    pasted_by_name[src_name] = raw;
    if (!clipboard_parent_names_[i].empty())
      pending_parent.emplace_back(raw, clipboard_parent_names_[i]);
    history_.Push(std::make_unique<PlaceObjectCommand>(scene_.get(), std::move(clone)));
  }

  // Second pass: link parents by the source name they referenced.
  for (auto& [child, src_parent_name] : pending_parent) {
    auto it = pasted_by_name.find(src_parent_name);
    if (it != pasted_by_name.end())
      child->Reparent(it->second);
  }

  // Select only the root-level pasted objects (those with no parent in the
  // pasted set) so the pivot is selected rather than one of its children.
  scene_->ClearSelection();
  for (std::size_t i = 0; i < clipboard_.size(); ++i) {
    if (clipboard_parent_names_[i].empty()) {
      auto it = pasted_by_name.find(clipboard_[i]->GetName());
      if (it != pasted_by_name.end())
        scene_->AddToSelection(it->second);
    }
  }

  scene_dirty_ = true;
}

void EditorWindow::FallToTerrain() {
  const game::GameTerrain* gt = FindTerrain(*scene_);
  if (!gt) return;
  const terrain::TerrainData& data = gt->GetData();

  for (game::GameObject* obj : scene_->GetSelection()) {
    if (obj->GetType() == game::GameObjectType::kTerrain) continue;

    const core::Mat4f& wt = obj->GetWorldTransform();

    // Object pivot XZ position.
    const float px = wt(0, 3);
    const float pz = wt(2, 3);

    // Terrain surface height and slope normal at the pivot XZ.
    const float terrain_h = data.GetHeight(px, pz);
    const core::Vec3f N   = data.GetNormal(px, pz);

    // Extract per-axis scale from the current transform's column magnitudes.
    const float sx = std::sqrt(wt(0, 0)*wt(0, 0) + wt(1, 0)*wt(1, 0) + wt(2, 0)*wt(2, 0));
    const float sy = std::sqrt(wt(0, 1)*wt(0, 1) + wt(1, 1)*wt(1, 1) + wt(2, 1)*wt(2, 1));
    const float sz = std::sqrt(wt(0, 2)*wt(0, 2) + wt(1, 2)*wt(1, 2) + wt(2, 2)*wt(2, 2));

    // Horizontal projection of the current Z-axis (forward); preserve yaw.
    core::Vec3f fwd(wt(0, 2), 0.f, wt(2, 2));
    const float fwd_len = fwd.Length();
    if (fwd_len < 1e-5f)
      fwd = core::Vec3f(0.f, 0.f, 1.f);
    else
      fwd = fwd / fwd_len;

    // Build an orthonormal frame whose Y-axis is the terrain normal (slope align).
    const core::Vec3f new_right   = N.Cross(fwd).Normalized();
    const core::Vec3f new_forward = new_right.Cross(N).Normalized();

    // Place the pivot so the local bbox bottom (min_y) touches the terrain surface.
    const float local_min_y = obj->GetLocalBBox().GetMin().y;
    const float new_px = px - local_min_y * N.x;
    const float new_py = terrain_h - local_min_y * N.y;
    const float new_pz = pz - local_min_y * N.z;

    const core::Mat4f new_transform(
      new_right.x * sx,   N.x * sy,   new_forward.x * sz,   new_px,
      new_right.y * sx,   N.y * sy,   new_forward.y * sz,   new_py,
      new_right.z * sx,   N.z * sy,   new_forward.z * sz,   new_pz,
      0.f,                0.f,        0.f,                   1.f);

    const core::Mat4f before = wt;
    obj->SetWorldTransform(new_transform);
    history_.Push(std::make_unique<TransformCommand>(obj, before, new_transform));
  }

  if (!scene_->GetSelection().empty()) scene_dirty_ = true;
}

void EditorWindow::CenterCameraOnObject() {
  const auto& sel = scene_->GetSelection();
  if (sel.empty()) return;

  core::BBox3 combined = sel[0]->GetWorldBBox();
  for (std::size_t i = 1; i < sel.size(); ++i)
    combined << sel[i]->GetWorldBBox();

  viewport_->FrameObject(combined);
}

void EditorWindow::PlaceGauge() {
  auto gauge = std::make_unique<game::GameGauge>();
  gauge->SetName(GenerateObjectName(*scene_, "gauge"));
  gauge->SetWorldTransform(
      core::Mat4f::Translation(viewport_->GetCameraFocusPoint()));
  history_.Push(std::make_unique<PlaceObjectCommand>(scene_.get(), std::move(gauge)));
  scene_dirty_ = true;
}

void EditorWindow::GroupUnderPivot() {
  const auto& sel = scene_->GetSelection();
  if (sel.size() < 2) return;
  const std::vector<game::GameObject*> members(sel.begin(), sel.end());
  const std::string name = GenerateObjectName(*scene_, "pivot");
  history_.Push(
      std::make_unique<GroupUnderPivotCommand>(scene_.get(), name, members));
  scene_dirty_ = true;
}

void EditorWindow::UngroupSelectedPivot() {
  const auto& sel = scene_->GetSelection();
  if (sel.size() != 1) return;
  if (sel[0]->GetType() != game::GameObjectType::kPivot) return;
  auto* pivot = static_cast<game::GamePivot*>(sel[0]);
  if (pivot->GetChildren().empty()) return;
  history_.Push(std::make_unique<UngroupPivotCommand>(scene_.get(), pivot));
  scene_dirty_ = true;
}

void EditorWindow::ActivateAlignTool() {
  const EditorTool t = toolbar_->GetActiveTool();
  if (t == EditorTool::kTranslate)
    align_prev_base_tool_ = translate_tool_.get();
  else if (t == EditorTool::kRotate)
    align_prev_base_tool_ = rotate_tool_.get();
  else if (t == EditorTool::kScale)
    align_prev_base_tool_ = scale_tool_.get();
  else
    align_prev_base_tool_ = selection_tool_.get();

  viewport_->SetActiveTool(align_tool_.get());
}

void EditorWindow::WireTerrainPanel() {
  game::GameTerrain* gt = FindTerrain(*scene_);
  if (!gt) {
    terrain_panel_.SetContext(nullptr, nullptr, nullptr, nullptr, nullptr);
    terrain_panel_.SetSculptTool(nullptr);
    viewport_->SetTerrainData(nullptr);
    // Switch away from sculpt tool before destroying it to avoid dangling ptr.
    if (sculpt_tool_active_) {
      sculpt_tool_active_ = false;
      viewport_->SetActiveTool(selection_tool_.get());
    }
    sculpt_tool_.reset();
    return;
  }

  // GameTerrain owns const data. The editor mutates them during sculpt/paint —
  // const_cast is safe because the objects are non-const.
  auto* data     = const_cast<terrain::TerrainData*>(&gt->GetData());
  auto* material = const_cast<terrain::TerrainMaterial*>(&gt->GetMaterial());
  terrain_panel_.SetContext(data, material, video_, &history_, gt);
  terrain_panel_.SetOnFoliageModified([this]{ scene_dirty_ = true; });
  viewport_->SetTerrainData(data);

  sculpt_tool_ = std::make_unique<TerrainSculptTool>(
      data, material, video_, &history_,
      [this](float wx, float wz, bool first, float dt) {
        terrain_panel_.OnFoliageBrush(wx, wz, first, dt);
      },
      [this]() { terrain_panel_.OnFoliageEnd(); });
  terrain_panel_.SetSculptTool(sculpt_tool_.get());
}

void EditorWindow::EnableSceneSound() {
  if (!editor_sound_manager_ || !editor_sound_resources_) return;

  for (game::GameObject* obj : scene_->GetObjects()) {
    if (obj->GetType() != game::GameObjectType::kSoundEmitter) continue;
    auto* emitter = static_cast<game::GameSoundEmitter*>(obj);
    emitter->SetManagers(editor_sound_manager_.get(),
                         editor_sound_resources_.get());
  }
}

void EditorWindow::DisableSceneSound() {
  if (editor_sound_manager_) editor_sound_manager_->StopAll();

  if (!scene_) return;
  for (game::GameObject* obj : scene_->GetObjects()) {
    if (obj->GetType() != game::GameObjectType::kSoundEmitter) continue;
    auto* emitter = static_cast<game::GameSoundEmitter*>(obj);
    emitter->SetManagers(nullptr, nullptr);
  }
}

}  // namespace editor
