#include "ActionsTab.h"

#include "../../Actions/ActionRuntime.h"
#include "../../Core/Overlay.h"

#include "imgui.h"

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

bool ActionButton(const char* label, fc::actions::Command command, const ImVec2& size)
{
    if (!ImGui::Button(label, size))
        return false;

    if (command == fc::actions::Command::AutoCast)
        fc::Overlay::Get().SetMenuVisible(false);

    fc::actions::Queue(command);
    return true;
}

} // namespace

namespace fc {

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
        refresh_requested = ActionButton("Refresh", actions::Command::Refresh, ImVec2(112.0f, 30.0f));

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

    ImGui::Spacing();
    ImGui::SeparatorText("Fishing");

    const float gap = 10.0f;
    const float width = (ImGui::GetContentRegionAvail().x - gap) * 0.5f;
    const ImVec2 button_size(width, 36.0f);

    ActionButton("Hitch", actions::Command::Hitch, button_size);
    ImGui::SameLine(0.0f, gap);
    ActionButton("Start Hooking", actions::Command::StartHooking, button_size);

    ActionButton("Alternative", actions::Command::AlternativeAction, button_size);
    ImGui::SameLine(0.0f, gap);
    ActionButton("Podsak", actions::Command::TogglePodsak, button_size);

    ActionButton("Toggle Reel", actions::Command::ToggleReel, button_size);
    ImGui::SameLine(0.0f, gap);
    ActionButton("Set Toggle Reel", actions::Command::FishingSetToggleReel, button_size);

    ActionButton("Cut Line", actions::Command::CutFishingLine, button_size);
    ImGui::SameLine(0.0f, gap);
    ActionButton("Return Idle", actions::Command::ReturnIdle, button_size);

    ActionButton("Switch Throw Mode", actions::Command::SwitchThrowMode, button_size);
    ImGui::SameLine(0.0f, gap);
    ActionButton("Change Distance", actions::Command::ChangeThrowDistance, button_size);

    ActionButton("Set Clip", actions::Command::FishingSetClip, button_size);
    ImGui::SameLine(0.0f, gap);
    ActionButton("Rig Clip", actions::Command::RigClip, button_size);

    ActionButton("Bait 1", actions::Command::HotSwapBait1, button_size);
    ImGui::SameLine(0.0f, gap);
    ActionButton("Bait 2", actions::Command::HotSwapBait2, button_size);

    ActionButton("Bobber Depth", actions::Command::ChangeBobberDepth, button_size);
    ImGui::SameLine(0.0f, gap);
    ActionButton("Rod Rest", actions::Command::RodToRodrest, button_size);

    ActionButton("Rod Slot", actions::Command::RodSlot, button_size);
    ImGui::SameLine(0.0f, gap);
    ActionButton("Hand HotSwap", actions::Command::HandItemHotSwap, button_size);

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, RGBA(0x3A2B10FF));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, RGBA(0x5A3D0BFF));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, RGBA(0xFFB800FF));
    ActionButton("Auto Cast", actions::Command::AutoCast, button_size);
    ImGui::SameLine(0.0f, gap);
    ActionButton("Auto Catch", actions::Command::AutoCatch, button_size);
    ActionButton("Auto Scout", actions::Command::AutoScout, button_size);
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0.0f, gap);
    ImGui::PushStyleColor(ImGuiCol_Button, RGBA(0x6A2430FF));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, RGBA(0x8A3142FF));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, RGBA(0xC24A5CFF));
    ActionButton("Stop All", actions::Command::StopAll, button_size);
    ImGui::PopStyleColor(3);

    ActionButton("Mark Spot", actions::Command::MarkSpot, button_size);
    ImGui::SameLine(0.0f, gap);
    ActionButton("Clear Spot", actions::Command::ClearSpot, button_size);
    ActionButton("Scan Fish", actions::Command::ScanFish, button_size);

    ImGui::Spacing();
    ImGui::SeparatorText("Catch Result");
    ActionButton("Keep Fish", actions::Command::KeepFish, button_size);
    ImGui::SameLine(0.0f, gap);
    ActionButton("Release Fish", actions::Command::ReleaseFish, button_size);
    ActionButton("Continue Fishing", actions::Command::ContinueFishing, button_size);

    ImGui::Spacing();
    ImGui::SeparatorText("Reel");
    ImGui::TextColored(
        status.auto_reel_enabled ? RGBA(0xFFB800FF) : RGBA(0x7F91A0FF),
        "auto reel: %s / ticks: %llu",
        status.auto_reel_enabled ? "on" : "off",
        status.auto_reel_ticks);

    ActionButton("Manual Roll", actions::Command::ManualRoll, button_size);
    ImGui::SameLine(0.0f, gap);
    ActionButton("Roll Boost", actions::Command::ManualRollBoost, button_size);

    ActionButton(
        status.auto_reel_enabled ? "Stop Auto Reel" : "Start Auto Reel",
        actions::Command::ToggleAutoReel,
        button_size);
    ImGui::SameLine(0.0f, gap);
    ActionButton("Auto Roll", actions::Command::ToggleAutoRollMode, button_size);

    ActionButton("Reset Auto", actions::Command::ResetAutoRollMode, button_size);

    ActionButton("Switch Speed", actions::Command::SwitchReelSpeed, button_size);
    ImGui::SameLine(0.0f, gap);
    ActionButton("Change Speed", actions::Command::ChangeRollSpeed, button_size);

    ActionButton("Friction", actions::Command::ChangeFriction, button_size);
    ImGui::SameLine(0.0f, gap);
    ActionButton("Speed Mode", actions::Command::RollSpeedMode, button_size);

    ActionButton("Transmission", actions::Command::ChangeTransmissionMode, button_size);
    ImGui::SameLine(0.0f, gap);
    ActionButton("Engine", actions::Command::ToggleEngine, button_size);

    ActionButton("Toggle Gearbox", actions::Command::ToggleTransmission, button_size);

    ImGui::Spacing();
    ImGui::SeparatorText("Sandbox Debug");
    ActionButton("Catch Fish", actions::Command::DebugCatchFish, button_size);
    ImGui::SameLine(0.0f, gap);
    ActionButton("Repair Rod", actions::Command::DebugRepairRod, button_size);

    ActionButton("Spawn Fish", actions::Command::DebugSpawnFish, button_size);
    ImGui::SameLine(0.0f, gap);
    ActionButton("Fish Jump", actions::Command::DebugFishJump, button_size);

    ActionButton("Level Up", actions::Command::DebugLevelUp, button_size);
    ImGui::SameLine(0.0f, gap);
    ActionButton("Debug Hitch", actions::Command::DebugHitch, button_size);

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

    ActionButton("Snapshot", actions::Command::SnapshotDiagnostics, button_size);
    ImGui::SameLine(0.0f, gap);
    ActionButton(
        status.diagnostics_enabled ? "Stop Log" : "Start Log",
        actions::Command::ToggleDiagnostics,
        button_size);

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
