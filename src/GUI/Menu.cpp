// ============================================================================
//  Menu.cpp - main overlay shell and tab navigation.
// ============================================================================

#include "Menu.h"
#include "Tabs/ActionsTab.h"
#include "Tabs/DashboardTab.h"
#include "Tabs/TimersTab.h"
#include "Tabs/SettingsTab.h"
#include "Tabs/SdkTab.h"

#include "imgui.h"

#include <algorithm>

namespace {

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

bool BeginPaddedChild(const char* id, const ImVec2& size)
{
#if IMGUI_VERSION_NUM >= 19000
    return ImGui::BeginChild(id, size, ImGuiChildFlags_AlwaysUseWindowPadding);
#else
    return ImGui::BeginChild(id, size, false, ImGuiWindowFlags_AlwaysUseWindowPadding);
#endif
}

void DrawHeaderChrome(const ImVec2& windowPos, const ImVec2& windowSize, float rounding)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float headerHeight = 70.0f;
    const ImVec2 headerMin = windowPos;
    const ImVec2 headerMax(windowPos.x + windowSize.x, windowPos.y + headerHeight);

    drawList->AddRectFilledMultiColor(
        headerMin,
        headerMax,
        U32(0x111722FF),
        U32(0x172439FF),
        U32(0x111722FF),
        U32(0x0D131DFF));

    drawList->AddRectFilled(
        ImVec2(windowPos.x, windowPos.y + headerHeight - 2.0f),
        ImVec2(windowPos.x + windowSize.x, windowPos.y + headerHeight),
        U32(0x31D3C6FF));

    drawList->AddRect(
        windowPos,
        ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y),
        U32(0x2F3D4EFF),
        rounding,
        0,
        1.0f);
}

bool NavItem(int id, const char* label, bool selected, const ImVec2& size)
{
    ImGui::PushID(id);

    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton("##nav_item", size);
    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 max = Add(pos, size);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImU32 bg = selected
        ? U32(0x1A2838F8)
        : hovered ? U32(0x172131E8) : U32(0x11192300);
    const ImU32 text = selected ? U32(0xEAFBFFFF) : hovered ? U32(0xC9D8E2FF) : U32(0x7F91A0FF);

    if (selected || hovered)
        drawList->AddRectFilled(pos, max, bg, 7.0f);

    if (selected)
    {
        drawList->AddRectFilled(
            ImVec2(pos.x, pos.y + 8.0f),
            ImVec2(pos.x + 3.0f, max.y - 8.0f),
            U32(0x31D3C6FF),
            2.0f);
        drawList->AddRect(pos, max, U32(0x31D3C645), 7.0f, 0, 1.0f);
    }

    drawList->AddText(ImVec2(pos.x + 14.0f, pos.y + 8.0f), text, label);

    ImGui::PopID();
    return pressed;
}

void SectionTitle(const char* title)
{
    ImGui::TextColored(RGBA(0x31D3C6FF), "%s", title);
    ImGui::Separator();
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
    m_tabs.reserve(m_tabs.size() + 5);

    AddTab(std::make_unique<DashboardTab>());
    AddTab(std::make_unique<ActionsTab>());
    AddTab(std::make_unique<TimersTab>());
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

    const ImVec2 minSize(660.0f, 450.0f);
    ImGui::SetNextWindowSize(ImVec2(760.0f, 520.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(minSize, GetMaxWindowSize(minSize));

    if (const ImGuiViewport* viewport = ImGui::GetMainViewport())
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    if (ImGui::Begin("Fishing Companion", nullptr, flags))
    {
        const ImVec2 windowPos = ImGui::GetWindowPos();
        const ImVec2 windowSize = ImGui::GetWindowSize();
        DrawHeaderChrome(windowPos, windowSize, 12.0f);

        ImGui::SetCursorPos(ImVec2(22.0f, 14.0f));
        ImGui::TextColored(RGBA(0xF4FBFFFF), "Fishing Companion");
        ImGui::SetCursorPos(ImVec2(22.0f, 38.0f));
        ImGui::TextColored(RGBA(0x7F91A0FF), "clean DX11 overlay shell");

        const char* status = "LIVE OVERLAY";
        const ImVec2 statusSize = ImGui::CalcTextSize(status);
        ImGui::SetCursorPos(ImVec2(windowSize.x - statusSize.x - 42.0f, 24.0f));
        ImGui::TextColored(RGBA(0xFFCF66FF), "%s", status);

        if (m_tabs.empty())
        {
            ImGui::SetCursorPos(ImVec2(24.0f, 92.0f));
            ImGui::TextDisabled("No tabs registered.");
        }
        else
        {
            m_selectedTab = std::clamp(m_selectedTab, 0, static_cast<int>(m_tabs.size()) - 1);

            const float sidebarWidth = 178.0f;
            const float headerHeight = 82.0f;
            const float gap = 12.0f;
            const float bottomPadding = 16.0f;
            const ImVec2 navSize(sidebarWidth, windowSize.y - headerHeight - bottomPadding);
            const ImVec2 contentSize(windowSize.x - sidebarWidth - gap - 28.0f, navSize.y);

            ImGui::SetCursorPos(ImVec2(14.0f, headerHeight));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, RGBA(0x0D131DF2));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));
            if (BeginPaddedChild("##fc_nav", navSize))
            {
                SectionTitle("Разделы");
                ImGui::Spacing();

                for (int i = 0; i < static_cast<int>(m_tabs.size()); ++i)
                {
                    ITab* tab = m_tabs[i].tab.get();
                    if (!tab)
                        continue;

                    if (NavItem(i, GetTabTitle(*tab), i == m_selectedTab, ImVec2(-1.0f, 36.0f)))
                        m_selectedTab = i;

                    ImGui::Spacing();
                }

                const float footerY = std::max(ImGui::GetCursorPosY() + 16.0f, ImGui::GetWindowHeight() - 58.0f);
                ImGui::SetCursorPosY(footerY);
                ImGui::Separator();
                ImGui::TextColored(RGBA(0x6F8190FF), "Insert  menu");
                ImGui::TextColored(RGBA(0x6F8190FF), "End     unload");
            }
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor();

            ImGui::SameLine(0.0f, gap);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, RGBA(0x111923F5));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));
            if (BeginPaddedChild("##fc_content", contentSize))
            {
                ITab* activeTab = m_tabs[m_selectedTab].tab.get();
                const char* activeTitle = activeTab ? GetTabTitle(*activeTab) : "Untitled";

                ImGui::TextColored(RGBA(0xF4FBFFFF), "%s", activeTitle);
                ImGui::SameLine();
                ImGui::TextColored(RGBA(0x6F8190FF), " / tuned skin");
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (BeginPaddedChild("##fc_tab_body", ImVec2(0.0f, 0.0f)))
                {
                    if (activeTab)
                        activeTab->Render();
                }
                ImGui::EndChild();
            }
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor();
        }
    }
    ImGui::End();

    ImGui::PopStyleVar(3);
}

} // namespace fc
