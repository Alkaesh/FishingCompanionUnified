#pragma once

#include <string>
#include <vector>

namespace fc::actions {

enum class Command {
    Hitch,
    StartHooking,
    AlternativeAction,
    TogglePodsak,
    ToggleReel,
    FishingSetToggleReel,
    CutFishingLine,
    FishingSetClip,
    RigClip,
    HotSwapBait1,
    HotSwapBait2,
    ChangeBobberDepth,
    RodToRodrest,
    RodSlot,
    HandItemHotSwap,
    SwitchThrowMode,
    ChangeThrowDistance,
    ReturnIdle,
    AutoCast,
    AutoCatch,
    AutoScout,
    StopAll,
    // Full autonomous fishing: cast -> wait for bite -> hook -> fight -> catch
    // result -> repeat. Runs a dedicated FSM separate from the legacy auto_reel.
    ToggleAutoFish,
    MarkSpot,
    ClearSpot,
    ScanFish,
    Refresh,
    ManualRoll,
    ManualRollBoost,
    SwitchReelSpeed,
    ChangeTransmissionMode,
    ToggleAutoRollMode,
    ResetAutoRollMode,
    ChangeFriction,
    RollSpeedMode,
    ChangeRollSpeed,
    ToggleEngine,
    ToggleTransmission,
    DebugCatchFish,
    DebugRepairRod,
    DebugSpawnFish,
    DebugFishJump,
    DebugLevelUp,
    DebugHitch,
    ToggleAutoReel,
    SnapshotDiagnostics,
    ToggleDiagnostics,
    KeepFish,
    ReleaseFish,
    ContinueFishing,
};

struct Status {
    bool running = false;
    bool ready = false;
    bool busy = false;
    bool diagnostics_enabled = false;
    bool auto_reel_enabled = false;
    bool auto_fish_enabled = false;          // FSM master switch
    unsigned int queued = 0;
    unsigned long long diagnostic_snapshots = 0;
    unsigned long long auto_reel_ticks = 0;
    unsigned long long auto_fish_cycles = 0; // completed cast->catch loops
    std::string message;
    std::string last_command;
    std::string last_result;
    bool last_effect_confirmed = false;
    std::string last_observation;
    std::string probe_summary;
    std::string sensor_summary;
    std::string auto_fish_state;             // current FSM phase name
    float auto_fish_rod_load = 0.0f;         // live Rod+0x110 during FSM
    float auto_fish_reel_value = 0.0f;       // live Reel+0xA8 during FSM
    bool auto_fish_rod_in_hand = false;      // FishingSet+0x100 (InteractiveRod) != 0
    std::vector<std::string> recent_events;
};

// FSM tuning parameters. Read/written through GetAutoFishParams/SetAutoFishParams
// so the Settings tab can expose them without recompiling. Defaults are the
// values used before this knob existed.
struct AutoFishParams {
    // Bite detection (Rod+0x110 rod load / bend). A bite is declared once the
    // load exceeds bite_load_threshold for bite_confirm_ms milliseconds.
    float bite_load_threshold = 0.35f;
    int   bite_confirm_ms     = 250;
    // Above this load during the fight we treat the tackle as overloaded and
    // briefly ease off to avoid snapping the line.
    float fight_load_danger   = 0.85f;
    // Hold time for the hook set (mouse left-button / StartHooking).
    int   hook_hold_ms        = 600;
    // Delay between casting and starting to watch for a bite.
    int   post_cast_wait_ms   = 1500;
    // Max seconds to wait for a bite before re-casting.
    int   bite_timeout_s      = 120;
    // Pause between completed cycles.
    int   cycle_cooldown_ms   = 1200;
};

bool Start();
void Stop();
void Queue(Command command);
Status GetStatus();

// Autonomous fishing FSM control + tuning, exposed for the GUI/hotkey.
void SetAutoFish(bool enabled);
bool IsAutoFishEnabled();
AutoFishParams GetAutoFishParams();
void SetAutoFishParams(const AutoFishParams& params);

} // namespace fc::actions
