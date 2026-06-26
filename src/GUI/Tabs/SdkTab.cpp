// ============================================================================
//  SdkTab.cpp - built-in SDK console.
// ============================================================================

#include "SdkTab.h"
#include "../../SDK/FCSDK.h"
#include "../../SDK/ModuleLoader.h"

#include "imgui.h"

#include <cstdio>
#include <string>
#include <vector>

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

void StatusLine(const char* label, const char* value, const ImVec4& color)
{
    ImGui::TextColored(RGBA(0x7F91A0FF), "%s", label);
    ImGui::SameLine(180.0f);
    ImGui::TextColored(color, "%s", value);
}

std::string Narrow(const std::wstring& value)
{
    if (value.empty())
        return {};

    const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1)
        return {};

    std::string result(static_cast<size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), size, nullptr, nullptr);
    return result;
}

ImVec4 EventColor(const std::string& level)
{
    if (level == "error")
        return RGBA(0xFF7A66FF);
    if (level == "warn")
        return RGBA(0xFFCF66FF);
    return RGBA(0xFFB800FF);
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

    if (BeginCard("##sdk_status", ImVec2(0.0f, 150.0f)))
    {
        const std::string modsPath = Narrow(loader.ModsDirectory());

        StatusLine("Version", FCSDK_GetVersionString(), RGBA(0xFFB800FF));
        StatusLine("ABI", "C exports + C++ helpers", RGBA(0xFFCF66FF));
        StatusLine("ImGui", FCSDK_GetImGuiVersion(), RGBA(0xFFD56BFF));
        StatusLine("Modules", modulesSummary, failedCount > 0 ? RGBA(0xFF7A66FF) : RGBA(0xFFB800FF));
        StatusLine("Mods", modsPath.empty() ? "mods/" : modsPath.c_str(), RGBA(0xFF7A66FF));
    }
    EndCard();

    ImGui::Spacing();
    ImGui::SeparatorText("Module Quickstart");

    if (BeginCard("##sdk_quickstart", ImVec2(0.0f, 178.0f)))
    {
        ImGui::TextWrapped("Include src/SDK/FCSDK.h, fill FCSDK_TabDesc, and call FCSDK_RegisterTab(). "
                           "The render callback runs inside the active host ImGui frame, so a module can draw "
                           "its controls directly or use helpers from FCSDK_UI.h.");
        ImGui::Spacing();
        ImGui::TextColored(RGBA(0x7F91A0FF), "Minimal flow");
        ImGui::BulletText("include FCSDK.h");
        ImGui::BulletText("implement void Render(void*)");
        ImGui::BulletText("call FCSDK_RegisterTab(&desc)");
    }
    EndCard();

    ImGui::Spacing();
    ImGui::SeparatorText("Loaded Modules");
    if (BeginCard("##sdk_modules", ImVec2(0.0f, 190.0f)))
    {
        if (moduleRecords.empty())
        {
            ImGui::TextColored(RGBA(0x7F91A0FF), "No external modules loaded yet.");
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
                    record.loaded ? RGBA(0xFFB800FF) : RGBA(0xFF7A66FF),
                    "%s",
                    record.loaded ? "loaded" : "failed");
                ImGui::TableNextColumn();
                ImGui::TextWrapped("%s", Narrow(record.name).c_str());
                ImGui::TableNextColumn();
                ImGui::TextWrapped("%s", record.detail.c_str());
            }

            ImGui::EndTable();
        }
    }
    EndCard();

    ImGui::Spacing();
    ImGui::SeparatorText("Loader Events");
    if (BeginCard("##sdk_loader_events", ImVec2(0.0f, 170.0f)))
    {
        if (events.empty())
        {
            ImGui::TextColored(RGBA(0x7F91A0FF), "No loader events yet.");
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
    EndCard();

    ImGui::Spacing();
    ImGui::TextColored(RGBA(0x7F91A0FF), "Safety scope: UI, overlays, allowed data sources. No anti-cheat bypass helpers.");
}

} // namespace fc
