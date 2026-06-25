#include "editor/tools/AlignTool.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "core/BBox3.h"
#include "core/Event.h"
#include "core/EventType.h"
#include "core/Key.h"
#include "core/Mat4f.h"
#include "core/Vec3f.h"
#include "core/Vec4f.h"
#include "editor/EditorCommandHistory.h"
#include "editor/EditorScene.h"
#include "editor/commands/MultiTransformCommand.h"
#include "editor/tools/EditorToolContext.h"
#include "editor/tools/PickingUtils.h"
#include "game/GameCamera.h"
#include "game/GameObject.h"

#include <imgui.h>

namespace editor {

namespace {

constexpr ImU32  kPickHoverColor     = IM_COL32(255, 220, 0, 200);
constexpr float  kPickHoverThickness = 1.5f;
constexpr ImU32  kPickHintBg         = IM_COL32(0, 0, 0, 140);
constexpr char   kPopupId[]          = "Align##modal";
const char* const kRefLabels[]       = {"Min", "Ctr", "Max"};

// Draws a yellow wireframe bounding box around obj for pick-target feedback.
void DrawPickHoverBBox(const game::GameObject* obj, const core::Mat4f& vp,
                       ImDrawList* dl, ImVec2 image_pos, ImVec2 image_size) {
  const auto& bbox    = obj->GetLocalBBox();
  const auto& model   = obj->GetWorldTransform();
  const auto  corners = bbox.GetCorners();

  struct ScreenPt { ImVec2 pos; bool valid; };
  ScreenPt sc[8];
  for (int i = 0; i < 8; ++i) {
    const core::Vec3f w = corners[i] * model;
    const core::Vec4f c = core::Vec4f(w.x, w.y, w.z, 1.f) * vp;
    if (c.w <= 0.f) {
      sc[i].valid = false;
      continue;
    }
    const float iw  = 1.f / c.w;
    sc[i].pos   = {(c.x * iw + 1.f) * 0.5f * image_size.x + image_pos.x,
                   (1.f - c.y * iw) * 0.5f * image_size.y + image_pos.y};
    sc[i].valid = true;
  }

  constexpr int kEdges[12][2] = {
    {0,1},{1,3},{3,2},{2,0},
    {4,5},{5,7},{7,6},{6,4},
    {0,4},{1,5},{3,7},{2,6},
  };
  for (int i = 0; i < 12; ++i) {
    const int* e = kEdges[i];
    if (sc[e[0]].valid && sc[e[1]].valid)
      dl->AddLine(sc[e[0]].pos, sc[e[1]].pos, kPickHoverColor, kPickHoverThickness);
  }
}

}  // namespace

float AlignTool::RefPoint(const core::BBox3& bbox, int axis, int ref) {
  const core::Vec3f& mn = bbox.GetMin();
  const core::Vec3f& mx = bbox.GetMax();
  const float mn_v = axis == 0 ? mn.x : (axis == 1 ? mn.y : mn.z);
  const float mx_v = axis == 0 ? mx.x : (axis == 1 ? mx.y : mx.z);
  if (ref == 0) return mn_v;
  if (ref == 2) return mx_v;
  return (mn_v + mx_v) * 0.5f;
}

void AlignTool::OnActivate(const EditorToolContext& ctx) {
  scene_      = ctx.scene;
  history_    = ctx.history;
  state_      = State::kPickingTarget;
  hovered_    = nullptr;
  target_     = nullptr;
  open_popup_ = false;
  saved_transforms_.clear();
}

void AlignTool::OnDeactivate() {
  scene_           = nullptr;
  history_         = nullptr;
  hovered_         = nullptr;
  target_          = nullptr;
  saved_transforms_.clear();
}

void AlignTool::OnEvent(const core::Event& event) {
  if (state_ == State::kPickingTarget &&
      event.type == core::EventType::kKeyDown &&
      event.key  == core::Key::kEscape) {
    Done();
  }
}

bool AlignTool::IsCapturingMouse() const {
  return true;
}

void AlignTool::RestoreSaved() {
  for (const auto& entry : saved_transforms_)
    entry.first->SetWorldTransform(entry.second);
}

void AlignTool::PreviewAlignment(const EditorToolContext& ctx) {
  if (!target_ || !ctx.scene) return;

  const auto&       sel      = ctx.scene->GetSelection();
  const core::BBox3 tgt_bbox = target_->GetWorldBBox();

  auto compute_delta = [&](const core::BBox3& src_bbox) -> core::Vec3f {
    core::Vec3f d(0.f, 0.f, 0.f);
    for (int a = 0; a < 3; ++a) {
      if (!options_.align[a]) continue;
      const float delta = RefPoint(tgt_bbox, a, options_.tgt_ref[a])
                        - RefPoint(src_bbox, a, options_.src_ref[a]);
      if (a == 0) d.x = delta;
      else if (a == 1) d.y = delta;
      else d.z = delta;
    }
    return d;
  };

  if (options_.group_mode && sel.size() > 1) {
    core::BBox3 group_bbox = sel[0]->GetWorldBBox();
    for (std::size_t i = 1; i < sel.size(); ++i)
      group_bbox << sel[i]->GetWorldBBox();

    const core::Vec3f delta = compute_delta(group_bbox);
    for (game::GameObject* obj : sel) {
      core::Mat4f t = obj->GetWorldTransform();
      t(0, 3) += delta.x;
      t(1, 3) += delta.y;
      t(2, 3) += delta.z;
      obj->SetWorldTransform(t);
    }
  } else {
    for (game::GameObject* obj : sel) {
      const core::Vec3f delta = compute_delta(obj->GetWorldBBox());
      core::Mat4f t = obj->GetWorldTransform();
      t(0, 3) += delta.x;
      t(1, 3) += delta.y;
      t(2, 3) += delta.z;
      obj->SetWorldTransform(t);
    }
  }
}

void AlignTool::CommitAlignment() {
  if (saved_transforms_.empty() || !history_) return;

  std::vector<MultiTransformCommand::Entry> entries;
  entries.reserve(saved_transforms_.size());
  std::transform(saved_transforms_.begin(), saved_transforms_.end(),
                 std::back_inserter(entries),
                 [](const std::pair<game::GameObject*, core::Mat4f>& e) {
                   return MultiTransformCommand::Entry{
                       e.first, e.second, e.first->GetWorldTransform()};
                 });
  if (!entries.empty())
    history_->Push(std::make_unique<MultiTransformCommand>(std::move(entries)));
}

AlignTool::ModalResult AlignTool::RenderModal() {
  if (open_popup_) {
    ImGui::OpenPopup(kPopupId);
    open_popup_ = false;
  }

  ModalResult result = ModalResult::kOpen;

  constexpr ImGuiWindowFlags kModalFlags = ImGuiWindowFlags_AlwaysAutoResize;
  if (!ImGui::BeginPopupModal(kPopupId, nullptr, kModalFlags))
    return result;

  const char* target_name = target_ ? target_->GetName().c_str() : "(none)";
  ImGui::Text("Align to: %s", target_name);
  ImGui::Separator();
  ImGui::Spacing();

  constexpr ImGuiTableFlags kTableFlags =
      ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit;
  if (ImGui::BeginTable("##align_axes", 5, kTableFlags)) {
    ImGui::TableSetupColumn("Axis",       ImGuiTableColumnFlags_WidthFixed, 24.f);
    ImGui::TableSetupColumn("Align",      ImGuiTableColumnFlags_WidthFixed, 96.f);
    ImGui::TableSetupColumn("Source ref", ImGuiTableColumnFlags_WidthFixed, 120.f);
    ImGui::TableSetupColumn("Arrow",      ImGuiTableColumnFlags_WidthFixed, 16.f);
    ImGui::TableSetupColumn("Target ref", ImGuiTableColumnFlags_WidthFixed, 120.f);
    ImGui::TableHeadersRow();

    const char* kAxisLabels[] = {"X", "Y", "Z"};
    for (int a = 0; a < 3; ++a) {
      ImGui::TableNextRow();
      ImGui::PushID(a);

      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(kAxisLabels[a]);

      ImGui::TableSetColumnIndex(1);
      int align_val = options_.align[a] ? 1 : 0;
      ImGui::RadioButton("Align##al",  &align_val, 1); ImGui::SameLine();
      ImGui::RadioButton("Ignore##ig", &align_val, 0);
      options_.align[a] = (align_val == 1);

      const bool active = options_.align[a];

      ImGui::TableSetColumnIndex(2);
      ImGui::BeginDisabled(!active);
      for (int r = 0; r < 3; ++r) {
        ImGui::PushID(r);
        ImGui::RadioButton(kRefLabels[r], &options_.src_ref[a], r);
        ImGui::PopID();
        if (r < 2) ImGui::SameLine();
      }
      ImGui::EndDisabled();

      ImGui::TableSetColumnIndex(3);
      ImGui::BeginDisabled(!active);
      ImGui::TextUnformatted("\xe2\x86\x92");  // UTF-8 → (U+2192)
      ImGui::EndDisabled();

      ImGui::TableSetColumnIndex(4);
      ImGui::BeginDisabled(!active);
      for (int r = 0; r < 3; ++r) {
        ImGui::PushID(100 + r);
        ImGui::RadioButton(kRefLabels[r], &options_.tgt_ref[a], r);
        ImGui::PopID();
        if (r < 2) ImGui::SameLine();
      }
      ImGui::EndDisabled();

      ImGui::PopID();
    }
    ImGui::EndTable();
  }

  // Group mode row — only visible for multi-selection.
  if (scene_ && scene_->GetSelection().size() > 1) {
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextUnformatted("Align as:");
    ImGui::SameLine();
    int gm = options_.group_mode ? 1 : 0;
    ImGui::RadioButton("Each independently", &gm, 0); ImGui::SameLine();
    ImGui::RadioButton("Group",              &gm, 1);
    options_.group_mode = (gm == 1);
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  if (ImGui::Button("Cancel", {80.f, 0.f})) {
    ImGui::CloseCurrentPopup();
    result = ModalResult::kCancel;
  }
  ImGui::SameLine();
  if (ImGui::Button("Apply", {80.f, 0.f})) {
    ImGui::CloseCurrentPopup();
    result = ModalResult::kApply;
  }

  ImGui::EndPopup();
  return result;
}

void AlignTool::Done() {
  state_      = State::kPickingTarget;
  hovered_    = nullptr;
  target_     = nullptr;
  open_popup_ = false;
  if (on_done_) on_done_();
}

void AlignTool::OnRender(const EditorToolContext& ctx,
                          ImVec2 image_pos, ImVec2 image_size) {
  if (!ctx.scene) return;

  if (state_ == State::kPickingTarget) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 hint_pos = {image_pos.x + 8.f, image_pos.y + 8.f};
    const char* hint = "Click target object to align to — Escape to cancel";
    const ImVec2 text_sz = ImGui::CalcTextSize(hint);
    constexpr float kPad = 4.f;
    dl->AddRectFilled(
        {hint_pos.x - kPad, hint_pos.y - kPad},
        {hint_pos.x + text_sz.x + kPad, hint_pos.y + text_sz.y + kPad},
        kPickHintBg, 3.f);
    dl->AddText(hint_pos, IM_COL32(255, 220, 0, 255), hint);

    const bool window_hovered = ImGui::IsWindowHovered();
    if (window_hovered) {
      game::GameObject* candidate =
          PickHitAt(ctx, ImGui::GetMousePos(), image_pos, image_size);
      hovered_ = (candidate && !ctx.scene->IsSelected(candidate))
                     ? candidate : nullptr;
    } else {
      hovered_ = nullptr;
    }

    if (hovered_) {
      const core::Mat4f& vp = ctx.camera->GetCamera()->GetViewProjectionMatrix();
      DrawPickHoverBBox(hovered_, vp, dl, image_pos, image_size);
    }

    if (window_hovered && hovered_ &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      target_     = hovered_;
      hovered_    = nullptr;
      state_      = State::kShowingModal;
      open_popup_ = true;
    }
    return;
  }

  // kShowingModal — live preview.

  // Capture originals on the first frame (when open_popup_ is still true).
  if (open_popup_) {
    saved_transforms_.clear();
    const auto& sel = ctx.scene->GetSelection();
    saved_transforms_.reserve(sel.size());
    std::transform(sel.begin(), sel.end(),
                   std::back_inserter(saved_transforms_),
                   [](game::GameObject* obj) {
                     return std::make_pair(obj, obj->GetWorldTransform());
                   });
  }

  // Each frame: restore originals then re-apply with current options so the
  // viewport reflects whatever the user has set in the modal this frame.
  RestoreSaved();
  PreviewAlignment(ctx);

  const ModalResult result = RenderModal();

  if (result == ModalResult::kApply) {
    CommitAlignment();
    Done();
  } else if (result == ModalResult::kCancel) {
    RestoreSaved();
    Done();
  }
}

}  // namespace editor
