// ============================================================================
//  ActionsTable.h - single data-driven source for the Actions tab buttons.
// ----------------------------------------------------------------------------
//  Replaces the old double source of truth in ActionsTab.cpp (a separate
//  kSearchActions[] array plus a hand-written button layout). Now one table
//  drives both the button grid (grouped, in declaration order within a group)
//  and the top search results. Add or remove a command here only.
//
//  Categories (Group) are defined here because fc_actions has no notion of
//  command grouping - the enum is flat. Keep the group set small and stable.
//
//  Render order is by Group (see kActionGroupOrder), then by the order entries
//  appear within each group in this table. The button grid layout mirrors the
//  previous hand-written ActionsTab exactly.
// ============================================================================

#pragma once

#include "../Actions/ActionRuntime.h"

namespace fc::gui {

// Visual grouping for the Actions grid. Order here = vertical section order.
enum class ActionGroup
{
    Fishing,
    Auto,
    Spots,
    CatchResult,
    Reel,
    Sandbox,
    Diagnostics,
    Navigation, // quick jump-to-tab entries, surfaced only by the command search
};

// Button visual treatment.
enum class ActionStyle
{
    Normal,
    Primary,
    Danger,
};

struct ActionSpec
{
    const char* label;
    actions::Command command;
    const char* keywords;       // extra search terms beyond the label
    ActionGroup group;
    ActionStyle style = ActionStyle::Normal;
    // When non-null, this entry is a quick navigation jump to the tab whose
    // Title() matches navTabTitle (instead of queuing a command). Used by the
    // command search dropdown; never rendered in the Actions grid.
    const char* navTabTitle = nullptr;
};

// Section display order.
constexpr ActionGroup kActionGroupOrder[] = {
    ActionGroup::Fishing,
    ActionGroup::Auto,
    ActionGroup::Spots,
    ActionGroup::CatchResult,
    ActionGroup::Reel,
    ActionGroup::Sandbox,
    ActionGroup::Diagnostics,
};

// Human-readable section title for a group.
inline const char* GroupTitle(ActionGroup group)
{
    switch (group)
    {
    case ActionGroup::Fishing:    return "Fishing";
    case ActionGroup::Auto:       return "Automation";
    case ActionGroup::Spots:      return "Spots & Scan";
    case ActionGroup::CatchResult:return "Catch Result";
    case ActionGroup::Reel:       return "Reel";
    case ActionGroup::Sandbox:    return "Sandbox Debug";
    case ActionGroup::Diagnostics:return "Diagnostics";
    case ActionGroup::Navigation: return "Navigation";
    }
    return "Actions";
}

// The full action catalog. Order within a group is the button order.
constexpr ActionSpec kActions[] = {
    // -- Fishing ---------------------------------------------------------
    {"Hitch",            actions::Command::Hitch,              "fishing hitch"},
    {"Start Hooking",    actions::Command::StartHooking,       "fishing start_hooking hook"},
    {"Alternative",      actions::Command::AlternativeAction,  "fishing alternative alt action"},
    {"Podsak",           actions::Command::TogglePodsak,       "fishing podsak toggle_podsak"},
    {"Toggle Reel",      actions::Command::ToggleReel,         "fishing reel toggle_reel"},
    {"Set Toggle Reel",  actions::Command::FishingSetToggleReel,"fishing set toggle reel"},
    {"Cut Line",         actions::Command::CutFishingLine,     "fishing cut line cut_fishing_line"},
    {"Return Idle",      actions::Command::ReturnIdle,         "fishing return idle return_idle"},
    {"Switch Throw Mode",actions::Command::SwitchThrowMode,    "fishing throw mode switch"},
    {"Change Distance",  actions::Command::ChangeThrowDistance,"fishing throw distance change"},
    {"Set Clip",         actions::Command::FishingSetClip,     "fishing set clip"},
    {"Rig Clip",         actions::Command::RigClip,            "fishing rig clip"},
    {"Bait 1",           actions::Command::HotSwapBait1,       "fishing bait hotswap hot swap 1"},
    {"Bait 2",           actions::Command::HotSwapBait2,       "fishing bait hotswap hot swap 2"},
    {"Bobber Depth",     actions::Command::ChangeBobberDepth,  "fishing bobber depth"},
    {"Rod Rest",         actions::Command::RodToRodrest,       "fishing rod rest rodrest"},
    {"Rod Slot",         actions::Command::RodSlot,            "fishing rod slot"},
    {"Hand HotSwap",     actions::Command::HandItemHotSwap,    "fishing hand item hotswap hot swap"},

    // -- Automation (primary/danger styled) ------------------------------
    {"Auto Cast",        actions::Command::AutoCast,           "auto cast autocast", ActionGroup::Auto, ActionStyle::Primary},
    {"Auto Catch",       actions::Command::AutoCatch,          "auto catch autocatch", ActionGroup::Auto, ActionStyle::Primary},
    {"Auto Fish",        actions::Command::ToggleAutoFish,     "auto fish autofish autonomous toggle start stop", ActionGroup::Auto, ActionStyle::Primary},
    {"Auto Scout",       actions::Command::AutoScout,          "auto scout autoscout scout_cast", ActionGroup::Auto, ActionStyle::Primary},
    {"Stop All",         actions::Command::StopAll,            "stop all cancel halt", ActionGroup::Auto, ActionStyle::Danger},
    // Full autonomous fishing FSM (cast -> bite -> hook -> fight -> catch -> repeat).
    {"Toggle Auto Fish", actions::Command::ToggleAutoFish,     "auto fish autonomous fsm cast catch fight full", ActionGroup::Auto, ActionStyle::Primary},

    // -- Spots & Scan ----------------------------------------------------
    {"Mark Spot",        actions::Command::MarkSpot,           "mark spot"},
    {"Clear Spot",       actions::Command::ClearSpot,          "clear spot"},
    {"Scan Fish",        actions::Command::ScanFish,           "scan fish fish_scan"},

    // -- Catch Result ----------------------------------------------------
    {"Keep Fish",        actions::Command::KeepFish,           "keep fish catch result"},
    {"Release Fish",     actions::Command::ReleaseFish,        "release fish catch result"},
    {"Continue Fishing", actions::Command::ContinueFishing,    "continue fishing keep and cast keep_and_cast"},

    // -- Reel ------------------------------------------------------------
    {"Manual Roll",      actions::Command::ManualRoll,         "reel manual roll"},
    {"Roll Boost",       actions::Command::ManualRollBoost,    "reel manual roll boost"},
    {"Toggle Auto Reel", actions::Command::ToggleAutoReel,     "reel auto reel toggle start stop"},
    {"Auto Roll",        actions::Command::ToggleAutoRollMode, "reel auto roll mode"},
    {"Reset Auto",       actions::Command::ResetAutoRollMode,  "reel reset auto roll"},
    {"Switch Speed",     actions::Command::SwitchReelSpeed,    "reel switch speed"},
    {"Change Speed",     actions::Command::ChangeRollSpeed,    "reel change speed"},
    {"Friction",         actions::Command::ChangeFriction,     "reel friction"},
    {"Speed Mode",       actions::Command::RollSpeedMode,      "reel speed mode"},
    {"Transmission",     actions::Command::ChangeTransmissionMode,"reel transmission mode"},
    {"Engine",           actions::Command::ToggleEngine,       "reel engine"},
    {"Toggle Gearbox",   actions::Command::ToggleTransmission, "reel gearbox transmission"},

    // -- Sandbox Debug ---------------------------------------------------
    {"Catch Fish",       actions::Command::DebugCatchFish,     "sandbox debug catch fish"},
    {"Repair Rod",       actions::Command::DebugRepairRod,     "sandbox debug repair rod"},
    {"Spawn Fish",       actions::Command::DebugSpawnFish,     "sandbox debug spawn fish"},
    {"Fish Jump",        actions::Command::DebugFishJump,      "sandbox debug fish jump"},
    {"Level Up",         actions::Command::DebugLevelUp,       "sandbox debug level up"},
    {"Debug Hitch",      actions::Command::DebugHitch,         "sandbox debug hitch"},

    // -- Diagnostics -----------------------------------------------------
    {"Refresh",          actions::Command::Refresh,            "runtime status refresh update actions ready", ActionGroup::Diagnostics},
    {"Snapshot",         actions::Command::SnapshotDiagnostics,"diagnostics snapshot", ActionGroup::Diagnostics},
    {"Toggle Diagnostics",actions::Command::ToggleDiagnostics, "diagnostics log start stop", ActionGroup::Diagnostics},
};

constexpr size_t kActionCount = sizeof(kActions) / sizeof(kActions[0]);

// True for commands that are meaningful even before the runtime reports ready
// (maintenance controls). Mirrors the previous ActionsTab whitelist exactly.
inline bool CanRunWhenNotReady(actions::Command command)
{
    switch (command)
    {
    case actions::Command::Refresh:
    case actions::Command::StopAll:
    case actions::Command::SnapshotDiagnostics:
    case actions::Command::ToggleDiagnostics:
        return true;
    default:
        return false;
    }
}

// Quick navigation entries for the command search. Each jumps to a built-in tab
// (matched by its Title()). The command field is unused for these; navTabTitle
// is what makes them navigation actions instead of commands.
constexpr ActionSpec kQuickNav[] = {
    {"Open Dashboard", actions::Command::Refresh, "go to overview home main",         ActionGroup::Navigation, ActionStyle::Normal, "Dashboard"},
    {"Open Actions",   actions::Command::Refresh, "go to commands fishing reel queue",ActionGroup::Navigation, ActionStyle::Normal, "Actions"},
    {"Open Logs",      actions::Command::Refresh, "go to events viewer log",          ActionGroup::Navigation, ActionStyle::Normal, "Logs"},
    {"Open Health",    actions::Command::Refresh, "go to diagnostics status health",  ActionGroup::Navigation, ActionStyle::Normal, "Health"},
    {"Open Settings",  actions::Command::Refresh, "go to hotkeys scale appearance",   ActionGroup::Navigation, ActionStyle::Normal, "Settings"},
    {"Open SDK",       actions::Command::Refresh, "go to modules plugins sdk",        ActionGroup::Navigation, ActionStyle::Normal, "SDK"},
};

constexpr size_t kQuickNavCount = sizeof(kQuickNav) / sizeof(kQuickNav[0]);

} // namespace fc::gui
