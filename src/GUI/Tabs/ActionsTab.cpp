#include "ActionsTab.h"

#include "../ActionsTable.h"
#include "../UI.h"
#include "../../Actions/ActionRuntime.h"
#include "../../Core/Overlay.h"

#include "imgui.h"

#include <cstring>
#include <string>

namespace ui = fc::gui::ui;

namespace {

// Aliases for the palette colors used heavily in this tab.
constexpr unsigned int kAmber    = fc::Palette::Amber;
constexpr unsigned int kAmberHi  = fc::Palette::AmberHi;
constexpr unsigned int kAmberSoft= fc::Palette::AmberSoft;
constexpr unsigned int kMuted    = fc::Palette::TextMuted;

std::string ButtonCaption(const char* label, const fc::actions::Status& status, bool enabled)
{
    std::string caption(label ? label : "");
    if (!enabled)
    {
        caption += " [wait]";
        return caption;
    }

    if (status.busy)
        caption += " [busy]";
    else if (status.queued > 0)
        caption += " [q:";
    else
        return caption;

    if (status.queued > 0 && !status.busy)
    {
        caption += std::to_string(status.queued);
        caption += "]";
    }

    return caption;
}

bool ActionButton(
    const char* label,
    fc::actions::Command command,
    const ImVec2& size,
    const fc::actions::Status& status,
    bool allowWhenNotReady = false)
{
    const bool enabled = allowWhenNotReady || status.ready;
    const std::string caption = ButtonCaption(label, status, enabled);
    const std::string imguiLabel = caption + "##" + (label ? label : "action");

    if (!enabled)
        ImGui::BeginDisabled();

    if (!ImGui::Button(imguiLabel.c_str(), size))
    {
        if (!enabled)
            ImGui::EndDisabled();
        return false;
    }

    if (!enabled)
        ImGui::EndDisabled();

    if (command == fc::actions::Command::AutoCast)
        fc::Overlay::Get().SetMenuVisible(false);

    fc::actions::Queue(command);
    return true;
}

int PushActionStyle(fc::gui::ActionStyle style)
{
    using namespace fc::gui;
    if (style == ActionStyle::Primary)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, fc::Color(0x3A2B10FF));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, fc::Color(0x5A3D0BFF));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, fc::Color(kAmber));
        return 3;
    }

    if (style == ActionStyle::Danger)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, fc::Color(0x6A2430FF));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, fc::Color(0x8A3142FF));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, fc::Color(0xC24A5CFF));
        return 3;
    }

    return 0;
}

bool StyledActionButton(const fc::gui::ActionSpec& action, const ImVec2& size, const fc::actions::Status& status)
{
    const int pushed = PushActionStyle(action.style);
    const bool pressed = ActionButton(
        action.label, action.command, size, status,
        fc::gui::CanRunWhenNotReady(action.command));
    if (pushed > 0)
        ImGui::PopStyleColor(pushed);
    return pressed;
}

// The dynamic caption shown on a command button (e.g. "Stop Auto Reel").
// Returns action.label for commands with a fixed caption.
const char* DynamicLabel(const fc::gui::ActionSpec& action, const fc::actions::Status& status)
{
    switch (action.command)
    {
    case fc::actions::Command::ToggleAutoReel:
        return status.auto_reel_enabled ? "Stop Auto Reel" : "Start Auto Reel";
    case fc::actions::Command::ToggleAutoFish:
        return status.auto_fish_enabled ? "Stop Auto Fish" : "Start Auto Fish";
    case fc::actions::Command::ToggleDiagnostics:
        return status.diagnostics_enabled ? "Stop Log" : "Start Log";
    default:
        return action.label;
    }
}

} // namespace

namespace fc {

const char* ActionsTab::SearchKeywords() const
{
    return "actions commands fishing reel diagnostics log queue runtime refresh hitch start hooking "
           "alternative podsak toggle reel cut line return idle throw distance clip bait bobber rod rest "
           "auto cast auto catch auto scout stop all mark spot clear spot scan fish keep fish release fish "
           "continue fishing manual roll boost auto reel auto roll reset speed friction transmission engine "
           "gearbox sandbox debug catch fish repair rod spawn fish fish jump level up snapshot start log stop log";
}

void ActionsTab::Render()
{
    const actions::Status status = actions::GetStatus();
    const ImVec4 state_color = status.ready ? fc::Color(kAmber) : fc::Color(kAmberSoft);
    const char* runtime_state = status.busy ? "busy" : (status.ready ? "ready" : "waiting");

    ImGui::SeparatorText("Runtime");

    bool refresh_requested = false;
    if (ui::BeginStatCard("##runtime_status", ImVec2(0.0f, 156.0f)))
    {
        ImGui::TextColored(state_color, "%s", runtime_state);
        ImGui::SameLine();
        ImGui::TextColored(fc::Color(kMuted), "queued: %u", status.queued);
        const float refreshX = ImGui::GetWindowContentRegionMax().x - 112.0f;
        if (refreshX > ImGui::GetCursorPosX() + 8.0f)
            ImGui::SameLine(refreshX);
        else
            ImGui::SameLine();
        refresh_requested = ActionButton(
            "Refresh",
            actions::Command::Refresh,
            ImVec2(112.0f, 30.0f),
            status,
            true);

        if (!status.message.empty())
            ImGui::TextWrapped("%s", status.message.c_str());

        if (!status.last_command.empty())
            ImGui::TextColored(
                status.last_effect_confirmed ? fc::Color(kAmber) : fc::Color(kAmberSoft),
                "last: %s / %s",
                status.last_command.c_str(),
                status.last_result.c_str());
        if (!status.last_observation.empty())
            ImGui::TextWrapped("observed: %s", status.last_observation.c_str());
    }
    ui::EndCard();

    if (refresh_requested)
        return;

    const float gap = 10.0f;
    const float width = (ImGui::GetContentRegionAvail().x - gap) * 0.5f;
    const ImVec2 button_size(width, 36.0f);

    // Button grid, grouped. Contextual status lines are kept inline exactly as
    // in the previous hand-written layout (reel/auto-reel + diagnostics).
    auto runButton = [&](const fc::gui::ActionSpec& action) {
        const int pushed = PushActionStyle(action.style);
        const bool ok = ActionButton(
            DynamicLabel(action, status), action.command, button_size, status,
            fc::gui::CanRunWhenNotReady(action.command));
        if (pushed > 0)
            ImGui::PopStyleColor(pushed);
        return ok;
    };

    // Render every action in a group as a two-column button grid.
    auto renderGroup = [&](fc::gui::ActionGroup group) {
        bool firstInRow = true;
        for (const fc::gui::ActionSpec& action : fc::gui::kActions)
        {
            if (action.group != group)
                continue;

            if (!firstInRow)
                ImGui::SameLine(0.0f, gap);
            runButton(action);
            firstInRow = !firstInRow;
        }
    };

    using AG = fc::gui::ActionGroup;

    // -- Fishing --------------------------------------------------------
    ImGui::Spacing();
    ImGui::SeparatorText(fc::gui::GroupTitle(AG::Fishing));
    renderGroup(AG::Fishing);

    // -- Automation -----------------------------------------------------
    ImGui::Spacing();
    ImGui::SeparatorText(fc::gui::GroupTitle(AG::Auto));
    ImGui::TextColored(
        status.auto_fish_enabled ? fc::Color(kAmber) : fc::Color(kMuted),
        "auto fish: %s / state: %s / rod in hand: %s / cycles: %llu / load: %.2f",
        status.auto_fish_enabled ? "on" : "off",
        status.auto_fish_state.empty() ? "-" : status.auto_fish_state.c_str(),
        status.auto_fish_rod_in_hand ? "yes" : "NO",
        status.auto_fish_cycles,
        status.auto_fish_rod_load);
    renderGroup(AG::Auto);

    // -- Spots & Scan ---------------------------------------------------
    ImGui::Spacing();
    ImGui::SeparatorText(fc::gui::GroupTitle(AG::Spots));
    renderGroup(AG::Spots);

    // -- Catch Result ---------------------------------------------------
    ImGui::Spacing();
    ImGui::SeparatorText(fc::gui::GroupTitle(AG::CatchResult));
    renderGroup(AG::CatchResult);

    // -- Reel (with the contextual auto-reel status line) ---------------
    ImGui::Spacing();
    ImGui::SeparatorText(fc::gui::GroupTitle(AG::Reel));
    ImGui::TextColored(
        status.auto_reel_enabled ? fc::Color(kAmber) : fc::Color(kMuted),
        "auto reel: %s / ticks: %llu",
        status.auto_reel_enabled ? "on" : "off",
        status.auto_reel_ticks);
    renderGroup(AG::Reel);

    // -- Sandbox Debug --------------------------------------------------
    ImGui::Spacing();
    ImGui::SeparatorText(fc::gui::GroupTitle(AG::Sandbox));
    renderGroup(AG::Sandbox);

    // -- Diagnostics (with contextual preamble) -------------------------
    ImGui::Spacing();
    ImGui::SeparatorText(fc::gui::GroupTitle(AG::Diagnostics));
    ImGui::TextColored(
        status.last_effect_confirmed ? fc::Color(kAmber) : fc::Color(kAmberSoft),
        "Observed in game: %s",
        status.last_effect_confirmed ? "confirmed" : "not confirmed");
    ImGui::TextColored(
        status.diagnostics_enabled ? fc::Color(kAmber) : fc::Color(kMuted),
        "log: %s / snapshots: %llu",
        status.diagnostics_enabled ? "on" : "off",
        status.diagnostic_snapshots);

    if (!status.probe_summary.empty())
        ImGui::TextWrapped("%s", status.probe_summary.c_str());
    if (!status.sensor_summary.empty())
        ImGui::TextWrapped("%s", status.sensor_summary.c_str());

    {
        const fc::gui::ActionSpec* group[8];
        size_t n = 0;
        for (const auto& a : fc::gui::kActions)
            if (a.group == AG::Diagnostics && n < 8)
                group[n++] = &a;

        // Snapshot | Toggle Diagnostics, each allowed when not ready.
        for (size_t i = 0; i < n; ++i)
        {
            runButton(*group[i]);
            if ((i % 2) == 0 && i + 1 < n)
                ImGui::SameLine(0.0f, gap);
        }
    }

    // -- Event Log ------------------------------------------------------
    ImGui::Spacing();
    ImGui::SeparatorText("Event Log");
    if (ui::BeginCard("##action_event_log", ImVec2(0.0f, 172.0f)))
    {
        if (status.recent_events.empty())
        {
            ImGui::TextColored(fc::Color(kMuted), "No runtime events yet.");
        }
        else
        {
            if (ImGui::BeginChild("##action_event_log_scroll", ImVec2(0.0f, 0.0f), false))
            {
                for (size_t i = status.recent_events.size(); i > 0; --i)
                {
                    const std::string& event = status.recent_events[i - 1];
                    ImGui::TextColored(fc::Color(fc::Palette::TextDim), "%02llu", static_cast<unsigned long long>(i));
                    ImGui::SameLine();
                    ImGui::TextWrapped("%s", event.c_str());
                }
            }
            ImGui::EndChild();
        }
    }
    ui::EndCard();
}

} // namespace fc
