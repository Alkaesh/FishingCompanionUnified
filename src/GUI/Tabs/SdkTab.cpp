// ============================================================================
//  SdkTab.cpp - built-in SDK console.
// ============================================================================

#include "SdkTab.h"
#include "../UI.h"
#include "../../SDK/FCSDK.h"
#include "../../SDK/ModuleLoader.h"

#include "imgui.h"

#include <cstdio>
#include <vector>

namespace ui = fc::gui::ui;

namespace {

ImVec4 EventColor(const std::string& level)
{
    if (level == "error")
        return fc::Color(fc::Palette::Coral);
    if (level == "warn")
        return fc::Color(fc::Palette::AmberSoft);
    return fc::Color(fc::Palette::Amber);
}

} // namespace

namespace fc {

void SdkTab::Render()
{
    const auto& loader = sdk::ModuleLoader::Get();
    const std::vector<sdk::ModuleLoader::ModuleRecord> moduleRecords = loader.ModuleRecords();
    const std::vector<sdk::ModuleLoader::Event> events = loader.Events();
    int loadedCount = 0;
    int failedCount = 0;
    for (const sdk::ModuleLoader::ModuleRecord& record : moduleRecords)
    {
        if (record.loaded)
            ++loadedCount;
        else
            ++failedCount;
    }

    char modulesSummary[64];
    std::snprintf(
        modulesSummary,
        sizeof(modulesSummary),
        "%d loaded / %d failed",
        loadedCount,
        failedCount);

    ImGui::SeparatorText("SDK Console");

    if (ui::BeginCard("##sdk_status", ImVec2(0.0f, 150.0f)))
    {
        const std::string modsPath = ui::Narrow(loader.ModsDirectory());

        // SDK uses a wider label column than the default 156.
        constexpr float kLabelW = 180.0f;
        ui::StatusLine("Version", FCSDK_GetVersionString(), Color(Palette::Amber), kLabelW);
        ui::StatusLine("ABI", "C exports + C++ helpers", Color(Palette::AmberSoft), kLabelW);
        ui::StatusLine("ImGui", FCSDK_GetImGuiVersion(), Color(Palette::AmberHi), kLabelW);
        ui::StatusLine("Modules", modulesSummary, failedCount > 0 ? Color(Palette::Coral) : Color(Palette::Amber), kLabelW);
        ui::StatusLine("Mods", modsPath.empty() ? "mods/" : modsPath.c_str(), Color(Palette::Coral), kLabelW);
    }
    ui::EndCard();

    ImGui::Spacing();
    ImGui::SeparatorText("Module Quickstart");

    if (ui::BeginCard("##sdk_quickstart", ImVec2(0.0f, 178.0f)))
    {
        ImGui::TextWrapped("Include src/SDK/FCSDK.h, fill FCSDK_TabDesc, and call FCSDK_RegisterTab(). "
                           "The render callback runs inside the active host ImGui frame, so a module can draw "
                           "its controls directly or use helpers from FCSDK_UI.h.");
        ImGui::Spacing();
        ImGui::TextColored(Color(Palette::TextMuted), "Minimal flow");
        ImGui::BulletText("include FCSDK.h");
        ImGui::BulletText("implement void Render(void*)");
        ImGui::BulletText("call FCSDK_RegisterTab(&desc)");
    }
    ui::EndCard();

    ImGui::Spacing();
    ImGui::SeparatorText("Loaded Modules");
    if (ui::BeginCard("##sdk_modules", ImVec2(0.0f, 190.0f)))
    {
        if (moduleRecords.empty())
        {
            ImGui::TextColored(Color(Palette::TextMuted), "No external modules loaded yet.");
        }
        else if (ImGui::BeginTable(
            "##sdk_modules_table",
            3,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 72.0f);
            ImGui::TableSetupColumn("Module", ImGuiTableColumnFlags_WidthStretch, 0.34f);
            ImGui::TableSetupColumn("Detail", ImGuiTableColumnFlags_WidthStretch, 0.66f);
            ImGui::TableHeadersRow();

            for (const sdk::ModuleLoader::ModuleRecord& record : moduleRecords)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextColored(
                    record.loaded ? Color(Palette::Amber) : Color(Palette::Coral),
                    "%s",
                    record.loaded ? "loaded" : "failed");
                ImGui::TableNextColumn();
                ImGui::TextWrapped("%s", ui::Narrow(record.name).c_str());
                ImGui::TableNextColumn();
                ImGui::TextWrapped("%s", record.detail.c_str());
            }

            ImGui::EndTable();
        }
    }
    ui::EndCard();

    ImGui::Spacing();
    ImGui::SeparatorText("Loader Events");
    if (ui::BeginCard("##sdk_loader_events", ImVec2(0.0f, 170.0f)))
    {
        if (events.empty())
        {
            ImGui::TextColored(Color(Palette::TextMuted), "No loader events yet.");
        }
        else
        {
            if (ImGui::BeginChild("##sdk_loader_events_scroll", ImVec2(0.0f, 0.0f), false))
            {
                for (size_t i = events.size(); i > 0; --i)
                {
                    const sdk::ModuleLoader::Event& event = events[i - 1];
                    ImGui::TextColored(EventColor(event.level), "%s", event.level.c_str());
                    ImGui::SameLine(78.0f);
                    ImGui::TextWrapped("%s", event.message.c_str());
                }
            }
            ImGui::EndChild();
        }
    }
    ui::EndCard();

    ImGui::Spacing();
    ImGui::TextColored(Color(Palette::TextMuted), "Safety scope: UI, overlays, allowed data sources. No anti-cheat bypass helpers.");
}

} // namespace fc
