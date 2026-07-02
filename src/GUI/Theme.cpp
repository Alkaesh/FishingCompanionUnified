// ============================================================================
//  Theme.cpp - compact dark Byster-style skin for the ImGui overlay.
// ----------------------------------------------------------------------------
//  Colors come from the Palette constants in Theme.h (single source of truth).
// ============================================================================

#include "Theme.h"
#include "imgui.h"

namespace fc {

void Theme::LoadFonts()
{
    ImGuiIO& io = ImGui::GetIO();

    ImFontConfig config;
    config.OversampleH = 2;
    config.OversampleV = 1;
    config.PixelSnapH = false;

    ImFont* font = io.Fonts->AddFontFromFileTTF(
        "C:\\Windows\\Fonts\\segoeui.ttf",
        16.0f,
        &config,
        io.Fonts->GetGlyphRangesCyrillic());

    if (font)
        io.FontDefault = font;
    else
        io.Fonts->AddFontDefault();
}

void Theme::Apply()
{
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding    = 12.0f;
    style.ChildRounding     = 10.0f;
    style.FrameRounding     = 7.0f;
    style.PopupRounding     = 8.0f;
    style.ScrollbarRounding = 10.0f;
    style.GrabRounding      = 999.0f;
    style.TabRounding       = 7.0f;

    style.WindowPadding     = ImVec2(14.0f, 14.0f);
    style.FramePadding      = ImVec2(11.0f, 7.0f);
    style.CellPadding       = ImVec2(10.0f, 7.0f);
    style.ItemSpacing       = ImVec2(10.0f, 9.0f);
    style.ItemInnerSpacing  = ImVec2(8.0f, 6.0f);
    style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
    style.IndentSpacing     = 18.0f;
    style.ScrollbarSize     = 11.0f;
    style.GrabMinSize       = 12.0f;

    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.PopupBorderSize   = 1.0f;
    style.TabBorderSize     = 0.0f;
    style.WindowTitleAlign  = ImVec2(0.5f, 0.5f);
    style.WindowMenuButtonPosition = ImGuiDir_None;

    // Aliases keep the ImGui color block readable.
    const ImVec4 bgDeep     = Color(Palette::BgDeep);
    const ImVec4 bgWindow   = Color(Palette::BgWindow);
    const ImVec4 bgPanel    = Color(Palette::BgPanel);
    const ImVec4 bgPanelHi  = Color(Palette::BgPanelHi);
    const ImVec4 bgInput    = Color(Palette::BgInput);
    const ImVec4 bgInputHi  = Color(Palette::BgInputHi);
    const ImVec4 amber      = Color(Palette::Amber);
    const ImVec4 amberHi    = Color(Palette::AmberHi);
    const ImVec4 amberDim   = Color(Palette::AmberDim);
    const ImVec4 amberSoft  = Color(Palette::AmberSoft);
    const ImVec4 coral      = Color(Palette::Coral);
    const ImVec4 textMain   = Color(Palette::TextMain);
    const ImVec4 textDim    = Color(Palette::TextDim);
    const ImVec4 border     = Color(Palette::Border);

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text]                  = textMain;
    c[ImGuiCol_TextDisabled]          = textDim;
    c[ImGuiCol_WindowBg]              = bgWindow;
    c[ImGuiCol_ChildBg]               = bgPanel;
    c[ImGuiCol_PopupBg]               = Color(Palette::BgPopup);
    c[ImGuiCol_Border]                = border;
    c[ImGuiCol_BorderShadow]          = Color(0x00000000);

    c[ImGuiCol_FrameBg]               = bgInput;
    c[ImGuiCol_FrameBgHovered]        = bgInputHi;
    c[ImGuiCol_FrameBgActive]         = Color(0x3A2B10FF);

    c[ImGuiCol_TitleBg]               = bgDeep;
    c[ImGuiCol_TitleBgActive]         = bgPanel;
    c[ImGuiCol_TitleBgCollapsed]      = bgDeep;

    c[ImGuiCol_Button]                = Color(0x17181BFF);
    c[ImGuiCol_ButtonHovered]         = Color(0x2B2416FF);
    c[ImGuiCol_ButtonActive]          = amberDim;

    c[ImGuiCol_CheckMark]             = amberHi;
    c[ImGuiCol_SliderGrab]            = amber;
    c[ImGuiCol_SliderGrabActive]      = Color(Palette::BrandText);

    c[ImGuiCol_Header]                = bgPanelHi;
    c[ImGuiCol_HeaderHovered]         = Color(0x2B2416FF);
    c[ImGuiCol_HeaderActive]          = Color(0x3A2B10FF);

    c[ImGuiCol_Separator]             = Color(0x2D2A22FF);
    c[ImGuiCol_SeparatorHovered]      = amberDim;
    c[ImGuiCol_SeparatorActive]       = amber;
    c[ImGuiCol_ResizeGrip]            = Color(0xFFB80033);
    c[ImGuiCol_ResizeGripHovered]     = Color(0xFFB80088);
    c[ImGuiCol_ResizeGripActive]      = amber;

    c[ImGuiCol_Tab]                   = bgPanel;
    c[ImGuiCol_TabHovered]            = Color(0x2B2416FF);
    c[ImGuiCol_TabActive]             = Color(0x332713FF);
    c[ImGuiCol_TabUnfocused]          = bgPanel;
    c[ImGuiCol_TabUnfocusedActive]    = Color(0x2A2113FF);

    c[ImGuiCol_TableHeaderBg]         = Color(0x1B1C20FF);
    c[ImGuiCol_TableBorderStrong]     = Color(0x3A3324FF);
    c[ImGuiCol_TableBorderLight]      = border;
    c[ImGuiCol_TableRowBg]            = Color(0x11192300);
    c[ImGuiCol_TableRowBgAlt]         = Color(0xFFFFFF05);

    c[ImGuiCol_ScrollbarBg]           = Color(0x0B1018FF);
    c[ImGuiCol_ScrollbarGrab]         = border;
    c[ImGuiCol_ScrollbarGrabHovered]  = Color(0x4A3515FF);
    c[ImGuiCol_ScrollbarGrabActive]   = amberDim;

    c[ImGuiCol_PlotLines]             = amber;
    c[ImGuiCol_PlotLinesHovered]      = amberHi;
    c[ImGuiCol_PlotHistogram]         = Color(Palette::BrandText);
    c[ImGuiCol_PlotHistogramHovered]  = coral;

    c[ImGuiCol_TextSelectedBg]        = Color(0xFFB80044);
    c[ImGuiCol_DragDropTarget]        = Color(Palette::BrandText);
    c[ImGuiCol_NavHighlight]          = Color(0xFFB80099);
    c[ImGuiCol_NavWindowingHighlight] = Color(0xE7F1F755);
    c[ImGuiCol_ModalWindowDimBg]      = Color(0x00000099);

    (void)amberSoft;
    (void)coral;
}

} // namespace fc
