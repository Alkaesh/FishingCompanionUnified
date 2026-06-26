// ============================================================================
//  DashboardTab.cpp - live overview for runtime, SDK, and hotkeys.
// ============================================================================

#include "DashboardTab.h"

#include "../../Actions/ActionRuntime.h"
#include "../../Core/Input.h"
#include "../../Features/KeyBinder.h"
#include "../../SDK/ModuleLoader.h"

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

void Metric(const char* label, const char* value, const ImVec4& color)
{
    ImGui::TextColored(RGBA(0x7F838CFF), "%s", label);
    ImGui::Spacing();
    ImGui::TextColored(color, "%s", value);
}

} // namespace

namespace fc {

void DashboardTab::Render()
{
    const actions::Status runtime = actions::GetStatus();
    const auto& loader = sdk::ModuleLoader::Get();

    int loadedModules = 0;
    int failedModules = 0;
    for (const sdk::ModuleLoader::ModuleRecord& record : loader.ModuleRecords())
    {
        if (record.loaded)
            ++loadedModules;
        else
            ++failedModules;
    }

    char queueValue[32];
    char moduleValue[32];
    std::snprintf(queueValue, sizeof(queueValue), "%u queued", runtime.queued);
    std::snprintf(moduleValue, sizeof(moduleValue), "%d / %d", loadedModules, failedModules);

    ImGui::SeparatorText("Overview");

    const float gap = 10.0f;
    const float available = ImGui::GetContentRegionAvail().x;
    const bool twoColumns = available >= 430.0f;
    const float cardWidth = twoColumns ? (available - gap) * 0.5f : available;
    const ImVec2 cardSize(cardWidth, 68.0f);

    const char* runtimeState = runtime.busy ? "busy" : (runtime.ready ? "ready" : "waiting");
    if (BeginCard("##runtime_card", cardSize))
    {
        Metric(
            "Action runtime",
            runtimeState,
            runtime.ready ? RGBA(0xFFB800FF) : RGBA(0xFF7A66FF));
    }
    EndCard();

    if (twoColumns)
        ImGui::SameLine(0.0f, gap);
    if (BeginCard("##queue_card", cardSize))
        Metric("Command queue", queueValue, runtime.queued == 0 ? RGBA(0xB9BBC0FF) : RGBA(0xFFD56BFF));
    EndCard();

    if (BeginCard("##modules_card", cardSize))
        Metric("SDK modules loaded / failed", moduleValue, failedModules == 0 ? RGBA(0xFFB800FF) : RGBA(0xFF7A66FF));
    EndCard();

    if (twoColumns)
        ImGui::SameLine(0.0f, gap);
    if (BeginCard("##diagnostics_card", cardSize))
    {
        Metric(
            "Diagnostics",
            runtime.diagnostics_enabled ? "enabled" : "off",
            runtime.diagnostics_enabled ? RGBA(0xFFB800FF) : RGBA(0x7F838CFF));
    }
    EndCard();

    ImGui::Spacing();
    ImGui::SeparatorText("Current State");

    if (BeginCard("##state_card", ImVec2(0.0f, 112.0f)))
    {
        ImGui::TextColored(RGBA(0x7F838CFF), "Message");
        ImGui::TextWrapped("%s", runtime.message.empty() ? "No runtime message yet." : runtime.message.c_str());

        if (!runtime.last_command.empty())
        {
            ImGui::Spacing();
            ImGui::TextColored(
                runtime.last_effect_confirmed ? RGBA(0xFFB800FF) : RGBA(0xFFD56BFF),
                "Last command: %s / %s",
                runtime.last_command.c_str(),
                runtime.last_result.c_str());
        }
    }
    EndCard();

    ImGui::Spacing();
    ImGui::SeparatorText("Hotkeys");

    if (BeginCard("##hotkeys_card", ImVec2(0.0f, 64.0f)))
    {
        ImGui::TextColored(
            RGBA(0xB9BBC0FF),
            "Menu: %s    Unload: %s",
            KeyBinder::KeyName(Input::ToggleKey()),
            KeyBinder::KeyName(Input::UnloadKey()));
        ImGui::TextColored(RGBA(0x7F838CFF), "Configure these in Settings.");
    }
    EndCard();
}

} // namespace fc
