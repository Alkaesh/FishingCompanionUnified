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
    const ImVec4 state_color = status.ready ? RGBA(0x31D3C6FF) : RGBA(0xFFCF66FF);

    ImGui::SeparatorText("Runtime");
    ImGui::TextColored(state_color, "%s", status.ready ? "ready" : "waiting");
    ImGui::SameLine();
    ImGui::TextColored(RGBA(0x7F91A0FF), "queued: %u", status.queued);

    if (!status.message.empty())
        ImGui::TextWrapped("%s", status.message.c_str());

    if (!status.last_command.empty())
        ImGui::TextColored(
            status.last_effect_confirmed ? RGBA(0x31D3C6FF) : RGBA(0xFFCF66FF),
            "last: %s / %s",
            status.last_command.c_str(),
            status.last_result.c_str());
    if (!status.last_observation.empty())
        ImGui::TextWrapped("observed: %s", status.last_observation.c_str());

    ImGui::Spacing();
    if (ActionButton("Refresh", actions::Command::Refresh, ImVec2(118.0f, 32.0f)))
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
    ImGui::PushStyleColor(ImGuiCol_Button, RGBA(0x1C5A62FF));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, RGBA(0x267B82FF));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, RGBA(0x31D3C6FF));
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
    ImGui::SeparatorText("Reel");
    ImGui::TextColored(
        status.auto_reel_enabled ? RGBA(0x31D3C6FF) : RGBA(0x7F91A0FF),
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
        status.last_effect_confirmed ? RGBA(0x31D3C6FF) : RGBA(0xFFCF66FF),
        "Observed in game: %s",
        status.last_effect_confirmed ? "confirmed" : "not confirmed");
    ImGui::TextColored(
        status.diagnostics_enabled ? RGBA(0x31D3C6FF) : RGBA(0x7F91A0FF),
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
}

} // namespace fc
