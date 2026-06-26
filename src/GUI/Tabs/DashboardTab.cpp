// ============================================================================
//  DashboardTab.cpp - live overview for runtime, SDK, and hotkeys.
// ============================================================================

#include "DashboardTab.h"

#include "../UI.h"
#include "../../Actions/ActionRuntime.h"
#include "../../Core/Input.h"
#include "../../Features/KeyBinder.h"
#include "../../SDK/ModuleLoader.h"

#include "imgui.h"

#include <cstdio>
#include <vector>

namespace ui = fc::gui::ui;

namespace fc {

void DashboardTab::Render()
{
    const actions::Status runtime = actions::GetStatus();
    const auto& loader = sdk::ModuleLoader::Get();
    const std::vector<sdk::ModuleLoader::ModuleRecord> moduleRecords = loader.ModuleRecords();

    int loadedModules = 0;
    int failedModules = 0;
    for (const sdk::ModuleLoader::ModuleRecord& record : moduleRecords)
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
    if (ui::BeginStatCard("##runtime_card", cardSize))
    {
        ui::Metric(
            "Action runtime",
            runtimeState,
            runtime.ready ? Color(Palette::Amber) : Color(Palette::Coral));
    }
    ui::EndCard();

    if (twoColumns)
        ImGui::SameLine(0.0f, gap);
    if (ui::BeginStatCard("##queue_card", cardSize))
        ui::Metric("Command queue", queueValue, runtime.queued == 0 ? Color(Palette::TextSoft) : Color(Palette::AmberHi));
    ui::EndCard();

    if (ui::BeginStatCard("##modules_card", cardSize))
        ui::Metric("SDK modules loaded / failed", moduleValue, failedModules == 0 ? Color(Palette::Amber) : Color(Palette::Coral));
    ui::EndCard();

    if (twoColumns)
        ImGui::SameLine(0.0f, gap);
    if (ui::BeginStatCard("##diagnostics_card", cardSize))
    {
        ui::Metric(
            "Diagnostics",
            runtime.diagnostics_enabled ? "enabled" : "off",
            runtime.diagnostics_enabled ? Color(Palette::Amber) : Color(Palette::TextFaint));
    }
    ui::EndCard();

    ImGui::Spacing();
    ImGui::SeparatorText("Current State");

    if (ui::BeginCard("##state_card", ImVec2(0.0f, 112.0f)))
    {
        ImGui::TextColored(Color(Palette::TextFaint), "Message");
        ImGui::TextWrapped("%s", runtime.message.empty() ? "No runtime message yet." : runtime.message.c_str());

        if (!runtime.last_command.empty())
        {
            ImGui::Spacing();
            ImGui::TextColored(
                runtime.last_effect_confirmed ? Color(Palette::Amber) : Color(Palette::AmberHi),
                "Last command: %s / %s",
                runtime.last_command.c_str(),
                runtime.last_result.c_str());
        }
    }
    ui::EndCard();

    ImGui::Spacing();
    ImGui::SeparatorText("Hotkeys");

    if (ui::BeginCard("##hotkeys_card", ImVec2(0.0f, 64.0f)))
    {
        ImGui::TextColored(
            Color(Palette::TextSoft),
            "Menu: %s    Unload: %s",
            KeyBinder::KeyName(Input::ToggleKey()),
            KeyBinder::KeyName(Input::UnloadKey()));
        ImGui::TextColored(Color(Palette::TextFaint), "Configure these in Settings.");
    }
    ui::EndCard();
}

} // namespace fc
