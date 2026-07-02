// ============================================================================
//  Theme - host palette and ImGui skin for the Byster overlay.
// ----------------------------------------------------------------------------
//  Palette holds the single source of host colors as RGBA hex (0xRRGGBBAA).
//  Theme::Apply() consumes them for ImGui style; fc::gui::ui helpers and any
//  host tab read colors from here so nothing hardcodes a hex literal twice.
// ============================================================================

#pragma once

#include "imgui.h"

namespace fc {

// Centralized host color palette (RGBA hex: 0xRRGGBBAA).
// Mirrors the values previously inlined in Theme.cpp and the host tabs.
namespace Palette {
    // Surfaces
    constexpr unsigned int BgDeep      = 0x0A0F16FF;
    constexpr unsigned int BgWindow    = 0x0D131DFF;
    constexpr unsigned int BgPanel     = 0x111923FF;
    constexpr unsigned int BgCard      = 0x121416F5;
    constexpr unsigned int BgPanelHi   = 0x1B1C20FF;
    constexpr unsigned int BgInput     = 0x101114FF;
    constexpr unsigned int BgInputHi   = 0x23201AFF;
    constexpr unsigned int BgShell     = 0x121315FF;
    constexpr unsigned int BgTopbar    = 0x1A1B1EFF;
    constexpr unsigned int BgContent   = 0x101113FA;
    constexpr unsigned int BgPopup     = 0x101722FA;

    // Lines / borders
    constexpr unsigned int Border       = 0x2B2C31FF;
    constexpr unsigned int BorderSubtle = 0x292A2EFF;
    constexpr unsigned int BorderInput  = 0x26272CFF;

    // Brand / accents (amber is the primary accent)
    constexpr unsigned int Amber       = 0xFFB800FF; // primary accent / positive
    constexpr unsigned int AmberHi     = 0xFFD56BFF; // bright positive
    constexpr unsigned int AmberSoft   = 0xFFCF66FF; // warm positive / warning
    constexpr unsigned int AmberDim    = 0xA66F00FF;
    constexpr unsigned int BrandText   = 0xF2C25AFF;
    constexpr unsigned int Coral       = 0xFF7A66FF; // warning / negative

    // Text
    constexpr unsigned int TextMain    = 0xE7F1F7FF;
    constexpr unsigned int TextSoft    = 0xB9BBC0FF;
    constexpr unsigned int TextMuted   = 0x7F91A0FF;
    constexpr unsigned int TextDim     = 0x6F8190FF;
    constexpr unsigned int TextFaint   = 0x7F838CFF;
    constexpr unsigned int TextLabel   = 0x7F91A0FF;

    // Caret / focus
    constexpr unsigned int Caret       = 0x62656DFF;
}

// Convert an RGBA hex (0xRRGGBBAA) to an ImVec4 color.
constexpr ImVec4 Color(unsigned int hex)
{
    return ImVec4(
        ((hex >> 24) & 0xFF) / 255.0f,
        ((hex >> 16) & 0xFF) / 255.0f,
        ((hex >> 8)  & 0xFF) / 255.0f,
        ((hex)       & 0xFF) / 255.0f);
}

// Convert an RGBA hex to a packed U32 (for ImDrawList).
inline ImU32 ColorU32(unsigned int hex)
{
    return ImGui::ColorConvertFloat4ToU32(Color(hex));
}

class Theme
{
public:
    // Apply the Byster style and palette to the current ImGui context.
    static void Apply();

    // Loads a Windows UI font with Cyrillic glyphs.
    static void LoadFonts();
};

} // namespace fc
