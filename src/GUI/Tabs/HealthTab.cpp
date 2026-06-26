// ============================================================================
//  HealthTab.cpp - compact runtime and SDK diagnostics.
// ============================================================================

#include "HealthTab.h"

#include "../../Actions/ActionRuntime.h"
#include "../../SDK/ModuleLoader.h"

#include "imgui.h"

#include <Windows.h>

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

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

std::string CurrentProcessDirectory()
{
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0)
        return {};

    return Narrow(fs::path(buffer).parent_path().wstring());
}

void Metric(const char* label, const char* value, const ImVec4& color)
{
    ImGui::TextColored(RGBA(0x7F838CFF), "%s", label);
    ImGui::Spacing();
    ImGui::TextColored(color, "%s", value);
}

void StatusLine(const char* label, const char* value, const ImVec4& color)
{
    ImGui::TextColored(RGBA(0x7F91A0FF), "%s", label);
    ImGui::SameLine(156.0f);
    ImGui::TextColored(color, "%s", value);
}

void WrappedStatusLine(const char* label, const char* value, const ImVec4& color)
{
    ImGui::TextColored(RGBA(0x7F91A0FF), "%s", label);
    ImGui::SameLine(156.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextWrapped("%s", value);
    ImGui::PopStyleColor();
}

} // namespace

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
    if (BeginCard("##health_runtime", cardSize))
    {
        Metric(
            "Action runtime",
            runtimeState,
            runtime.ready ? RGBA(0xFFB800FF) : RGBA(0xFF7A66FF));
    }
    EndCard();

    if (twoColumns)
        ImGui::SameLine(0.0f, gap);
    if (BeginCard("##health_queue", cardSize))
        Metric("Queue", queueValue, runtime.queued == 0 ? RGBA(0xB9BBC0FF) : RGBA(0xFFD56BFF));
    EndCard();

    if (BeginCard("##health_modules", cardSize))
        Metric("SDK modules", modulesValue, failedCount == 0 ? RGBA(0xFFB800FF) : RGBA(0xFF7A66FF));
    EndCard();

    if (twoColumns)
        ImGui::SameLine(0.0f, gap);
    if (BeginCard("##health_logs", cardSize))
        Metric("Loader events", eventValue, errorCount == 0 ? RGBA(0xB9BBC0FF) : RGBA(0xFF7A66FF));
    EndCard();

    ImGui::Spacing();
    ImGui::SeparatorText("Runtime Details");
    if (BeginCard("##health_runtime_details", ImVec2(0.0f, 162.0f)))
    {
        StatusLine("Ready", runtime.ready ? "yes" : "no", runtime.ready ? RGBA(0xFFB800FF) : RGBA(0xFF7A66FF));
        StatusLine("Busy", runtime.busy ? "yes" : "no", runtime.busy ? RGBA(0xFFD56BFF) : RGBA(0xB9BBC0FF));
        StatusLine(
            "Diagnostics",
            runtime.diagnostics_enabled ? "enabled" : "off",
            runtime.diagnostics_enabled ? RGBA(0xFFB800FF) : RGBA(0x7F838CFF));
        StatusLine(
            "Auto reel",
            runtime.auto_reel_enabled ? "enabled" : "off",
            runtime.auto_reel_enabled ? RGBA(0xFFB800FF) : RGBA(0x7F838CFF));
        StatusLine("Snapshots", snapshotValue, RGBA(0xB9BBC0FF));

        if (!runtime.message.empty())
        {
            ImGui::Spacing();
            ImGui::TextWrapped("%s", runtime.message.c_str());
        }
    }
    EndCard();

    ImGui::Spacing();
    ImGui::SeparatorText("Last Signal");
    if (BeginCard("##health_last_signal", ImVec2(0.0f, 134.0f)))
    {
        if (runtime.last_command.empty())
        {
            ImGui::TextColored(RGBA(0x7F91A0FF), "No command result yet.");
        }
        else
        {
            ImGui::TextColored(
                runtime.last_effect_confirmed ? RGBA(0xFFB800FF) : RGBA(0xFFCF66FF),
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
    EndCard();

    ImGui::Spacing();
    ImGui::SeparatorText("Paths");
    if (BeginCard("##health_paths", ImVec2(0.0f, 132.0f)))
    {
        const std::string processDirectory = CurrentProcessDirectory();
        const std::string modsDirectory = Narrow(loader.ModsDirectory());

        WrappedStatusLine("Process dir", processDirectory.empty() ? "unknown" : processDirectory.c_str(), RGBA(0xB9BBC0FF));
        WrappedStatusLine("Mods dir", modsDirectory.empty() ? "not scanned" : modsDirectory.c_str(), RGBA(0xB9BBC0FF));
        StatusLine("SDK scanned", loader.HasLoaded() ? "yes" : "no", loader.HasLoaded() ? RGBA(0xFFB800FF) : RGBA(0xFFCF66FF));
    }
    EndCard();
}

} // namespace fc
