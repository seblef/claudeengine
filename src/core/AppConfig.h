#pragma once

#include <filesystem>

#include <yaml-cpp/yaml.h>

#include "core/PhysicsConfig.h"
#include "core/PlayerConfig.h"
#include "core/PostProcessConfig.h"
#include "core/ShadowConfig.h"

namespace core {

// Holds graphics/window configuration from the [graphics] section of config.yaml.
class GraphicsConfig {
 public:
  // Populates fields from the provided YAML node (the "graphics" subtree).
  void Parse(const YAML::Node& node);

  [[nodiscard]] int  GetWidth()   const { return width_; }
  [[nodiscard]] int  GetHeight()  const { return height_; }
  [[nodiscard]] bool IsWindowed() const { return windowed_; }

 private:
  int  width_    = 640;
  int  height_   = 480;
  bool windowed_ = true;
};

// Top-level application configuration loaded from config.yaml.
// Call Init() once at startup, after Config::Init() has set the data folder.
class AppConfig {
 public:
  AppConfig() = delete;

  static void Init(const std::filesystem::path& config_path);

  [[nodiscard]] static const GraphicsConfig&    GetGraphics()           { return graphics_; }
  [[nodiscard]] static const ShadowConfig&      GetShadows()            { return shadows_; }
  [[nodiscard]] static const PostProcessConfig& GetPostProcess()        { return post_process_; }
  [[nodiscard]] static       PostProcessConfig& GetMutablePostProcess() { return post_process_; }
  [[nodiscard]] static const PlayerConfig&      GetPlayer()             { return player_; }
  [[nodiscard]] static const PhysicsConfig&     GetPhysics()            { return physics_; }

 private:
  static GraphicsConfig    graphics_;
  static ShadowConfig      shadows_;
  static PostProcessConfig post_process_;
  static PlayerConfig      player_;
  static PhysicsConfig     physics_;
};

}  // namespace core
