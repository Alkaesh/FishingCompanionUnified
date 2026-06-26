// ============================================================================
//  Menu.cpp - main overlay shell and tab navigation.
// ============================================================================

#include "Menu.h"
#include "Tabs/ActionsTab.h"
#include "Tabs/DashboardTab.h"
#include "Tabs/SettingsTab.h"
#include "Tabs/SdkTab.h"

#include "../Core/Overlay.h"
#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace {

constexpr float kTopbarHeight = 42.0f;
constexpr float kTopbarY = 7.0f;
constexpr float kToolbarY = 8.0f;
constexpr float kToolbarButtonSize = 28.0f;
constexpr float kToolbarGap = 6.0f;
constexpr float kWindowEdgePadding = 10.0f;
constexpr float kBrandWidth = 92.0f;
constexpr float kNavStartX = 106.0f;

ImVec2 Add(const ImVec2& a, const ImVec2& b)
{
    return ImVec2(a.x + b.x, a.y + b.y);
}

ImVec4 RGBA(unsigned int hex)
{
    return ImVec4(
        ((hex >> 24) & 0xFF) / 255.0f,
        ((hex >> 16) & 0xFF) / 255.0f,
        ((hex >> 8)  & 0xFF) / 255.0f,
        ((hex)       & 0xFF) / 255.0f);
}

ImU32 U32(unsigned int hex)
{
    return ImGui::ColorConvertFloat4ToU32(RGBA(hex));
}

ImVec2 GetMaxWindowSize(const ImVec2& minSize)
{
    ImVec2 maxSize(1240.0f, 900.0f);
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;

    if (displaySize.x > 0.0f)
        maxSize.x = std::min(maxSize.x, std::max(minSize.x, displaySize.x - 32.0f));
    if (displaySize.y > 0.0f)
        maxSize.y = std::min(maxSize.y, std::max(minSize.y, displaySize.y - 32.0f));

    return maxSize;
}

const char* GetTabTitle(const fc::ITab& tab)
{
    const char* title = tab.Title();
    return (title && title[0] != '\0') ? title : "Untitled";
}

bool ContainsNoCase(const char* haystack, const char* needle)
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

bool TabMatchesSearch(const fc::ITab* tab, const char* query)
{
    if (!query || query[0] == '\0')
        return true;
    if (!tab)
        return false;

    const char* title = GetTabTitle(*tab);
    return ContainsNoCase(title, query) || ContainsNoCase(tab->SearchKeywords(), query);
}

bool BeginPanelChild(const char* id, const ImVec2& size)
{
#if IMGUI_VERSION_NUM >= 19000
    return ImGui::BeginChild(id, size, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
#else
    return ImGui::BeginChild(id, size, true, ImGuiWindowFlags_AlwaysUseWindowPadding);
#endif
}

void DrawHexMark(ImDrawList* drawList, const ImVec2& center, float radius, ImU32 stroke, ImU32 fill)
{
    ImVec2 points[6] = {
        ImVec2(center.x + radius * 0.86f, center.y - radius * 0.50f),
        ImVec2(center.x + radius * 0.86f, center.y + radius * 0.50f),
        ImVec2(center.x, center.y + radius),
        ImVec2(center.x - radius * 0.86f, center.y + radius * 0.50f),
        ImVec2(center.x - radius * 0.86f, center.y - radius * 0.50f),
        ImVec2(center.x, center.y - radius),
    };

    drawList->AddConvexPolyFilled(points, 6, fill);
    drawList->AddPolyline(points, 6, stroke, ImDrawFlags_Closed, 1.6f);
}

void DrawShellChrome(const ImVec2& windowPos, const ImVec2& windowSize, float rounding)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawList->AddRectFilled(
        windowPos,
        ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y),
        U32(0x121315FF),
        rounding);

    drawList->AddRectFilled(
        windowPos,
        ImVec2(windowPos.x + windowSize.x, windowPos.y + kTopbarHeight),
        U32(0x1A1B1EFF),
        rounding,
        ImDrawFlags_RoundCornersTop);

    drawList->AddLine(
        ImVec2(windowPos.x, windowPos.y + kTopbarHeight),
        ImVec2(windowPos.x + windowSize.x, windowPos.y + kTopbarHeight),
        U32(0x292A2EFF),
        1.0f);

    drawList->AddRect(
        windowPos,
        ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y),
        U32(0xF0B000CC),
        rounding,
        0,
        1.2f);

    drawList->AddLine(
        ImVec2(windowPos.x + 1.0f, windowPos.y + windowSize.y - 2.0f),
        ImVec2(windowPos.x + windowSize.x - 2.0f, windowPos.y + windowSize.y - 2.0f),
        U32(0xF0B000FF),
        1.0f);
}

void DrawBrand()
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    DrawHexMark(drawList, ImVec2(pos.x + 13.0f, pos.y + 13.0f), 10.0f, U32(0xFFB800FF), U32(0x7A3B1BFF));
    drawList->AddText(ImVec2(pos.x + 31.0f, pos.y + 5.0f), U32(0xF2C25AFF), "Byster");
    ImGui::Dummy(ImVec2(kBrandWidth, kToolbarButtonSize));
}

bool TopTab(int id, const char* label, bool selected, const ImVec2& size)
{
    ImGui::PushID(id);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton("##top_tab", size);
    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 max = Add(pos, size);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImU32 bg = selected
        ? U32(0x242018F8)
        : hovered ? U32(0x202124E8) : U32(0x11121400);
    const ImU32 text = selected ? U32(0xFFD56BFF) : hovered ? U32(0xE6E0D2FF) : U32(0x8D9098FF);

    if (selected || hovered)
        drawList->AddRectFilled(pos, max, bg, 2.0f);

    if (selected)
    {
        drawList->AddRectFilled(
            ImVec2(pos.x + 4.0f, max.y - 2.0f),
            ImVec2(max.x - 4.0f, max.y),
            U32(0xFFB800FF),
            1.0f);
    }

    DrawHexMark(drawList, ImVec2(pos.x + 14.0f, pos.y + size.y * 0.5f), 5.0f, selected ? U32(0xFFB800FF) : U32(0x5B5E66FF), U32(0x00000000));
    const ImVec4 textClip(pos.x + 24.0f, pos.y, max.x - 4.0f, max.y);
    drawList->AddText(
        ImGui::GetFont(),
        ImGui::GetFontSize(),
        ImVec2(pos.x + 26.0f, pos.y + 7.0f),
        text,
        label,
        nullptr,
        0.0f,
        &textClip);
    ImGui::PopID();
    return pressed;
}

enum class ToolIcon
{
    Collapse,
    Settings,
};

void DrawToolIcon(ImDrawList* drawList, ToolIcon icon, const ImVec2& pos, const ImVec2& size, ImU32 color)
{
    const ImVec2 center(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);

    if (icon == ToolIcon::Collapse)
    {
        drawList->AddLine(
            ImVec2(center.x - 4.5f, center.y + 2.0f),
            ImVec2(center.x, center.y - 3.0f),
            color,
            1.7f);
        drawList->AddLine(
            ImVec2(center.x, center.y - 3.0f),
            ImVec2(center.x + 4.5f, center.y + 2.0f),
            color,
            1.7f);
        return;
    }

    drawList->AddCircle(center, 4.2f, color, 16, 1.5f);
    drawList->AddLine(ImVec2(center.x - 8.0f, center.y), ImVec2(center.x - 5.8f, center.y), color, 1.5f);
    drawList->AddLine(ImVec2(center.x + 5.8f, center.y), ImVec2(center.x + 8.0f, center.y), color, 1.5f);
    drawList->AddLine(ImVec2(center.x, center.y - 8.0f), ImVec2(center.x, center.y - 5.8f), color, 1.5f);
    drawList->AddLine(ImVec2(center.x, center.y + 5.8f), ImVec2(center.x, center.y + 8.0f), color, 1.5f);
    drawList->AddLine(ImVec2(center.x - 5.8f, center.y - 5.8f), ImVec2(center.x - 4.2f, center.y - 4.2f), color, 1.5f);
    drawList->AddLine(ImVec2(center.x + 4.2f, center.y + 4.2f), ImVec2(center.x + 5.8f, center.y + 5.8f), color, 1.5f);
    drawList->AddLine(ImVec2(center.x + 5.8f, center.y - 5.8f), ImVec2(center.x + 4.2f, center.y - 4.2f), color, 1.5f);
    drawList->AddLine(ImVec2(center.x - 4.2f, center.y + 4.2f), ImVec2(center.x - 5.8f, center.y + 5.8f), color, 1.5f);
}

bool ToolbarButton(const char* id, ToolIcon icon, const char* tooltip)
{
    ImGui::PushID(id);
    const ImVec2 size(kToolbarButtonSize, kToolbarButtonSize);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton("##tool", size);
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawList->AddRectFilled(pos, Add(pos, size), hovered ? U32(0x2C2D32FF) : U32(0x17181BFF), 2.0f);
    drawList->AddRect(pos, Add(pos, size), hovered ? U32(0xFFB80088) : U32(0x2A2B30FF), 2.0f, 0, 1.0f);
    DrawToolIcon(drawList, icon, pos, size, hovered ? U32(0xFFD56BFF) : U32(0xA7ABB3FF));

    if (hovered && tooltip)
        ImGui::SetTooltip("%s", tooltip);

    ImGui::PopID();
    return pressed;
}

} // namespace

namespace fc {

Menu& Menu::Get()
{
    static Menu instance;
    return instance;
}

void Menu::RegisterDefaultTabs()
{
    if (m_defaultTabsRegistered)
        return;

    m_defaultTabsRegistered = true;
    m_tabs.reserve(m_tabs.size() + 4);

    AddTab(std::make_unique<DashboardTab>());
    AddTab(std::make_unique<ActionsTab>());
    AddTab(std::make_unique<SettingsTab>());
    AddTab(std::make_unique<SdkTab>());
}

void Menu::AddTab(std::unique_ptr<ITab> tab, void* owner)
{
    if (!tab)
        return;

    m_tabs.push_back(TabEntry{std::move(tab), owner});
}

void Menu::RemoveTabsByOwner(void* owner)
{
    if (!owner)
        return;

    m_tabs.erase(
        std::remove_if(m_tabs.begin(), m_tabs.end(), [owner](const TabEntry& entry) {
            return entry.owner == owner;
        }),
        m_tabs.end());

    if (m_tabs.empty())
        m_selectedTab = 0;
    else
        m_selectedTab = std::clamp(m_selectedTab, 0, static_cast<int>(m_tabs.size()) - 1);
}

void Menu::Render()
{
    RegisterDefaultTabs();

    const ImVec2 minSize(660.0f, 420.0f);
    ImGui::SetNextWindowSize(ImVec2(820.0f, 540.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(minSize, GetMaxWindowSize(minSize));

    if (const ImGuiViewport* viewport = ImGui::GetMainViewport())
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    if (ImGui::Begin("Byster", nullptr, flags))
    {
        const ImVec2 windowPos = ImGui::GetWindowPos();
        const ImVec2 windowSize = ImGui::GetWindowSize();
        DrawShellChrome(windowPos, windowSize, 6.0f);

        ImGui::SetCursorPos(ImVec2(12.0f, kTopbarY));
        DrawBrand();

        const float settingsX = windowSize.x - kWindowEdgePadding - kToolbarButtonSize;
        const float collapseX = settingsX - kToolbarGap - kToolbarButtonSize;
        const bool showSearch = windowSize.x >= 740.0f;
        const float searchWidth = showSearch ? std::clamp(windowSize.x * 0.15f, 108.0f, 170.0f) : 0.0f;
        const float searchX = collapseX - 8.0f - searchWidth;
        const float navRight = (showSearch ? searchX : collapseX) - 12.0f;

        if (showSearch)
        {
            ImGui::SetCursorPos(ImVec2(searchX, kToolbarY));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, RGBA(0x101114FF));
            ImGui::PushStyleColor(ImGuiCol_Border, RGBA(0x26272CFF));
            ImGui::PushStyleColor(ImGuiCol_TextDisabled, RGBA(0x62656DFF));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            ImGui::SetNextItemWidth(searchWidth);
            ImGui::InputTextWithHint(
                "##byster_search",
                "Search",
                m_searchText.data(),
                m_searchText.size());
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(3);
        }

        ImGui::SetCursorPos(ImVec2(collapseX, kToolbarY));
        if (ToolbarButton("collapse_hint", ToolIcon::Collapse, "Hide menu"))
            Overlay::Get().SetMenuVisible(false);

        ImGui::SetCursorPos(ImVec2(settingsX, kToolbarY));
        if (ToolbarButton("settings_shortcut", ToolIcon::Settings, "Open Settings"))
        {
            m_searchText[0] = '\0';

            for (int i = 0; i < static_cast<int>(m_tabs.size()); ++i)
            {
                ITab* tab = m_tabs[i].tab.get();
                if (tab && std::strcmp(GetTabTitle(*tab), "Settings") == 0)
                {
                    m_selectedTab = i;
                    break;
                }
            }
        }

        int firstSearchMatch = -1;
        for (int i = 0; i < static_cast<int>(m_tabs.size()); ++i)
        {
            if (TabMatchesSearch(m_tabs[i].tab.get(), m_searchText.data()))
            {
                firstSearchMatch = i;
                break;
            }
        }

        if (m_searchText[0] != '\0' &&
            (m_selectedTab < 0 ||
             m_selectedTab >= static_cast<int>(m_tabs.size()) ||
             !TabMatchesSearch(m_tabs[m_selectedTab].tab.get(), m_searchText.data())))
        {
            if (firstSearchMatch >= 0)
                m_selectedTab = firstSearchMatch;
        }

        ImGui::SetCursorPos(ImVec2(kNavStartX, kTopbarY));
        bool drewNavTab = false;
        for (int i = 0; i < static_cast<int>(m_tabs.size()); ++i)
        {
            ITab* tab = m_tabs[i].tab.get();
            if (!tab || !TabMatchesSearch(tab, m_searchText.data()))
                continue;

            const char* title = GetTabTitle(*tab);
            const float tabWidth = std::clamp(ImGui::CalcTextSize(title).x + 34.0f, 64.0f, 104.0f);
            if (ImGui::GetCursorPosX() + tabWidth > navRight)
                break;

            if (drewNavTab)
                ImGui::SameLine(0.0f, 2.0f);

            if (TopTab(i, title, i == m_selectedTab, ImVec2(tabWidth, 31.0f)))
                m_selectedTab = i;

            drewNavTab = true;
        }

        if (m_tabs.empty())
        {
            ImGui::SetCursorPos(ImVec2(18.0f, 54.0f));
            ImGui::TextDisabled("No tabs registered.");
        }
        else
        {
            m_selectedTab = std::clamp(m_selectedTab, 0, static_cast<int>(m_tabs.size()) - 1);

            const ImVec2 contentSize(windowSize.x - 16.0f, windowSize.y - 52.0f);
            ImGui::SetCursorPos(ImVec2(8.0f, 46.0f));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, RGBA(0x101113FA));
            ImGui::PushStyleColor(ImGuiCol_Border, RGBA(0x2B2C31FF));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
            if (BeginPanelChild("##byster_content", contentSize))
            {
                ITab* activeTab = firstSearchMatch >= 0 ? m_tabs[m_selectedTab].tab.get() : nullptr;
                if (activeTab)
                {
                    activeTab->Render();
                }
                else
                {
                    ImGui::TextColored(RGBA(0xFFD56BFF), "No matching section");
                    ImGui::Spacing();
                    ImGui::TextColored(RGBA(0x7F838CFF), "Try: actions, sdk, hotkeys, diagnostics, modules.");
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);
        }
    }
    ImGui::End();

    ImGui::PopStyleVar(3);
}

} // namespace fc
