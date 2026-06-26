// ============================================================================
//  Theme.cpp - compact dark Byster-style skin for the ImGui overlay.
// ============================================================================

#include "Theme.h"
#include "imgui.h"

namespace fc {

static ImVec4 RGBA(unsigned int hex)
{
    return ImVec4(
        ((hex >> 24) & 0xFF) / 255.0f,
        ((hex >> 16) & 0xFF) / 255.0f,
        ((hex >> 8)  & 0xFF) / 255.0f,
        ((hex)       & 0xFF) / 255.0f);
}

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

    const ImVec4 bgDeep     = RGBA(0x0A0F16FF);
    const ImVec4 bgWindow   = RGBA(0x0D131DFF);
    const ImVec4 bgPanel    = RGBA(0x111923FF);
    const ImVec4 bgPanelHi  = RGBA(0x1B1C20FF);
    const ImVec4 bgInput    = RGBA(0x101114FF);
    const ImVec4 bgInputHi  = RGBA(0x23201AFF);
    const ImVec4 cyan       = RGBA(0xFFB800FF);
    const ImVec4 cyanHi     = RGBA(0xFFD56BFF);
    const ImVec4 cyanDim    = RGBA(0xA66F00FF);
    const ImVec4 amber      = RGBA(0xF2C25AFF);
    const ImVec4 coral      = RGBA(0xFF7A66FF);
    const ImVec4 textMain   = RGBA(0xE7F1F7FF);
    const ImVec4 textSoft   = RGBA(0xA9B7C3FF);
    const ImVec4 textDim    = RGBA(0x6F8190FF);
    const ImVec4 border     = RGBA(0x2B2C31FF);

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text]                  = textMain;
    c[ImGuiCol_TextDisabled]          = textDim;
    c[ImGuiCol_WindowBg]              = bgWindow;
    c[ImGuiCol_ChildBg]               = bgPanel;
    c[ImGuiCol_PopupBg]               = RGBA(0x101722FA);
    c[ImGuiCol_Border]                = border;
    c[ImGuiCol_BorderShadow]          = RGBA(0x00000000);

    c[ImGuiCol_FrameBg]               = bgInput;
    c[ImGuiCol_FrameBgHovered]        = bgInputHi;
    c[ImGuiCol_FrameBgActive]         = RGBA(0x3A2B10FF);

    c[ImGuiCol_TitleBg]               = bgDeep;
    c[ImGuiCol_TitleBgActive]         = bgPanel;
    c[ImGuiCol_TitleBgCollapsed]      = bgDeep;

    c[ImGuiCol_Button]                = RGBA(0x17181BFF);
    c[ImGuiCol_ButtonHovered]         = RGBA(0x2B2416FF);
    c[ImGuiCol_ButtonActive]          = cyanDim;

    c[ImGuiCol_CheckMark]             = cyanHi;
    c[ImGuiCol_SliderGrab]            = cyan;
    c[ImGuiCol_SliderGrabActive]      = amber;

    c[ImGuiCol_Header]                = bgPanelHi;
    c[ImGuiCol_HeaderHovered]         = RGBA(0x2B2416FF);
    c[ImGuiCol_HeaderActive]          = RGBA(0x3A2B10FF);

    c[ImGuiCol_Separator]             = RGBA(0x2D2A22FF);
    c[ImGuiCol_SeparatorHovered]      = cyanDim;
    c[ImGuiCol_SeparatorActive]       = cyan;
    c[ImGuiCol_ResizeGrip]            = RGBA(0xFFB80033);
    c[ImGuiCol_ResizeGripHovered]     = RGBA(0xFFB80088);
    c[ImGuiCol_ResizeGripActive]      = cyan;

    c[ImGuiCol_Tab]                   = bgPanel;
    c[ImGuiCol_TabHovered]            = RGBA(0x2B2416FF);
    c[ImGuiCol_TabActive]             = RGBA(0x332713FF);
    c[ImGuiCol_TabUnfocused]          = bgPanel;
    c[ImGuiCol_TabUnfocusedActive]    = RGBA(0x2A2113FF);

    c[ImGuiCol_TableHeaderBg]         = RGBA(0x1B1C20FF);
    c[ImGuiCol_TableBorderStrong]     = RGBA(0x3A3324FF);
    c[ImGuiCol_TableBorderLight]      = RGBA(0x2B2C31FF);
    c[ImGuiCol_TableRowBg]            = RGBA(0x11192300);
    c[ImGuiCol_TableRowBgAlt]         = RGBA(0xFFFFFF05);

    c[ImGuiCol_ScrollbarBg]           = RGBA(0x0B1018FF);
    c[ImGuiCol_ScrollbarGrab]         = RGBA(0x2B2C31FF);
    c[ImGuiCol_ScrollbarGrabHovered]  = RGBA(0x4A3515FF);
    c[ImGuiCol_ScrollbarGrabActive]   = cyanDim;

    c[ImGuiCol_PlotLines]             = cyan;
    c[ImGuiCol_PlotLinesHovered]      = cyanHi;
    c[ImGuiCol_PlotHistogram]         = amber;
    c[ImGuiCol_PlotHistogramHovered]  = coral;

    c[ImGuiCol_TextSelectedBg]        = RGBA(0xFFB80044);
    c[ImGuiCol_DragDropTarget]        = amber;
    c[ImGuiCol_NavHighlight]          = RGBA(0xFFB80099);
    c[ImGuiCol_NavWindowingHighlight] = RGBA(0xE7F1F755);
    c[ImGuiCol_ModalWindowDimBg]      = RGBA(0x00000099);

    (void)textSoft;
}

} // namespace fc
