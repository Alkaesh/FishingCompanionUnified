// ============================================================================
//  DashboardTab.cpp - session overview and HUD toggles.
// ============================================================================

#include "DashboardTab.h"
#include "../../Features/Statistics.h"

#include "imgui.h"

#include <cstdio>

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

void MetricCard(const char* id, const char* label, const char* value, const ImVec4& accent, const ImVec2& size)
{
    if (BeginCard(id, size))
    {
        ImGui::TextColored(RGBA(0x7F91A0FF), "%s", label);
        ImGui::Spacing();
        ImGui::TextColored(accent, "%s", value);
    }
    EndCard();
}

} // namespace

namespace fc {

void DashboardTab::Render()
{
    Statistics& stats = Statistics::Get();

    ImGui::SeparatorText("Текущая сессия");

    char fish[32];
    char weight[32];
    char best[32];
    char time[32];
    std::snprintf(fish, sizeof(fish), "%d", stats.fishCaught);
    std::snprintf(weight, sizeof(weight), "%.2f кг", stats.currentWeight);
    std::snprintf(best, sizeof(best), "%.2f кг", stats.bestCatch);
    std::snprintf(time, sizeof(time), "%02d:%02d", stats.sessionMinutes / 60, stats.sessionMinutes % 60);

    const float gap = 10.0f;
    const float available = ImGui::GetContentRegionAvail().x;
    const bool twoColumns = available >= 410.0f;
    const float cardWidth = twoColumns ? (available - gap) * 0.5f : available;
    const ImVec2 cardSize(cardWidth, 74.0f);

    MetricCard("##fish_card", "Поймано рыбы", fish, RGBA(0x31D3C6FF), cardSize);
    if (twoColumns) ImGui::SameLine(0.0f, gap);
    MetricCard("##weight_card", "Текущий вес", weight, RGBA(0xFFCF66FF), cardSize);

    MetricCard("##best_card", "Лучший улов", best, RGBA(0x7FF4EAFF), cardSize);
    if (twoColumns) ImGui::SameLine(0.0f, gap);
    MetricCard("##time_card", "Время сессии", time, RGBA(0xFF7A66FF), cardSize);

    ImGui::Spacing();
    ImGui::SeparatorText("Элементы HUD");

    if (BeginCard("##hud_card", ImVec2(0.0f, 126.0f)))
    {
        ImGui::Checkbox("Показывать радар", &stats.showRadar);
        ImGui::Checkbox("Звуковые уведомления", &stats.soundAlerts);
        ImGui::Checkbox("Показывать счётчик улова", &stats.showCatchCounter);
    }
    EndCard();

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, RGBA(0x1C5A62FF));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, RGBA(0x267B82FF));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, RGBA(0x31D3C6FF));
    if (ImGui::Button("Обновить сейчас", ImVec2(180.0f, 34.0f)))
        stats.Update();
    ImGui::PopStyleColor(3);
}

} // namespace fc
