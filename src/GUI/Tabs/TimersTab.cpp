// ============================================================================
//  TimersTab.cpp - reminder tuning.
// ============================================================================

#include "TimersTab.h"
#include "../../Features/Statistics.h"

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

void TimersTab::Render()
{
    Statistics& stats = Statistics::Get();

    ImGui::SeparatorText("Напоминания");

    if (BeginCard("##timers_card", ImVec2(0.0f, 190.0f)))
    {
        const float sliderWidth = ImGui::GetContentRegionAvail().x * 0.58f;
        const float controlWidth = sliderWidth < 220.0f ? 220.0f : sliderWidth;

        ImGui::SetNextItemWidth(controlWidth);
        ImGui::SliderInt("Таймер сессии (мин)", &stats.sessionTimerMin, 5, 240);
        ImGui::ProgressBar(stats.sessionTimerMin / 240.0f, ImVec2(-1.0f, 7.0f), "");

        ImGui::Spacing();
        ImGui::SetNextItemWidth(controlWidth);
        ImGui::SliderInt("Интервал уведомлений (сек)", &stats.notifyIntervalSec, 10, 600);
        ImGui::ProgressBar(stats.notifyIntervalSec / 600.0f, ImVec2(-1.0f, 7.0f), "");

        ImGui::Spacing();
        ImGui::SetNextItemWidth(controlWidth);
        ImGui::SliderFloat("Громкость уведомлений", &stats.notifyVolume, 0.0f, 1.0f, "%.2f");
        ImGui::ProgressBar(stats.notifyVolume, ImVec2(-1.0f, 7.0f), "");
    }
    EndCard();

    ImGui::Spacing();
    ImGui::SeparatorText("Поведение");

    if (BeginCard("##timer_behavior_card", ImVec2(0.0f, 72.0f)))
        ImGui::Checkbox("Напоминать о перерыве", &stats.breakReminder);
    EndCard();
}

} // namespace fc
