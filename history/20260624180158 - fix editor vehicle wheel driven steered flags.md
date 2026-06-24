# fix(editor): VehicleEditorWindow missing is_driven / is_steered wheel controls and save

**Date**: 2026-06-24  
**Branch**: `fix/editor-vehicle-wheel-driven-steered-flags`  
**PR**: #779  
**Issue**: #777

## Problem

`VehicleEditorWindow` had three gaps around the `is_driven` and `is_steered` fields of `WheelDesc`:

1. **Load** — `LoadFromYaml()` used a `read_pos` lambda that only pulled `position` from each wheel node.
2. **UI** — `DrawWheelsSection()` had no controls for those flags.
3. **Save** — `SaveToYaml()`'s `write_wheel` lambda only emitted `position`.

The net effect: opening any vehicle YAML and re-saving it silently stripped the flags, breaking physics behaviour (driven/steering wheels would reset to `false`).

## Changes (`src/editor/VehicleEditorWindow.cpp`)

### LoadFromYaml (lines 172-186)

Renamed `read_pos` → `read_wheel` and extended it to also read `is_driven` and `is_steered`:

```cpp
auto read_wheel = [](physics::WheelDesc& w, const YAML::Node& n) {
  if (!n) return;
  if (n["position"])   w.position   = core::ParseVec3(n["position"], w.position);
  if (n["is_driven"])  w.is_driven  = n["is_driven"].as<bool>(w.is_driven);
  if (n["is_steered"]) w.is_steered = n["is_steered"].as<bool>(w.is_steered);
};
```

This mirrors the `read_wheel` lambda already in `VehicleTemplate.cpp` (lines 116-117).

### DrawWheelsSection (lines 639-642)

Added two inline checkboxes per wheel after the `DragFloat3`:

```cpp
ImGui::SameLine();
if (ImGui::Checkbox("Driven##drv",  &wheels[i]->is_driven))  dirty_ = true;
ImGui::SameLine();
if (ImGui::Checkbox("Steered##str", &wheels[i]->is_steered)) dirty_ = true;
```

The `PushID(i)` already wraps each row, making `##drv` / `##str` suffixes unique across all four wheels.

### SaveToYaml (lines 228-241)

Extended `write_wheel` to take a full `WheelDesc` and emit all three fields:

```cpp
auto write_wheel = [&out](const char* key, const physics::WheelDesc& w) {
  out << YAML::Key << key << YAML::Value << YAML::BeginMap;
  out << YAML::Key << "position" << YAML::Value << YAML::Flow
      << YAML::BeginSeq << w.position.x << w.position.y << w.position.z << YAML::EndSeq;
  out << YAML::Key << "is_driven"  << YAML::Value << w.is_driven;
  out << YAML::Key << "is_steered" << YAML::Value << w.is_steered;
  out << YAML::EndMap;
};
```

The wheel node is no longer emitted with `YAML::Flow` at the map level (it was before) so the three keys are written on separate lines — matches the style used by the `physics.suspension` block.

## Decisions

- **Per-wheel independence**: The mirror logic that symmetrises `position.x` across opposite wheels deliberately does **not** apply to `is_driven`/`is_steered`. The flags are independent per wheel, giving maximum flexibility (e.g., a single-wheel-drive setup).
- **No `YAML::Flow` on wheel maps**: Removed `YAML::Flow` from the wheel-level map so that `position`, `is_driven`, and `is_steered` are each on their own line, consistent with the rest of the file and more readable in diffs.
- **Checkbox label uniqueness**: Used `##drv`/`##str` suffixes (not per-wheel index) because the row is already wrapped in `ImGui::PushID(i)`, making the full ImGui ID unique.

## Skills / instructions used

- `impl-issue` skill
- `src/CLAUDE.md` — Google C++ style, one class per file
- `src/editor/CLAUDE.md` — ImGui bracketing, UI/logic separation

## Keep in mind for future features

- The `write_wheel` lambda in `SaveToYaml` now takes `const physics::WheelDesc&` — if new per-wheel fields are added to `WheelDesc`, update both `read_wheel` (load) and `write_wheel` (save) together.
- The format written here is what `VehicleTemplate.cpp` already expects; no changes were needed there.
