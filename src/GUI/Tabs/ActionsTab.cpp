#include "ActionsTab.h"

#include "../../Actions/ActionRuntime.h"
#include "../../Core/Overlay.h"
#include "../Menu.h"

#include "imgui.h"

#include <cctype>
#include <cstring>
#include <string>

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

enum class ActionStyle
{
    Normal,
    Primary,
    Danger,
};

struct ActionSpec
{
    const char* label;
    fc::actions::Command command;
    const char* keywords;
    ActionStyle style = ActionStyle::Normal;
};

constexpr ActionSpec kSearchActions[] = {
    {"Refresh", fc::actions::Command::Refresh, "runtime status refresh update actions ready"},
    {"Hitch", fc::actions::Command::Hitch, "fishing hitch"},
    {"Start Hooking", fc::actions::Command::StartHooking, "fishing start_hooking hook"},
    {"Alternative", fc::actions::Command::AlternativeAction, "fishing alternative alt action"},
    {"Podsak", fc::actions::Command::TogglePodsak, "fishing podsak toggle_podsak"},
    {"Toggle Reel", fc::actions::Command::ToggleReel, "fishing reel toggle_reel"},
    {"Set Toggle Reel", fc::actions::Command::FishingSetToggleReel, "fishing set toggle reel"},
    {"Cut Line", fc::actions::Command::CutFishingLine, "fishing cut line cut_fishing_line"},
    {"Return Idle", fc::actions::Command::ReturnIdle, "fishing return idle return_idle"},
    {"Switch Throw Mode", fc::actions::Command::SwitchThrowMode, "fishing throw mode switch"},
    {"Change Distance", fc::actions::Command::ChangeThrowDistance, "fishing throw distance change"},
    {"Set Clip", fc::actions::Command::FishingSetClip, "fishing set clip"},
    {"Rig Clip", fc::actions::Command::RigClip, "fishing rig clip"},
    {"Bait 1", fc::actions::Command::HotSwapBait1, "fishing bait hotswap hot swap 1"},
    {"Bait 2", fc::actions::Command::HotSwapBait2, "fishing bait hotswap hot swap 2"},
    {"Bobber Depth", fc::actions::Command::ChangeBobberDepth, "fishing bobber depth"},
    {"Rod Rest", fc::actions::Command::RodToRodrest, "fishing rod rest rodrest"},
    {"Rod Slot", fc::actions::Command::RodSlot, "fishing rod slot"},
    {"Hand HotSwap", fc::actions::Command::HandItemHotSwap, "fishing hand item hotswap hot swap"},
    {"Auto Cast", fc::actions::Command::AutoCast, "auto cast autocast", ActionStyle::Primary},
    {"Auto Catch", fc::actions::Command::AutoCatch, "auto catch autocatch", ActionStyle::Primary},
    {"Auto Scout", fc::actions::Command::AutoScout, "auto scout autoscout scout_cast", ActionStyle::Primary},
    {"Stop All", fc::actions::Command::StopAll, "stop all cancel halt", ActionStyle::Danger},
    {"Mark Spot", fc::actions::Command::MarkSpot, "mark spot"},
    {"Clear Spot", fc::actions::Command::ClearSpot, "clear spot"},
    {"Scan Fish", fc::actions::Command::ScanFish, "scan fish fish_scan"},
    {"Keep Fish", fc::actions::Command::KeepFish, "keep fish catch result"},
    {"Release Fish", fc::actions::Command::ReleaseFish, "release fish catch result"},
    {"Continue Fishing", fc::actions::Command::ContinueFishing, "continue fishing keep and cast keep_and_cast"},
    {"Manual Roll", fc::actions::Command::ManualRoll, "reel manual roll"},
    {"Roll Boost", fc::actions::Command::ManualRollBoost, "reel manual roll boost"},
    {"Toggle Auto Reel", fc::actions::Command::ToggleAutoReel, "reel auto reel toggle start stop"},
    {"Auto Roll", fc::actions::Command::ToggleAutoRollMode, "reel auto roll mode"},
    {"Reset Auto", fc::actions::Command::ResetAutoRollMode, "reel reset auto roll"},
    {"Switch Speed", fc::actions::Command::SwitchReelSpeed, "reel switch speed"},
    {"Change Speed", fc::actions::Command::ChangeRollSpeed, "reel change speed"},
    {"Friction", fc::actions::Command::ChangeFriction, "reel friction"},
    {"Speed Mode", fc::actions::Command::RollSpeedMode, "reel speed mode"},
    {"Transmission", fc::actions::Command::ChangeTransmissionMode, "reel transmission mode"},
    {"Engine", fc::actions::Command::ToggleEngine, "reel engine"},
    {"Toggle Gearbox", fc::actions::Command::ToggleTransmission, "reel gearbox transmission"},
    {"Catch Fish", fc::actions::Command::DebugCatchFish, "sandbox debug catch fish"},
    {"Repair Rod", fc::actions::Command::DebugRepairRod, "sandbox debug repair rod"},
    {"Spawn Fish", fc::actions::Command::DebugSpawnFish, "sandbox debug spawn fish"},
    {"Fish Jump", fc::actions::Command::DebugFishJump, "sandbox debug fish jump"},
    {"Level Up", fc::actions::Command::DebugLevelUp, "sandbox debug level up"},
    {"Debug Hitch", fc::actions::Command::DebugHitch, "sandbox debug hitch"},
    {"Snapshot", fc::actions::Command::SnapshotDiagnostics, "diagnostics snapshot"},
    {"Toggle Diagnostics", fc::actions::Command::ToggleDiagnostics, "diagnostics log start stop"},
};

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

bool ActionMatches(const ActionSpec& action, const char* query)
{
    return ContainsNoCase(action.label, query) || ContainsNoCase(action.keywords, query);
}

bool CanRunWhenNotReady(fc::actions::Command command)
{
    switch (command)
    {
    case fc::actions::Command::Refresh:
    case fc::actions::Command::StopAll:
    case fc::actions::Command::SnapshotDiagnostics:
    case fc::actions::Command::ToggleDiagnostics:
        return true;
    default:
        return false;
    }
}

size_t FindNoCase(const char* haystack, const char* needle)
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

void HighlightLabel(const char* label, const char* query)
{
    const size_t match = FindNoCase(label, query);
    if (match == std::string::npos)
    {
        ImGui::TextColored(RGBA(0x7F91A0FF), "%s", label);
        return;
    }

    const std::string text(label);
    const size_t length = std::strlen(query);
    const std::string before = text.substr(0, match);
    const std::string selected = text.substr(match, length);
    const std::string after = text.substr(match + length);

    ImGui::TextColored(RGBA(0x7F91A0FF), "match:");
    ImGui::SameLine();
    if (!before.empty())
    {
        ImGui::TextColored(RGBA(0xB9BBC0FF), "%s", before.c_str());
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
        ImGui::TextColored(RGBA(0xB9BBC0FF), "%s", after.c_str());
    }
}

int PushActionStyle(ActionStyle style)
{
    if (style == ActionStyle::Primary)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, RGBA(0x3A2B10FF));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, RGBA(0x5A3D0BFF));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, RGBA(0xFFB800FF));
        return 3;
    }

    if (style == ActionStyle::Danger)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, RGBA(0x6A2430FF));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, RGBA(0x8A3142FF));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, RGBA(0xC24A5CFF));
        return 3;
    }

    return 0;
}

bool StyledActionButton(const ActionSpec& action, const ImVec2& size, const fc::actions::Status& status)
{
    const int pushed = PushActionStyle(action.style);
    const bool pressed = ActionButton(action.label, action.command, size, status, CanRunWhenNotReady(action.command));
    if (pushed > 0)
        ImGui::PopStyleColor(pushed);
    return pressed;
}

bool HasSearchResult(const char* query)
{
    for (const ActionSpec& action : kSearchActions)
    {
        if (ActionMatches(action, query))
            return true;
    }

    return false;
}

void RenderSearchResults(
    const char* query,
    const ImVec2& buttonSize,
    float gap,
    const fc::actions::Status& status)
{
    ImGui::Spacing();
    ImGui::SeparatorText("Search Results");

    int shown = 0;
    for (const ActionSpec& action : kSearchActions)
    {
        if (!ActionMatches(action, query))
            continue;

        if ((shown % 2) == 1)
            ImGui::SameLine(0.0f, gap);

        ImGui::BeginGroup();
        HighlightLabel(action.label, query);
        StyledActionButton(action, buttonSize, status);
        ImGui::EndGroup();
        ++shown;
    }

    if (shown == 0)
        ImGui::TextColored(RGBA(0x7F91A0FF), "No action buttons matched this search.");
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
    const ImVec4 state_color = status.ready ? RGBA(0xFFB800FF) : RGBA(0xFFCF66FF);
    const char* runtime_state = status.busy ? "busy" : (status.ready ? "ready" : "waiting");

    ImGui::SeparatorText("Runtime");

    bool refresh_requested = false;
    if (BeginCard("##runtime_status", ImVec2(0.0f, 156.0f)))
    {
        ImGui::TextColored(state_color, "%s", runtime_state);
        ImGui::SameLine();
        ImGui::TextColored(RGBA(0x7F91A0FF), "queued: %u", status.queued);
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
                status.last_effect_confirmed ? RGBA(0xFFB800FF) : RGBA(0xFFCF66FF),
                "last: %s / %s",
                status.last_command.c_str(),
                status.last_result.c_str());
        if (!status.last_observation.empty())
            ImGui::TextWrapped("observed: %s", status.last_observation.c_str());
    }
    EndCard();

    if (refresh_requested)
        return;

    const float gap = 10.0f;
    const float width = (ImGui::GetContentRegionAvail().x - gap) * 0.5f;
    const ImVec2 button_size(width, 36.0f);
    const char* query = Menu::Get().SearchText();
    auto actionButton = [&](const char* label, actions::Command command, bool allowWhenNotReady = false) {
        return ActionButton(label, command, button_size, status, allowWhenNotReady);
    };

    if (Menu::Get().HasSearchText() && HasSearchResult(query))
    {
        RenderSearchResults(query, button_size, gap, status);
        return;
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Fishing");

    actionButton("Hitch", actions::Command::Hitch);
    ImGui::SameLine(0.0f, gap);
    actionButton("Start Hooking", actions::Command::StartHooking);

    actionButton("Alternative", actions::Command::AlternativeAction);
    ImGui::SameLine(0.0f, gap);
    actionButton("Podsak", actions::Command::TogglePodsak);

    actionButton("Toggle Reel", actions::Command::ToggleReel);
    ImGui::SameLine(0.0f, gap);
    actionButton("Set Toggle Reel", actions::Command::FishingSetToggleReel);

    actionButton("Cut Line", actions::Command::CutFishingLine);
    ImGui::SameLine(0.0f, gap);
    actionButton("Return Idle", actions::Command::ReturnIdle);

    actionButton("Switch Throw Mode", actions::Command::SwitchThrowMode);
    ImGui::SameLine(0.0f, gap);
    actionButton("Change Distance", actions::Command::ChangeThrowDistance);

    actionButton("Set Clip", actions::Command::FishingSetClip);
    ImGui::SameLine(0.0f, gap);
    actionButton("Rig Clip", actions::Command::RigClip);

    actionButton("Bait 1", actions::Command::HotSwapBait1);
    ImGui::SameLine(0.0f, gap);
    actionButton("Bait 2", actions::Command::HotSwapBait2);

    actionButton("Bobber Depth", actions::Command::ChangeBobberDepth);
    ImGui::SameLine(0.0f, gap);
    actionButton("Rod Rest", actions::Command::RodToRodrest);

    actionButton("Rod Slot", actions::Command::RodSlot);
    ImGui::SameLine(0.0f, gap);
    actionButton("Hand HotSwap", actions::Command::HandItemHotSwap);

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, RGBA(0x3A2B10FF));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, RGBA(0x5A3D0BFF));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, RGBA(0xFFB800FF));
    actionButton("Auto Cast", actions::Command::AutoCast);
    ImGui::SameLine(0.0f, gap);
    actionButton("Auto Catch", actions::Command::AutoCatch);
    actionButton("Auto Scout", actions::Command::AutoScout);
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0.0f, gap);
    ImGui::PushStyleColor(ImGuiCol_Button, RGBA(0x6A2430FF));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, RGBA(0x8A3142FF));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, RGBA(0xC24A5CFF));
    actionButton("Stop All", actions::Command::StopAll, true);
    ImGui::PopStyleColor(3);

    actionButton("Mark Spot", actions::Command::MarkSpot);
    ImGui::SameLine(0.0f, gap);
    actionButton("Clear Spot", actions::Command::ClearSpot);
    actionButton("Scan Fish", actions::Command::ScanFish);

    ImGui::Spacing();
    ImGui::SeparatorText("Catch Result");
    actionButton("Keep Fish", actions::Command::KeepFish);
    ImGui::SameLine(0.0f, gap);
    actionButton("Release Fish", actions::Command::ReleaseFish);
    actionButton("Continue Fishing", actions::Command::ContinueFishing);

    ImGui::Spacing();
    ImGui::SeparatorText("Reel");
    ImGui::TextColored(
        status.auto_reel_enabled ? RGBA(0xFFB800FF) : RGBA(0x7F91A0FF),
        "auto reel: %s / ticks: %llu",
        status.auto_reel_enabled ? "on" : "off",
        status.auto_reel_ticks);

    actionButton("Manual Roll", actions::Command::ManualRoll);
    ImGui::SameLine(0.0f, gap);
    actionButton("Roll Boost", actions::Command::ManualRollBoost);

    actionButton(
        status.auto_reel_enabled ? "Stop Auto Reel" : "Start Auto Reel",
        actions::Command::ToggleAutoReel);
    ImGui::SameLine(0.0f, gap);
    actionButton("Auto Roll", actions::Command::ToggleAutoRollMode);

    actionButton("Reset Auto", actions::Command::ResetAutoRollMode);

    actionButton("Switch Speed", actions::Command::SwitchReelSpeed);
    ImGui::SameLine(0.0f, gap);
    actionButton("Change Speed", actions::Command::ChangeRollSpeed);

    actionButton("Friction", actions::Command::ChangeFriction);
    ImGui::SameLine(0.0f, gap);
    actionButton("Speed Mode", actions::Command::RollSpeedMode);

    actionButton("Transmission", actions::Command::ChangeTransmissionMode);
    ImGui::SameLine(0.0f, gap);
    actionButton("Engine", actions::Command::ToggleEngine);

    actionButton("Toggle Gearbox", actions::Command::ToggleTransmission);

    ImGui::Spacing();
    ImGui::SeparatorText("Sandbox Debug");
    actionButton("Catch Fish", actions::Command::DebugCatchFish);
    ImGui::SameLine(0.0f, gap);
    actionButton("Repair Rod", actions::Command::DebugRepairRod);

    actionButton("Spawn Fish", actions::Command::DebugSpawnFish);
    ImGui::SameLine(0.0f, gap);
    actionButton("Fish Jump", actions::Command::DebugFishJump);

    actionButton("Level Up", actions::Command::DebugLevelUp);
    ImGui::SameLine(0.0f, gap);
    actionButton("Debug Hitch", actions::Command::DebugHitch);

    ImGui::Spacing();
    ImGui::SeparatorText("Diagnostics");
    ImGui::TextColored(
        status.last_effect_confirmed ? RGBA(0xFFB800FF) : RGBA(0xFFCF66FF),
        "Observed in game: %s",
        status.last_effect_confirmed ? "confirmed" : "not confirmed");
    ImGui::TextColored(
        status.diagnostics_enabled ? RGBA(0xFFB800FF) : RGBA(0x7F91A0FF),
        "log: %s / snapshots: %llu",
        status.diagnostics_enabled ? "on" : "off",
        status.diagnostic_snapshots);

    if (!status.probe_summary.empty())
        ImGui::TextWrapped("%s", status.probe_summary.c_str());
    if (!status.sensor_summary.empty())
        ImGui::TextWrapped("%s", status.sensor_summary.c_str());

    actionButton("Snapshot", actions::Command::SnapshotDiagnostics, true);
    ImGui::SameLine(0.0f, gap);
    actionButton(
        status.diagnostics_enabled ? "Stop Log" : "Start Log",
        actions::Command::ToggleDiagnostics,
        true);

    ImGui::Spacing();
    ImGui::SeparatorText("Event Log");
    if (BeginCard("##action_event_log", ImVec2(0.0f, 172.0f)))
    {
        if (status.recent_events.empty())
        {
            ImGui::TextColored(RGBA(0x7F91A0FF), "No runtime events yet.");
        }
        else
        {
            if (ImGui::BeginChild("##action_event_log_scroll", ImVec2(0.0f, 0.0f), false))
            {
                for (size_t i = status.recent_events.size(); i > 0; --i)
                {
                    const std::string& event = status.recent_events[i - 1];
                    ImGui::TextColored(RGBA(0x6F8190FF), "%02llu", static_cast<unsigned long long>(i));
                    ImGui::SameLine();
                    ImGui::TextWrapped("%s", event.c_str());
                }
            }
            ImGui::EndChild();
        }
    }
    EndCard();
}

} // namespace fc
