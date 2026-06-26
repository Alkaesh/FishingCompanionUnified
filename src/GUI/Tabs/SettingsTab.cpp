// ============================================================================
//  SettingsTab.cpp - hotkeys and overlay appearance.
// ============================================================================

#include "SettingsTab.h"
#include "../../Core/Input.h"
#include "../../Features/KeyBinder.h"

#include "imgui.h"

namespace {

ImVec4 RGBA(unsigned int hex)
{
    return ImVec4(
        ((hex >> 24) & 0xFF) / 255.0f,
        ((hex >> 16) & 0xFF) / 255.0f,
        ((hex >> 8)  & 0xFF) / 255.0f,
        ((hex)       & 0xFF) / 255.0f);
}

bool BeginCard(const char* id, const ImVec2& size)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, RGBA(0x121416F5));
    ImGui::PushStyleColor(ImGuiCol_Border, RGBA(0x2B2C31FF));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(13.0f, 12.0f));

#if IMGUI_VERSION_NUM >= 19000
    return ImGui::BeginChild(
        id,
        size,
        ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
#else
    return ImGui::BeginChild(
        id,
        size,
        true,
        ImGuiWindowFlags_AlwaysUseWindowPadding |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse);
#endif
}

void EndCard()
{
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

} // namespace

namespace fc {

void SettingsTab::Render()
{
    ImGui::SeparatorText("Hotkeys");

    if (BeginCard("##hotkey_card", ImVec2(0.0f, 118.0f)))
    {
        KeyBinder::Draw("Open/close menu", &Input::ToggleKey());
        KeyBinder::Draw("Unload module", &Input::UnloadKey());
    }
    EndCard();

    ImGui::Spacing();
    ImGui::SeparatorText("Appearance");

    if (BeginCard("##appearance_card", ImVec2(0.0f, 128.0f)))
    {
        static float uiScale = ImGui::GetIO().FontGlobalScale;
        ImGui::SetNextItemWidth(240.0f);
        if (ImGui::SliderFloat("Interface scale", &uiScale, 0.8f, 1.6f, "%.2f"))
            ImGui::GetIO().FontGlobalScale = uiScale;

        ImGui::SameLine();
        if (ImGui::Button("Reset", ImVec2(82.0f, 0.0f)))
        {
            uiScale = 1.0f;
            ImGui::GetIO().FontGlobalScale = uiScale;
        }

        ImGui::Spacing();
        ImGui::TextColored(RGBA(0x7F91A0FF), "Menu: %s   Unload: %s",
                           KeyBinder::KeyName(Input::ToggleKey()),
                           KeyBinder::KeyName(Input::UnloadKey()));
    }
    EndCard();
}

} // namespace fc
