// ============================================================================
//  UI.h - shared host-side ImGui helpers for the built-in tabs.
// ----------------------------------------------------------------------------
//  Single source of the helpers previously duplicated in every tab: card
//  panels, status/metric lines, wide->narrow text conversion, process path
//  resolution, and case-insensitive search + highlight. Colors come from
//  Palette (Theme.h), so the host UI never hardcodes a hex literal.
//
//  This is the host-internal counterpart of the public fc::sdk::ui layer
//  (src/SDK/FCSDK_UI.h). External modules use the SDK helpers; the built-in
//  host shell uses these. They are intentionally separate so the host palette
//  (amber/graphite) can evolve without touching the SDK ABI.
// ============================================================================

#pragma once

#include "Theme.h"
#include "imgui.h"

#include <Windows.h>

#include <cctype>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <string>

namespace fc::gui::ui {

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
//  Cards: bordered child panels with the host surface treatment.
// ---------------------------------------------------------------------------

// Opens a style-matched card. Pass extra ImGuiWindowFlags via flags (e.g. the
// Logs viewer adds ImGuiWindowFlags_HorizontalScrollbar). size.y == 0 means
// "fit content height".
inline bool BeginCard(const char* id, const ImVec2& size = ImVec2(0.0f, 0.0f),
                      ImGuiWindowFlags flags = 0)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Color(Palette::BgCard));
    ImGui::PushStyleColor(ImGuiCol_Border, Color(Palette::Border));
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

inline void EndCard()
{
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

// A compact non-scrolling card (no scrollbars, fixed-looking metrics).
inline bool BeginStatCard(const char* id, const ImVec2& size)
{
    return BeginCard(id, size, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
}

// ---------------------------------------------------------------------------
//  Labeled value rows.
// ---------------------------------------------------------------------------

// Label on its own line, then a colored value (used by Dashboard/Health).
inline void Metric(const char* label, const char* value, const ImVec4& color)
{
    ImGui::TextColored(Color(Palette::TextFaint), "%s", label);
    ImGui::Spacing();
    ImGui::TextColored(color, "%s", value);
}

// "label    value" on one line, value colored. labelWidth is the x of the value
// column (Health uses 156, SDK uses 180).
inline void StatusLine(const char* label, const char* value, const ImVec4& color,
                       float labelWidth = 156.0f)
{
    ImGui::TextColored(Color(Palette::TextLabel), "%s", label);
    ImGui::SameLine(labelWidth);
    ImGui::TextColored(color, "%s", value);
}

// StatusLine whose value may wrap (e.g. long paths).
inline void WrappedStatusLine(const char* label, const char* value, const ImVec4& color,
                              float labelWidth = 156.0f)
{
    ImGui::TextColored(Color(Palette::TextLabel), "%s", label);
    ImGui::SameLine(labelWidth);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextWrapped("%s", value);
    ImGui::PopStyleColor();
}

// ---------------------------------------------------------------------------
//  Text helpers (case-insensitive substring search + match highlight).
// ---------------------------------------------------------------------------

// True if haystack contains needle (case-insensitive). Empty needle == match.
inline bool ContainsNoCase(const char* haystack, const char* needle)
{
    if (!needle || needle[0] == '\0')
        return true;
    if (!haystack)
        return false;

    for (const char* start = haystack; *start; ++start)
    {
        const char* h = start;
        const char* n = needle;
        while (*h && *n &&
               std::tolower(static_cast<unsigned char>(*h)) ==
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

inline bool ContainsNoCase(const std::string& haystack, const char* needle)
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

// Offset of needle in haystack (case-insensitive), or npos.
inline size_t FindNoCase(const char* haystack, const char* needle)
{
    if (!needle || needle[0] == '\0' || !haystack)
        return std::string::npos;

    for (size_t start = 0; haystack[start] != '\0'; ++start)
    {
        size_t h = start;
        const char* n = needle;
        while (haystack[h] && *n &&
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

inline size_t FindNoCase(const std::string& haystack, const char* needle)
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

// Draws text with the first case-insensitive match of query highlighted
// (amber background, dark text). Used by Logs and Actions search results.
inline void HighlightText(const std::string& text, const char* query)
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
        ColorU32(0xFFB800CC),
        2.0f);
    ImGui::TextColored(Color(Palette::BgShell), "%s", selected.c_str());

    if (!after.empty())
    {
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::TextWrapped("%s", after.c_str());
    }
}

// Same as HighlightText but wraps the match on its own leading line for action
// search results ("match: <before>[highlight]<after>"). Preserves the original
// ActionsTab search-result layout exactly.
inline void HighlightLabel(const char* label, const char* query)
{
    const size_t match = FindNoCase(label, query);
    if (match == std::string::npos)
    {
        ImGui::TextColored(Color(Palette::TextMuted), "%s", label);
        return;
    }

    const std::string text(label);
    const size_t length = std::strlen(query);
    const std::string before = text.substr(0, match);
    const std::string selected = text.substr(match, length);
    const std::string after = text.substr(match + length);

    ImGui::TextColored(Color(Palette::TextMuted), "match:");
    ImGui::SameLine();
    if (!before.empty())
    {
        ImGui::TextColored(Color(Palette::TextSoft), "%s", before.c_str());
        ImGui::SameLine(0.0f, 0.0f);
    }

    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 textSize = ImGui::CalcTextSize(selected.c_str());
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(pos.x - 2.0f, pos.y),
        ImVec2(pos.x + textSize.x + 2.0f, pos.y + textSize.y),
        ColorU32(0xFFB800CC),
        2.0f);
    ImGui::TextColored(Color(Palette::BgShell), "%s", selected.c_str());

    if (!after.empty())
    {
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::TextColored(Color(Palette::TextSoft), "%s", after.c_str());
    }
}

// ---------------------------------------------------------------------------
//  Windows path helpers (host process directory, wide->narrow conversion).
// ---------------------------------------------------------------------------

inline std::string Narrow(const std::wstring& value)
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

// Directory of the host process exe (where settings/logs/mods live).
inline std::wstring ProcessDirectoryW()
{
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0)
        return {};

    return fs::path(buffer).parent_path().wstring();
}

inline std::string ProcessDirectory()
{
    return Narrow(ProcessDirectoryW());
}

} // namespace fc::gui::ui
