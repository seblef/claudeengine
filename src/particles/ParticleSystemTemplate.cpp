#include "particles/ParticleSystemTemplate.h"

#include <algorithm>
#include <iterator>
#include <string>

#include <loguru.hpp>
#include <yaml-cpp/yaml.h>

#include "core/Color.h"
#include "core/Config.h"
#include "core/Vec3f.h"
#include "core/YamlUtils.h"

namespace particles {

namespace {

ParticleBlendMode ParseBlendMode(const std::string& s) {
  if (s == "additive")    return ParticleBlendMode::kAdditive;
  if (s == "alpha_blend") return ParticleBlendMode::kAlphaBlend;
  return ParticleBlendMode::kGBuffer;
}

ParticleAnimationMode ParseAnimationMode(const std::string& s) {
  if (s == "random") return ParticleAnimationMode::kRandom;
  return ParticleAnimationMode::kSequential;
}

EmitterShape ParseEmitterShape(const std::string& s) {
  if (s == "sphere") return EmitterShape::kSphere;
  if (s == "box")    return EmitterShape::kBox;
  if (s == "cone")   return EmitterShape::kCone;
  return EmitterShape::kPoint;
}

renderer::LightType ParseLightType(const std::string& s) {
  if (s == "circle_spot") return renderer::LightType::kCircleSpot;
  return renderer::LightType::kOmni;
}

ParticleSubSystemDesc ParseSubSystem(const YAML::Node& node) {
  ParticleSubSystemDesc desc;

  desc.name = node["name"].as<std::string>("");

  if (node["blend_mode"])
    desc.blend_mode = ParseBlendMode(node["blend_mode"].as<std::string>(""));
  if (node["lit"])
    desc.lit = node["lit"].as<bool>(false);
  if (node["texture"])
    desc.texture = node["texture"].as<std::string>("");
  if (node["sprite_cols"])
    desc.sprite_cols = node["sprite_cols"].as<int>(1);
  if (node["sprite_rows"])
    desc.sprite_rows = node["sprite_rows"].as<int>(1);
  if (node["animation_fps"])
    desc.animation_fps = node["animation_fps"].as<float>(0.f);
  if (node["animation_mode"])
    desc.animation_mode =
        ParseAnimationMode(node["animation_mode"].as<std::string>(""));

  if (node["emitter_shape"])
    desc.emitter_shape =
        ParseEmitterShape(node["emitter_shape"].as<std::string>(""));
  if (node["emitter_radius"])
    desc.emitter_radius = node["emitter_radius"].as<float>(0.f);

  if (node["emission_rate"])
    desc.emission_rate = node["emission_rate"].as<float>(10.f);
  if (node["max_particles"])
    desc.max_particles = node["max_particles"].as<int>(100);
  if (node["looping"])
    desc.looping = node["looping"].as<bool>(true);
  if (node["duration"])
    desc.duration = node["duration"].as<float>(1.f);

  if (node["lifetime"]) {
    desc.lifetime_min = node["lifetime"]["min"].as<float>(desc.lifetime_min);
    desc.lifetime_max = node["lifetime"]["max"].as<float>(desc.lifetime_max);
  }
  if (node["speed"]) {
    desc.speed_min = node["speed"]["min"].as<float>(desc.speed_min);
    desc.speed_max = node["speed"]["max"].as<float>(desc.speed_max);
  }

  if (node["direction"])
    desc.direction = core::ParseVec3(node["direction"], desc.direction);
  if (node["spread"])
    desc.spread = node["spread"].as<float>(desc.spread);
  if (node["gravity"])
    desc.gravity = node["gravity"].as<float>(desc.gravity);

  if (node["size_start"]) {
    desc.size_start_min =
        node["size_start"]["min"].as<float>(desc.size_start_min);
    desc.size_start_max =
        node["size_start"]["max"].as<float>(desc.size_start_max);
  }
  if (node["size_end"]) {
    desc.size_end_min = node["size_end"]["min"].as<float>(desc.size_end_min);
    desc.size_end_max = node["size_end"]["max"].as<float>(desc.size_end_max);
  }

  if (node["color_start"])
    desc.color_start = core::ParseColor(node["color_start"], desc.color_start);
  if (node["color_end"])
    desc.color_end = core::ParseColor(node["color_end"], desc.color_end);

  return desc;
}

EmbeddedLightDesc ParseLight(const YAML::Node& node) {
  EmbeddedLightDesc desc;

  if (node["type"])
    desc.type = ParseLightType(node["type"].as<std::string>(""));
  if (node["color"])
    desc.color = core::ParseColor(node["color"], desc.color);
  if (node["intensity_min"])
    desc.intensity_min = node["intensity_min"].as<float>(desc.intensity_min);
  if (node["intensity_max"])
    desc.intensity_max = node["intensity_max"].as<float>(desc.intensity_max);
  if (node["radius"])
    desc.radius = node["radius"].as<float>(desc.radius);
  if (node["local_offset"])
    desc.local_offset = core::ParseVec3(node["local_offset"], desc.local_offset);
  if (node["flicker_frequency"])
    desc.flicker_frequency =
        node["flicker_frequency"].as<float>(desc.flicker_frequency);
  if (node["flicker_amplitude"])
    desc.flicker_amplitude =
        node["flicker_amplitude"].as<float>(desc.flicker_amplitude);

  return desc;
}

}  // namespace

ParticleSystemTemplate::ParticleSystemTemplate(const std::string& name,
                                               abstract::VideoDevice* /*video*/)
    : Resource(name) {
  const auto path =
      core::Config::GetDataFolder() / "particles" / (name + ".particles.yaml");

  YAML::Node root;
  try {
    root = core::LoadYamlFile(path);
  } catch (const std::exception& e) {
    LOG_F(ERROR, "ParticleSystemTemplate: failed to load '%s': %s",
          path.string().c_str(), e.what());
    return;
  }

  if (root["sub_systems"] && root["sub_systems"].IsSequence()) {
    std::transform(root["sub_systems"].begin(), root["sub_systems"].end(),
                   std::back_inserter(sub_systems_), ParseSubSystem);
  }

  if (root["lights"] && root["lights"].IsSequence()) {
    std::transform(root["lights"].begin(), root["lights"].end(),
                   std::back_inserter(lights_), ParseLight);
  }

  initialized_ = true;
  LOG_F(INFO, "ParticleSystemTemplate: loaded '%s' (%zu sub-systems, %zu lights)",
        name.c_str(), sub_systems_.size(), lights_.size());
}

// static
ParticleSystemTemplate* ParticleSystemTemplate::GetOrLoad(
    const std::string& name, abstract::VideoDevice* video) {
  ParticleSystemTemplate* existing = Get(name);
  if (existing) {
    existing->AddRef();
    return existing;
  }
  return new ParticleSystemTemplate(name, video);
}

// static
std::map<std::string, ParticleSystemTemplate*> ParticleSystemTemplate::GetAll() {
  std::map<std::string, ParticleSystemTemplate*> result;
  for (const auto& kv : Resource::GetRegistry())
    result[kv.first] = kv.second;
  return result;
}

const std::vector<ParticleSubSystemDesc>&
ParticleSystemTemplate::GetSubSystems() const {
  return sub_systems_;
}

const std::vector<EmbeddedLightDesc>& ParticleSystemTemplate::GetLights() const {
  return lights_;
}

}  // namespace particles
