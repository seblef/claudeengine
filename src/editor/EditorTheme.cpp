#include "editor/EditorTheme.h"

#include <imgui.h>

namespace editor {

namespace {

// ---- Palette ---------------------------------------------------------------
// Fully opaque base colors.
constexpr ImVec4 kWindowBg {0.118f, 0.133f, 0.157f, 1.0f};  // #1E2228
constexpr ImVec4 kPanelBg  {0.153f, 0.173f, 0.208f, 1.0f};  // #272C35
constexpr ImVec4 kTitleBg  {0.102f, 0.122f, 0.149f, 1.0f};  // #1A1F26
constexpr ImVec4 kBorder   {0.208f, 0.235f, 0.282f, 1.0f};  // #353C48
constexpr ImVec4 kAccent   {0.184f, 0.769f, 0.698f, 1.0f};  // #2FC4B2
constexpr ImVec4 kText     {0.831f, 0.847f, 0.878f, 1.0f};  // #D4D8E0
constexpr ImVec4 kDimmed   {0.420f, 0.451f, 0.498f, 1.0f};  // #6B737F

// Derived: border brightened ~10% for hover states.
constexpr ImVec4 kBorderHover{0.310f, 0.340f, 0.390f, 1.0f};
// Derived: accent at various alpha levels.
constexpr ImVec4 kAccentBright{0.350f, 0.870f, 0.800f, 1.0f};

constexpr ImVec4 WithAlpha(ImVec4 c, float a) { return {c.x, c.y, c.z, a}; }
constexpr ImVec4 kTransparent{0.0f, 0.0f, 0.0f, 0.0f};

}  // namespace

void ApplySlateAndTealTheme() {
  ImGuiStyle& s = ImGui::GetStyle();

  // ---- Rounding & metrics --------------------------------------------------
  s.WindowRounding   = 6.0f;
  s.ChildRounding    = 4.0f;
  s.FrameRounding    = 4.0f;
  s.PopupRounding    = 6.0f;
  s.ScrollbarRounding = 4.0f;
  s.GrabRounding     = 4.0f;
  s.TabRounding      = 4.0f;

  s.WindowBorderSize = 1.0f;
  s.ChildBorderSize  = 0.0f;
  s.FrameBorderSize  = 0.0f;

  s.FramePadding  = {6.0f, 4.0f};
  s.ItemSpacing   = {8.0f, 4.0f};
  s.ScrollbarSize = 12.0f;

  // ---- Colors --------------------------------------------------------------
  ImVec4* c = s.Colors;

  c[ImGuiCol_Text]                 = kText;
  c[ImGuiCol_TextDisabled]         = kDimmed;
  c[ImGuiCol_TextLink]             = kAccent;

  c[ImGuiCol_WindowBg]             = kWindowBg;
  c[ImGuiCol_ChildBg]              = kPanelBg;
  c[ImGuiCol_PopupBg]              = kWindowBg;

  c[ImGuiCol_Border]               = kBorder;
  c[ImGuiCol_BorderShadow]         = kTransparent;

  c[ImGuiCol_FrameBg]              = kPanelBg;
  c[ImGuiCol_FrameBgHovered]       = WithAlpha(kAccent, 0.15f);
  c[ImGuiCol_FrameBgActive]        = WithAlpha(kAccent, 0.30f);

  c[ImGuiCol_TitleBg]              = kTitleBg;
  c[ImGuiCol_TitleBgActive]        = kWindowBg;
  c[ImGuiCol_TitleBgCollapsed]     = kTitleBg;

  c[ImGuiCol_MenuBarBg]            = kTitleBg;

  c[ImGuiCol_ScrollbarBg]          = kTitleBg;
  c[ImGuiCol_ScrollbarGrab]        = kBorder;
  c[ImGuiCol_ScrollbarGrabHovered] = kBorderHover;
  c[ImGuiCol_ScrollbarGrabActive]  = kAccent;

  c[ImGuiCol_CheckMark]            = kAccent;
  c[ImGuiCol_CheckboxSelectedBg]   = WithAlpha(kAccent, 0.80f);

  c[ImGuiCol_SliderGrab]           = kAccent;
  c[ImGuiCol_SliderGrabActive]     = kAccentBright;

  c[ImGuiCol_InputTextCursor]      = kText;

  c[ImGuiCol_Button]               = kPanelBg;
  c[ImGuiCol_ButtonHovered]        = kBorder;
  c[ImGuiCol_ButtonActive]         = WithAlpha(kAccent, 0.80f);

  c[ImGuiCol_Header]               = kPanelBg;
  c[ImGuiCol_HeaderHovered]        = WithAlpha(kAccent, 0.18f);
  c[ImGuiCol_HeaderActive]         = WithAlpha(kAccent, 0.35f);

  c[ImGuiCol_Separator]            = kBorder;
  c[ImGuiCol_SeparatorHovered]     = WithAlpha(kAccent, 0.70f);
  c[ImGuiCol_SeparatorActive]      = kAccent;

  c[ImGuiCol_ResizeGrip]           = WithAlpha(kBorder, 0.50f);
  c[ImGuiCol_ResizeGripHovered]    = WithAlpha(kAccent, 0.70f);
  c[ImGuiCol_ResizeGripActive]     = kAccent;

  c[ImGuiCol_Tab]                  = kTitleBg;
  c[ImGuiCol_TabHovered]           = WithAlpha(kAccent, 0.20f);
  c[ImGuiCol_TabSelected]          = kPanelBg;
  c[ImGuiCol_TabSelectedOverline]  = kAccent;
  c[ImGuiCol_TabDimmed]            = kTitleBg;
  c[ImGuiCol_TabDimmedSelected]    = kWindowBg;
  c[ImGuiCol_TabDimmedSelectedOverline] = kTransparent;

  c[ImGuiCol_DockingPreview]       = WithAlpha(kAccent, 0.35f);
  c[ImGuiCol_DockingEmptyBg]       = kTitleBg;

  c[ImGuiCol_PlotLines]            = kAccent;
  c[ImGuiCol_PlotLinesHovered]     = kAccentBright;
  c[ImGuiCol_PlotHistogram]        = kAccent;
  c[ImGuiCol_PlotHistogramHovered] = kAccentBright;

  c[ImGuiCol_TableHeaderBg]        = kTitleBg;
  c[ImGuiCol_TableBorderStrong]    = kBorder;
  c[ImGuiCol_TableBorderLight]     = kPanelBg;
  c[ImGuiCol_TableRowBg]           = kTransparent;
  c[ImGuiCol_TableRowBgAlt]        = {1.0f, 1.0f, 1.0f, 0.03f};

  c[ImGuiCol_TreeLines]            = WithAlpha(kBorder, 0.50f);

  c[ImGuiCol_TextSelectedBg]       = WithAlpha(kAccent, 0.25f);

  c[ImGuiCol_DragDropTarget]       = kAccent;
  c[ImGuiCol_DragDropTargetBg]     = WithAlpha(kAccent, 0.20f);

  c[ImGuiCol_NavCursor]            = kAccent;
  c[ImGuiCol_NavWindowingHighlight] = {1.0f, 1.0f, 1.0f, 0.70f};
  c[ImGuiCol_NavWindowingDimBg]    = {0.0f, 0.0f, 0.0f, 0.20f};

  c[ImGuiCol_ModalWindowDimBg]     = {0.0f, 0.0f, 0.0f, 0.50f};

  c[ImGuiCol_UnsavedMarker]        = {0.90f, 0.60f, 0.10f, 1.0f};
}

}  // namespace editor
