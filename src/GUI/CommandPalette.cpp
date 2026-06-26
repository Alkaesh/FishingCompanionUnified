// ============================================================================
//  CommandPalette.cpp - global command search dropdown.
// ============================================================================

#include "CommandPalette.h"
#include "UI.h"

#include "imgui.h"

#include <cctype>
#include <cstdio>
#include <cstddef>
#include <cstring>
#include <string>

namespace ui = fc::gui::ui;

namespace fc::gui {

namespace {

// One candidate row, resolved from kActions/kQuickNav into a flat list.
struct Candidate
{
    const ActionSpec* spec;
    const char* matchText; // label + " " + keywords, pre-concatenated lazily
    std::string haystack;  // storage when we built a combined haystack
};

// Does query match the label+keywords? Multi-word AND: every whitespace token
// of the query must appear (case-insensitive).
bool Matches(const ActionSpec& spec, const char* query)
{
    if (!query || query[0] == '\0')
        return false; // empty query = no dropdown

    // Build a single haystack "label keywords" for token search.
    std::string haystack;
    haystack.reserve(64);
    if (spec.label) haystack += spec.label;
    if (spec.keywords && spec.keywords[0] != '\0')
    {
        if (!haystack.empty())
            haystack += ' ';
        haystack += spec.keywords;
    }

    // Walk the query token by token; every token must be present.
    const char* q = query;
    while (*q)
    {
        // Skip whitespace between tokens.
        while (*q && std::isspace(static_cast<unsigned char>(*q)))
            ++q;
        if (!*q)
            break;

        const char* tokStart = q;
        while (*q && !std::isspace(static_cast<unsigned char>(*q)))
            ++q;
        const size_t tokLen = static_cast<size_t>(q - tokStart);

        // Build a null-terminated token for ContainsNoCase.
        char tok[64];
        if (tokLen >= sizeof(tok))
            continue; // token too long to match anything; skip (no false match)
        std::memcpy(tok, tokStart, tokLen);
        tok[tokLen] = '\0';

        if (!ui::ContainsNoCase(haystack, tok))
            return false; // a missing token => AND fails
    }

    return true;
}

} // namespace

PaletteResult RenderCommandPalette(
    const char* query,
    float originX,
    float originY,
    float width,
    int& selectedIndex)
{
    PaletteResult result;

    // Gather matching candidates from both catalogs.
    // (Collecting first keeps the visible list stable within a frame.)
    const ActionSpec* matches[64];
    int count = 0;
    for (const ActionSpec& a : kActions)
    {
        if (count >= 64) break;
        if (Matches(a, query))
            matches[count++] = &a;
    }
    for (const ActionSpec& a : kQuickNav)
    {
        if (count >= 64) break;
        if (Matches(a, query))
            matches[count++] = &a;
    }

    const float rowH = 24.0f;
    const float maxVisible = 8.0f;
    const float listH = static_cast<float>(count) * rowH;
    const float padY = 8.0f;
    const float winH = (count == 0 ? rowH + padY : (listH < maxVisible * rowH ? listH : maxVisible * rowH)) + padY;

    // Clamp the selection into range; if the query changed and shrank the list,
    // fall back to the first row.
    if (count == 0)
        selectedIndex = 0;
    else if (selectedIndex < 0 || selectedIndex >= count)
        selectedIndex = 0;

    // Draw as a borderless popup anchored under the field.
    ImGui::SetNextWindowPos(ImVec2(originX, originY));
    ImGui::SetNextWindowSize(ImVec2(width, winH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 6.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, fc::Color(fc::Palette::BgCard));
    ImGui::PushStyleColor(ImGuiCol_Border, fc::Color(fc::Palette::Border));

    const ImGuiWindowFlags wflags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoFocusOnAppearing;

    char title[32];
    std::snprintf(title, sizeof(title), "##cmd_palette_%p", (const void*)query);
    if (ImGui::Begin(title, nullptr, wflags))
    {
        if (count == 0)
        {
            ImGui::TextColored(fc::Color(fc::Palette::TextMuted), "No commands match");
        }
        else
        {
            // Keyboard navigation while the palette is open.
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && selectedIndex < count - 1)
                ++selectedIndex;
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && selectedIndex > 0)
                --selectedIndex;

            // Allow scrolling if the list is taller than the window.
            ImGui::BeginChild("##cmd_palette_scroll", ImVec2(0.0f, 0.0f), false);
            for (int i = 0; i < count; ++i)
            {
                const ActionSpec* a = matches[i];
                ImGui::PushID(i);
                const bool selected = (i == selectedIndex);

                char rowLabel[160];
                std::snprintf(rowLabel, sizeof(rowLabel), "%s  -  %s", a->label, GroupTitle(a->group));

                if (selected)
                    ImGui::PushStyleColor(ImGuiCol_Header, fc::Color(fc::Palette::Amber));
                const bool activated = ImGui::Selectable(rowLabel, selected, ImGuiSelectableFlags_None, ImVec2(0.0f, rowH));
                if (selected)
                    ImGui::PopStyleColor();

                // Highlight the matched query token within the label.
                // (Drawn as an overlay via HighlightText would stack; instead we
                //  rely on Selectable + the amber selection to indicate a match.
                //  Keep it simple and readable.)

                if (activated || (selected && ImGui::IsKeyPressed(ImGuiKey_Enter)))
                {
                    if (a->navTabTitle)
                    {
                        result.action = PaletteAction::Navigate;
                        result.navTabTitle = a->navTabTitle;
                    }
                    else
                    {
                        result.action = PaletteAction::RunCommand;
                        result.command = a->command;
                    }
                }
                ImGui::PopID();
            }
            // Keep the selected row visible when navigating by keyboard.
            ImGui::SetScrollHereY(selectedIndex * rowH / std::max(1.0f, (float)count * rowH));
            ImGui::EndChild();
        }
    }
    ImGui::End();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);

    return result;
}

} // namespace fc::gui
