#include "editor/ObjectNamingUtils.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>

#include "editor/EditorScene.h"
#include "game/GameObject.h"

namespace editor {

std::string GenerateObjectName(const EditorScene& scene,
                               const std::string& base_name) {
  const std::string prefix = base_name + "_";
  int highest = 0;
  for (const game::GameObject* obj : scene.GetObjects()) {
    const std::string& name = obj->GetName();
    if (name.size() <= prefix.size()) continue;
    if (name.compare(0, prefix.size(), prefix) != 0) continue;
    const std::string suffix = name.substr(prefix.size());
    if (suffix.empty() ||
        !std::all_of(suffix.begin(), suffix.end(), ::isdigit))
      continue;
    const int idx = std::stoi(suffix);
    if (idx > highest) highest = idx;
  }
  char buf[8];
  std::snprintf(buf, sizeof(buf), "%03d", highest + 1);
  return base_name + "_" + buf;
}

}  // namespace editor
