// ============================================================================
//  LogsTab.cpp - unified runtime and SDK event viewer.
// ============================================================================

#include "LogsTab.h"

#include "../UI.h"
#include "../../Actions/ActionRuntime.h"
#include "../../SDK/ModuleLoader.h"

#include "imgui.h"

#include <Windows.h>
#include <Shellapi.h>

#include <array>
#include <string>
#include <vector>

namespace ui = fc::gui::ui;

namespace {

struct LogEntry
{
    const char* source = "";
    std::string level;
    std::string message;
};

ImVec4 LevelColor(const std::string& level)
{
    if (level == "error")
        return fc::Color(fc::Palette::Coral);
    if (level == "warn")
        return fc::Color(fc::Palette::AmberSoft);
    return fc::Color(fc::Palette::Amber);
}

bool LevelAllowed(const std::string& level, bool showInfo, bool showWarn, bool showError)
{
    if (level == "error")
        return showError;
    if (level == "warn")
        return showWarn;
    return showInfo;
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

void OpenLogFolder()
{
    const std::wstring directory = ui::ProcessDirectoryW();
    if (!directory.empty())
        ShellExecuteW(nullptr, L"open", directory.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

} // namespace

namespace fc {

void LogsTab::Render()
{
    // Initialize the search buffer once; static zero-init already leaves it empty,
    // this just makes the intent explicit for future buffer changes.
    if (m_firstFrame)
        m_firstFrame = false;

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

    if (ui::BeginCard("##log_filters", ImVec2(0.0f, 112.0f)))
    {
        ImGui::SetNextItemWidth(std::min(320.0f, ImGui::GetContentRegionAvail().x));
        ImGui::InputTextWithHint("##log_search", "Filter events", m_localSearch.data(), m_localSearch.size());

        ImGui::Spacing();
        ImGui::Checkbox("Info", &m_showInfo);
        ImGui::SameLine();
        ImGui::Checkbox("Warn", &m_showWarn);
        ImGui::SameLine();
        ImGui::Checkbox("Error", &m_showError);
        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &m_autoScroll);

        ImGui::Spacing();
        ImGui::TextColored(
            Color(Palette::TextMuted),
            "events: %d info / %d warn / %d error",
            infoCount,
            warnCount,
            errorCount);
        ImGui::SameLine();
        if (ImGui::Button("Open Folder", ImVec2(118.0f, 0.0f)))
            OpenLogFolder();
        ImGui::SameLine();
        if (ImGui::Button("Copy", ImVec2(82.0f, 0.0f)))
        {
            // Build the currently visible log lines and push them to the clipboard.
            std::string clip;
            clip.reserve(entries.size() * 64);
            for (const LogEntry& entry : entries)
            {
                if (!LevelAllowed(entry.level, m_showInfo, m_showWarn, m_showError))
                    continue;
                if (!ui::ContainsNoCase(entry.message, m_localSearch.data()) &&
                    !ui::ContainsNoCase(entry.level, m_localSearch.data()) &&
                    !ui::ContainsNoCase(entry.source, m_localSearch.data()))
                {
                    continue;
                }
                clip += entry.level;
                clip += "\t";
                clip += entry.source;
                clip += "\t";
                clip += entry.message;
                clip += "\n";
            }
            ImGui::SetClipboardText(clip.c_str());
        }
    }
    ui::EndCard();

    ImGui::Spacing();
    if (ui::BeginCard("##log_events", ImVec2(0.0f, 0.0f), ImGuiWindowFlags_HorizontalScrollbar))
    {
        if (ImGui::BeginChild("##log_events_scroll", ImVec2(0.0f, 0.0f), false))
        {
            const bool wasAtBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 6.0f;

            if (entries.empty())
            {
                ImGui::TextColored(Color(Palette::TextMuted), "No events yet.");
            }
            else
            {
                for (const LogEntry& entry : entries)
                {
                    if (!LevelAllowed(entry.level, m_showInfo, m_showWarn, m_showError))
                        continue;
                    if (!ui::ContainsNoCase(entry.message, m_localSearch.data()) &&
                        !ui::ContainsNoCase(entry.level, m_localSearch.data()) &&
                        !ui::ContainsNoCase(entry.source, m_localSearch.data()))
                    {
                        continue;
                    }

                    ++shown;
                    ImGui::TextColored(LevelColor(entry.level), "%s", entry.level.c_str());
                    ImGui::SameLine(72.0f);
                    ImGui::TextColored(Color(Palette::TextMuted), "%s", entry.source);
                    ImGui::SameLine(142.0f);
                    ui::HighlightText(entry.message, m_localSearch.data());
                }
            }

            if (!entries.empty() && shown == 0)
                ImGui::TextColored(Color(Palette::TextMuted), "No events matched the current filters.");
            if (m_autoScroll && wasAtBottom)
                ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
    }
    ui::EndCard();

    const std::wstring directory = ui::ProcessDirectoryW();
    if (!directory.empty())
    {
        ImGui::Spacing();
        ImGui::TextColored(Color(Palette::Caret), "Folder: %s", ui::Narrow(directory).c_str());
    }
}

} // namespace fc
