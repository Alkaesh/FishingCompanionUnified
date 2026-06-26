// ============================================================================
//  Theme — кастомная тёмная тема ImGui в «водной» гамме (deep blue / teal).
// ============================================================================

#pragma once

namespace fc {

class Theme
{
public:
    // Применяет стиль и цвета к текущему контексту ImGui.
    static void Apply();

    // Loads a Windows UI font with Cyrillic glyphs.
    static void LoadFonts();
};

} // namespace fc
