#include "editor/ObjectNamingUtils.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>

#include "editor/EditorScene.h"
#include "editor/ObjectGroup.h"
#include "game/GameObject.h"

namespace editor {

namespace {

int HighestIndexForPrefix(const std::string& name, const std::string& prefix) {
  if (name.size() <= prefix.size()) return -1;
  if (name.compare(0, prefix.size(), prefix) != 0) return -1;
  const std::string suffix = name.substr(prefix.size());
  if (suffix.empty() ||
      !std::all_of(suffix.begin(), suffix.end(), ::isdigit))
    return -1;
  return std::stoi(suffix);
}

}  // namespace

std::string GenerateObjectName(const EditorScene& scene,
                               const std::string& base_name,
                               bool use_groups) {
  const std::string prefix = base_name + "_";
  int highest = 0;
  for (const game::GameObject* obj : scene.GetObjects()) {
    const int idx = HighestIndexForPrefix(obj->GetName(), prefix);
    if (idx > highest) highest = idx;
  }
  if (use_groups) {
    for (const ObjectGroup& grp : scene.GetGroups()) {
      const int idx = HighestIndexForPrefix(grp.name, prefix);
      if (idx > highest) highest = idx;
    }
  }
  char buf[12];
  std::snprintf(buf, sizeof(buf), "%03d", highest + 1);
  return base_name + "_" + buf;
}

}  // namespace editor
