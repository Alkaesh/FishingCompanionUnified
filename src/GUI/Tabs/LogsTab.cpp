// ============================================================================
//  LogsTab.cpp - unified runtime and SDK event viewer.
// ============================================================================

#include "LogsTab.h"

#include "../../Actions/ActionRuntime.h"
#include "../../SDK/ModuleLoader.h"

#include "imgui.h"

#include <Windows.h>
#include <Shellapi.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct LogEntry
{
    const char* source = "";
    std::string level;
    std::string message;
};

ImVec4 RGBA(unsigned int hex)
{
    return ImVec4(
        ((hex >> 24) & 0xFF) / 255.0f,
        ((hex >> 16) & 0xFF) / 255.0f,
        ((hex >> 8)  & 0xFF) / 255.0f,
        ((hex)       & 0xFF) / 255.0f);
}

bool BeginCard(const char* id, const ImVec2& size, ImGuiWindowFlags flags = 0)
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
        flags);
#else
    return ImGui::BeginChild(
        id,
        size,
        true,
        ImGuiWindowFlags_AlwaysUseWindowPadding | flags);
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

std::wstring CurrentProcessDirectory()
{
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0)
        return {};

    return fs::path(buffer).parent_path().wstring();
}

void OpenLogFolder()
{
    const std::wstring directory = CurrentProcessDirectory();
    if (!directory.empty())
        ShellExecuteW(nullptr, L"open", directory.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

bool ContainsNoCase(const std::string& haystack, const char* needle)
{
    if (!needle || needle[0] == '\0')
        return true;

    for (size_t start = 0; start < haystack.size(); ++start)
    {
        size_t h = start;
        const char* n = needle;
        while (h < haystack.size() && *n &&
               std::tolower(static_cast<unsigned char>(haystack[h])) ==
               std::tolower(static_cast<unsigned char>(*n)))
        {
            ++h;
            ++n;
        }

        if (*n == '\0')
            return true;
    }

    return false;
}

size_t FindNoCase(const std::string& haystack, const char* needle)
{
    if (!needle || needle[0] == '\0')
        return std::string::npos;

    for (size_t start = 0; start < haystack.size(); ++start)
    {
        size_t h = start;
        const char* n = needle;
        while (h < haystack.size() && *n &&
               std::tolower(static_cast<unsigned char>(haystack[h])) ==
               std::tolower(static_cast<unsigned char>(*n)))
        {
            ++h;
            ++n;
        }

        if (*n == '\0')
            return start;
    }

    return std::string::npos;
}

ImVec4 LevelColor(const std::string& level)
{
    if (level == "error")
        return RGBA(0xFF7A66FF);
    if (level == "warn")
        return RGBA(0xFFCF66FF);
    return RGBA(0xFFB800FF);
}

bool LevelAllowed(const std::string& level, bool showInfo, bool showWarn, bool showError)
{
    if (level == "error")
        return showError;
    if (level == "warn")
        return showWarn;
    return showInfo;
}

void HighlightText(const std::string& text, const char* query)
{
    const size_t match = FindNoCase(text, query);
    if (match == std::string::npos)
    {
        ImGui::TextWrapped("%s", text.c_str());
        return;
    }

    const size_t queryLength = std::char_traits<char>::length(query);
    const std::string before = text.substr(0, match);
    const std::string selected = text.substr(match, queryLength);
    const std::string after = text.substr(match + queryLength);

    if (!before.empty())
    {
        ImGui::TextUnformatted(before.c_str());
        ImGui::SameLine(0.0f, 0.0f);
    }

    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 textSize = ImGui::CalcTextSize(selected.c_str());
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(pos.x - 2.0f, pos.y),
        ImVec2(pos.x + textSize.x + 2.0f, pos.y + textSize.y),
        ImGui::ColorConvertFloat4ToU32(RGBA(0xFFB800CC)),
        2.0f);
    ImGui::TextColored(RGBA(0x121315FF), "%s", selected.c_str());

    if (!after.empty())
    {
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::TextWrapped("%s", after.c_str());
    }
}

std::vector<LogEntry> CollectEntries()
{
    std::vector<LogEntry> entries;

    const fc::actions::Status status = fc::actions::GetStatus();
    const std::vector<fc::sdk::ModuleLoader::Event> loaderEvents = fc::sdk::ModuleLoader::Get().Events();
    entries.reserve(status.recent_events.size() + loaderEvents.size());

    for (const std::string& event : status.recent_events)
        entries.push_back(LogEntry{"Actions", "info", event});

    for (const fc::sdk::ModuleLoader::Event& event : loaderEvents)
        entries.push_back(LogEntry{"SDK", event.level.empty() ? "info" : event.level, event.message});

    return entries;
}

} // namespace

namespace fc {

void LogsTab::Render()
{
    static std::array<char, 96> localSearch{};
    static bool showInfo = true;
    static bool showWarn = true;
    static bool showError = true;
    static bool autoScroll = true;

    const std::vector<LogEntry> entries = CollectEntries();
    int shown = 0;
    int infoCount = 0;
    int warnCount = 0;
    int errorCount = 0;

    for (const LogEntry& entry : entries)
    {
        if (entry.level == "error")
            ++errorCount;
        else if (entry.level == "warn")
            ++warnCount;
        else
            ++infoCount;
    }

    ImGui::SeparatorText("Log Viewer");

    if (BeginCard("##log_filters", ImVec2(0.0f, 112.0f)))
    {
        ImGui::SetNextItemWidth(std::min(320.0f, ImGui::GetContentRegionAvail().x));
        ImGui::InputTextWithHint("##log_search", "Filter events", localSearch.data(), localSearch.size());

        ImGui::Spacing();
        ImGui::Checkbox("Info", &showInfo);
        ImGui::SameLine();
        ImGui::Checkbox("Warn", &showWarn);
        ImGui::SameLine();
        ImGui::Checkbox("Error", &showError);
        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &autoScroll);

        ImGui::Spacing();
        ImGui::TextColored(
            RGBA(0x7F91A0FF),
            "events: %d info / %d warn / %d error",
            infoCount,
            warnCount,
            errorCount);
        ImGui::SameLine();
        if (ImGui::Button("Open Folder", ImVec2(118.0f, 0.0f)))
            OpenLogFolder();
    }
    EndCard();

    ImGui::Spacing();
    if (BeginCard("##log_events", ImVec2(0.0f, 0.0f), ImGuiWindowFlags_HorizontalScrollbar))
    {
        if (ImGui::BeginChild("##log_events_scroll", ImVec2(0.0f, 0.0f), false))
        {
            const bool wasAtBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 6.0f;

            if (entries.empty())
            {
                ImGui::TextColored(RGBA(0x7F91A0FF), "No events yet.");
            }
            else
            {
                for (size_t i = 0; i < entries.size(); ++i)
                {
                    const LogEntry& entry = entries[i];
                    if (!LevelAllowed(entry.level, showInfo, showWarn, showError))
                        continue;
                    if (!ContainsNoCase(entry.message, localSearch.data()) &&
                        !ContainsNoCase(entry.level, localSearch.data()) &&
                        !ContainsNoCase(entry.source, localSearch.data()))
                    {
                        continue;
                    }

                    ++shown;
                    ImGui::TextColored(LevelColor(entry.level), "%s", entry.level.c_str());
                    ImGui::SameLine(72.0f);
                    ImGui::TextColored(RGBA(0x7F91A0FF), "%s", entry.source);
                    ImGui::SameLine(142.0f);
                    HighlightText(entry.message, localSearch.data());
                }
            }

            if (!entries.empty() && shown == 0)
                ImGui::TextColored(RGBA(0x7F91A0FF), "No events matched the current filters.");
            if (autoScroll && wasAtBottom)
                ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
    }
    EndCard();

    const std::wstring directory = CurrentProcessDirectory();
    if (!directory.empty())
    {
        ImGui::Spacing();
        ImGui::TextColored(RGBA(0x62656DFF), "Folder: %s", Narrow(directory).c_str());
    }
}

} // namespace fc
