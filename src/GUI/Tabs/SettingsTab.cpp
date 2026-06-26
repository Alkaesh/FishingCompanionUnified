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
    ImGui::PushStyleColor(ImGuiCol_ChildBg, RGBA(0x172131F5));
    ImGui::PushStyleColor(ImGuiCol_Border, RGBA(0x2F3D4EFF));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(13.0f, 12.0f));

#if IMGUI_VERSION_NUM >= 19000
    return ImGui::BeginChild(id, size, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
#else
    return ImGui::BeginChild(id, size, true, ImGuiWindowFlags_AlwaysUseWindowPadding);
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
    ImGui::SeparatorText("Горячие клавиши");

    if (BeginCard("##hotkey_card", ImVec2(0.0f, 118.0f)))
    {
        KeyBinder::Draw("Открыть/закрыть меню", &Input::ToggleKey());
        KeyBinder::Draw("Выгрузить модуль", &Input::UnloadKey());
    }
    EndCard();

    ImGui::Spacing();
    ImGui::SeparatorText("Видимость HUD");

    if (BeginCard("##hud_hotkey_card", ImVec2(0.0f, 76.0f)))
    {
        static int hudToggleKey = 0;
        KeyBinder::Draw("Скрыть/показать HUD", &hudToggleKey);
    }
    EndCard();

    ImGui::Spacing();
    ImGui::SeparatorText("Оформление");

    if (BeginCard("##appearance_card", ImVec2(0.0f, 128.0f)))
    {
        static float uiScale = ImGui::GetIO().FontGlobalScale;
        ImGui::SetNextItemWidth(240.0f);
        if (ImGui::SliderFloat("Масштаб интерфейса", &uiScale, 0.8f, 1.6f, "%.2f"))
            ImGui::GetIO().FontGlobalScale = uiScale;

        ImGui::SameLine();
        if (ImGui::Button("Сброс", ImVec2(82.0f, 0.0f)))
        {
            uiScale = 1.0f;
            ImGui::GetIO().FontGlobalScale = uiScale;
        }

        ImGui::Spacing();
        ImGui::TextColored(RGBA(0x7F91A0FF), "Меню: %s   Выгрузка: %s",
                           KeyBinder::KeyName(Input::ToggleKey()),
                           KeyBinder::KeyName(Input::UnloadKey()));
    }
    EndCard();
}

} // namespace fc
