#pragma once

#include <string>

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
};

struct Status {
    bool running = false;
    bool ready = false;
    bool busy = false;
    bool diagnostics_enabled = false;
    bool auto_reel_enabled = false;
    unsigned int queued = 0;
    unsigned long long diagnostic_snapshots = 0;
    unsigned long long auto_reel_ticks = 0;
    std::string message;
    std::string last_command;
    std::string last_result;
    bool last_effect_confirmed = false;
    std::string last_observation;
    std::string probe_summary;
    std::string sensor_summary;
};

bool Start();
void Stop();
void Queue(Command command);
Status GetStatus();

} // namespace fc::actions
