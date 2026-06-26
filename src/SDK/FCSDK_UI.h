// ============================================================================
//  FCSDK_UI.h - lightweight ImGui helpers for SDK modules.
// ============================================================================

#pragma once

#include "imgui.h"

namespace fc::sdk::ui {

enum class Accent
{
    Cyan,
    Amber,
    Coral,
    Muted,
    Text
};

inline ImVec4 Color(unsigned int hex)
{
    return ImVec4(
        ((hex >> 24) & 0xFF) / 255.0f,
        ((hex >> 16) & 0xFF) / 255.0f,
        ((hex >> 8)  & 0xFF) / 255.0f,
        ((hex)       & 0xFF) / 255.0f);
}

inline ImVec4 Color(Accent accent)
{
    switch (accent)
    {
    case Accent::Cyan:  return Color(0x31D3C6FF);
    case Accent::Amber: return Color(0xFFCF66FF);
    case Accent::Coral: return Color(0xFF7A66FF);
    case Accent::Text:  return Color(0xE7F1F7FF);
    case Accent::Muted:
    default:            return Color(0x7F91A0FF);
    }
}

inline bool BeginCard(const char* id, const ImVec2& size = ImVec2(0.0f, 0.0f))
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Color(0x172131F5));
    ImGui::PushStyleColor(ImGuiCol_Border, Color(0x2F3D4EFF));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(13.0f, 12.0f));

#if IMGUI_VERSION_NUM >= 19000
    return ImGui::BeginChild(id, size, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
#else
    return ImGui::BeginChild(id, size, true, ImGuiWindowFlags_AlwaysUseWindowPadding);
#endif
}

inline void EndCard()
{
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

inline void Metric(const char* label, const char* value, Accent accent = Accent::Cyan)
{
    ImGui::TextColored(Color(Accent::Muted), "%s", label);
    ImGui::TextColored(Color(accent), "%s", value);
}

inline void Section(const char* title)
{
    ImGui::SeparatorText(title);
}

} // namespace fc::sdk::ui
