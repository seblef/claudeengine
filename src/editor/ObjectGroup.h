#pragma once

#include <string>
#include <vector>

namespace game { class GameObject; }

namespace editor {

// An editor-only group of scene objects.
//
// Groups are a pure editor concept: they affect selection and transform
// behaviour in the editor but have no runtime representation in the game.
// Saved and loaded from the "editor" section of the map YAML.
//
// Closed group: clicking any member selects the whole group; transform tools
//   apply to all members simultaneously around the group centre.
// Open group: each member can be selected and modified independently.
struct ObjectGroup {
  // cppcheck-suppress unusedStructMember
  std::string                    name;
  // cppcheck-suppress unusedStructMember
  bool                           is_open = false;
  // cppcheck-suppress unusedStructMember
  std::vector<game::GameObject*> objects;
};

}  // namespace editor
