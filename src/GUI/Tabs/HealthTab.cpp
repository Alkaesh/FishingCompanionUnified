// ============================================================================
//  HealthTab.cpp - compact runtime and SDK diagnostics.
// ============================================================================

#include "HealthTab.h"

#include "../UI.h"
#include "../../Actions/ActionRuntime.h"
#include "../../SDK/ModuleLoader.h"

#include "imgui.h"

#include <cstdio>
#include <vector>

namespace ui = fc::gui::ui;

namespace fc {

void HealthTab::Render()
{
    const actions::Status runtime = actions::GetStatus();
    const auto& loader = sdk::ModuleLoader::Get();
    const std::vector<sdk::ModuleLoader::ModuleRecord> moduleRecords = loader.ModuleRecords();
    const std::vector<sdk::ModuleLoader::Event> events = loader.Events();

    int loadedCount = 0;
    int failedCount = 0;
    int infoCount = 0;
    int warnCount = 0;
    int errorCount = 0;

    for (const sdk::ModuleLoader::ModuleRecord& record : moduleRecords)
    {
        if (record.loaded)
            ++loadedCount;
        else
            ++failedCount;
    }

    for (const sdk::ModuleLoader::Event& event : events)
    {
        if (event.level == "error")
            ++errorCount;
        else if (event.level == "warn")
            ++warnCount;
        else
            ++infoCount;
    }

    char queueValue[32];
    char modulesValue[48];
    char eventValue[64];
    char snapshotValue[48];
    std::snprintf(queueValue, sizeof(queueValue), "%u queued", runtime.queued);
    std::snprintf(modulesValue, sizeof(modulesValue), "%d loaded / %d failed", loadedCount, failedCount);
    std::snprintf(eventValue, sizeof(eventValue), "%d info / %d warn / %d error", infoCount, warnCount, errorCount);
    std::snprintf(snapshotValue, sizeof(snapshotValue), "%llu snapshots", runtime.diagnostic_snapshots);

    ImGui::SeparatorText("Health");

    const float gap = 10.0f;
    const float available = ImGui::GetContentRegionAvail().x;
    const bool twoColumns = available >= 430.0f;
    const float cardWidth = twoColumns ? (available - gap) * 0.5f : available;
    const ImVec2 cardSize(cardWidth, 68.0f);

    const char* runtimeState = runtime.busy ? "busy" : (runtime.ready ? "ready" : "waiting");
    if (ui::BeginStatCard("##health_runtime", cardSize))
    {
        ui::Metric(
            "Action runtime",
            runtimeState,
            runtime.ready ? Color(Palette::Amber) : Color(Palette::Coral));
    }
    ui::EndCard();

    if (twoColumns)
        ImGui::SameLine(0.0f, gap);
    if (ui::BeginStatCard("##health_queue", cardSize))
        ui::Metric("Queue", queueValue, runtime.queued == 0 ? Color(Palette::TextSoft) : Color(Palette::AmberHi));
    ui::EndCard();

    if (ui::BeginStatCard("##health_modules", cardSize))
        ui::Metric("SDK modules", modulesValue, failedCount == 0 ? Color(Palette::Amber) : Color(Palette::Coral));
    ui::EndCard();

    if (twoColumns)
        ImGui::SameLine(0.0f, gap);
    if (ui::BeginStatCard("##health_logs", cardSize))
        ui::Metric("Loader events", eventValue, errorCount == 0 ? Color(Palette::TextSoft) : Color(Palette::Coral));
    ui::EndCard();

    ImGui::Spacing();
    ImGui::SeparatorText("Runtime Details");
    if (ui::BeginCard("##health_runtime_details", ImVec2(0.0f, 162.0f)))
    {
        ui::StatusLine("Ready", runtime.ready ? "yes" : "no", runtime.ready ? Color(Palette::Amber) : Color(Palette::Coral));
        ui::StatusLine("Busy", runtime.busy ? "yes" : "no", runtime.busy ? Color(Palette::AmberHi) : Color(Palette::TextSoft));
        ui::StatusLine(
            "Diagnostics",
            runtime.diagnostics_enabled ? "enabled" : "off",
            runtime.diagnostics_enabled ? Color(Palette::Amber) : Color(Palette::TextFaint));
        ui::StatusLine(
            "Auto reel",
            runtime.auto_reel_enabled ? "enabled" : "off",
            runtime.auto_reel_enabled ? Color(Palette::Amber) : Color(Palette::TextFaint));
        ui::StatusLine("Snapshots", snapshotValue, Color(Palette::TextSoft));

        if (!runtime.message.empty())
        {
            ImGui::Spacing();
            ImGui::TextWrapped("%s", runtime.message.c_str());
        }
    }
    ui::EndCard();

    ImGui::Spacing();
    ImGui::SeparatorText("Last Signal");
    if (ui::BeginCard("##health_last_signal", ImVec2(0.0f, 134.0f)))
    {
        if (runtime.last_command.empty())
        {
            ImGui::TextColored(Color(Palette::TextMuted), "No command result yet.");
        }
        else
        {
            ImGui::TextColored(
                runtime.last_effect_confirmed ? Color(Palette::Amber) : Color(Palette::AmberSoft),
                "Command: %s / %s",
                runtime.last_command.c_str(),
                runtime.last_result.c_str());
        }

        if (!runtime.last_observation.empty())
        {
            ImGui::Spacing();
            ImGui::TextWrapped("%s", runtime.last_observation.c_str());
        }
    }
    ui::EndCard();

    ImGui::Spacing();
    ImGui::SeparatorText("Paths");
    if (ui::BeginCard("##health_paths", ImVec2(0.0f, 132.0f)))
    {
        const std::string processDirectory = ui::ProcessDirectory();
        const std::string modsDirectory = ui::Narrow(loader.ModsDirectory());

        ui::WrappedStatusLine("Process dir", processDirectory.empty() ? "unknown" : processDirectory.c_str(), Color(Palette::TextSoft));
        ui::WrappedStatusLine("Mods dir", modsDirectory.empty() ? "not scanned" : modsDirectory.c_str(), Color(Palette::TextSoft));
        ui::StatusLine("SDK scanned", loader.HasLoaded() ? "yes" : "no", loader.HasLoaded() ? Color(Palette::Amber) : Color(Palette::AmberSoft));
    }
    ui::EndCard();
}

} // namespace fc
