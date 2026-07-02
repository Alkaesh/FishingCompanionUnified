# Fishing Function Map

This file tracks the sandbox-only fishing actions and fields used by the companion DLL.

## Stable Input Actions

These are routed through Unity InputSystem `InputAction` objects and are the safest integration layer found so far.

- Fishing: `StartHooking`, `AlternativeAction`, `TogglePodsak`, `ToggleReel`
- FishingSet: `CutFishingLine`, `ToggleReel`, `ChangeReelClipPosition`, `SwitchThrowMode`, `ReturnToIdle`, `Hitch`
- FishingRig: `HotSwapBait1`, `HotSwapBait2`, `ChangeReelClipPosition`, `ChangeBobberDepth`
- HandItem: `HotSwap`, `ChangeThrowDistance`
- Interactions: `RodToRodrest`, `RodSlot`
- FishingReel: `ManualRoll`, `ManualRollBoost`, `SwitchSpeed`, `ChangeTransmissionMode`, `ToggleAutoRollMode`, `ResetAutoRollMode`, `ChangeFriction`, `RollSpeedMode`, `ChangeRollSpeed`, `ToggleEngine`, `ToggleTransmission`

## Sandbox Debug Actions

These exist in the generated input wrapper and are exposed only as sandbox test buttons/commands.

- `CatchFish`
- `RepairRod`
- `SpawnFish`
- `FishJump`
- `LevelUp`
- `Hitch`

`SpawnFish`, `FishJump`, and `CatchFish` are now treated as verified debug commands:
the input pulse must succeed and the DLL must detect a live/logical fish afterward.
If no fish is detected in the sandbox scene, the command result is reported as failed
instead of silently claiming a gameplay effect.

## Current Scenarios

- `auto_cast`: `SwitchThrowMode` -> `ChangeThrowDistance` -> `Hitch` -> `StartHooking`
- `auto_catch`: runs `auto_cast`, enables adaptive `auto_reel`, then writes a diagnostic snapshot
- `auto_fish`: **full autonomous fishing FSM** (see below) — separate from the legacy auto_reel timer
- `auto_scout`: runs `auto_cast`, enables diagnostics, marks the current lure/rod position, then writes a diagnostic snapshot
- `stop_all`: disables `auto_fish`, disables `auto_reel`, disables periodic diagnostics, then tries `ReturnToIdle`
- `auto_reel`: pulses `ManualRoll`; while fishing state is active it uses a faster interval and periodic `ManualRollBoost`
- `mark_spot`: stores the current lure position; falls back to the rod tip when the lure position is not valid
- `clear_spot`: clears the marked spot and removes spot-distance tracking
- `scan_fish`: refreshes all live `Fish` instances and writes the closest fish position/distance snapshot

## Auto Fish FSM (`auto_fish`)

A dedicated state machine in `ActionRuntime.cpp` (`perform_auto_fish_tick`) that
runs every worker-loop iteration while enabled. It is independent of the legacy
`auto_reel` timer: when `auto_fish` is on it owns the reel and forces
`auto_reel` off so the two never fight over `ManualRoll`.

Phases (`AutoFishPhase`):

1. `idle` -> `cast`
2. `cast`      : runs `perform_auto_cast`; on success captures the resting rod
   load as the bite-detection baseline, waits `post_cast_wait_ms`, -> `wait_bite`
3. `wait_bite` : declares a **bite** when `Rod+0x110` (rod load) exceeds
   `baseline + bite_load_threshold` for `bite_confirm_ms` ms (debounced). Times
   out to a re-cast after `bite_timeout_s`. -> `hooked`
4. `hooked`    : sets the hook (foreground mouse hold + `StartHooking` pulse)
   for `hook_hold_ms`. -> `fight`
5. `fight`     : reels at maximum speed (`ManualRollBoost` + RMB/LMB fight hold).
   When the rod load reaches `fight_load_danger` it **eases off** for ~450 ms so
   the line tension does not snap the tackle (manual load control). On
   `is_likely_catch_result_screen` -> `catch_result`.
6. `catch_result`: accepts the fish (`perform_continue_fishing`, which also
   re-casts), increments the cycle counter, cools down `cycle_cooldown_ms`, ->
   `idle` (the FSM owns the next cast).

Controls:

- GUI: **Auto Fish** button in the Automation group (Start/Stop), plus a live
  status line (state / cycles / rod load) and an **Auto Fish** tuning section in
  the Settings tab (all thresholds are runtime-adjustable and clamped).
- Hotkey: `F9` by default (configurable in Settings -> Hotkeys). Toggles the FSM
  hands-free, even with the menu closed, via `actions::SetAutoFish`.
- Command file: `auto_fish` / `autofish` / `toggle_auto_fish` alias.
- Public API: `fc::actions::SetAutoFish`, `IsAutoFishEnabled`,
  `GetAutoFishParams`, `SetAutoFishParams` (struct `AutoFishParams`).

## Runtime Verification

Last sandbox verification: 2026-06-25, process `rf4_x64.exe`, DLL SHA256
`C8A7BEEE8386B02483845AAACBC41B864E8F399D24EF2A5603FEFAD05BD3FF8D`.

Confirmed as command-successful in the live sandbox process:

- `refresh`, `snapshot`, `scan_fish`, `mark_spot`, `clear_spot`
- `hitch`, `start_hooking`, `toggle_reel`, `set_toggle_reel`
- `switch_throw_mode`, `change_throw_distance`, `manual_roll`, `roll_boost`
- `repair_rod`
- `auto_cast`, `auto_scout`, `auto_catch`, `stop_all`

Confirmed telemetry effects:

- `snapshot` writes full diagnostics and coordinate rows.
- `mark_spot` sets `marked=1`; `clear_spot` clears the status.
- `auto_scout` performs `auto_cast`, enables diagnostics, marks the lure spot, and writes snapshots.
- `auto_catch` performs `auto_cast`, enables adaptive auto reel, and writes a snapshot.
- `stop_all` disables diagnostics and auto reel.

Currently not confirmed as gameplay-effective:

- `spawn_fish`, `fish_jump`, and `catch_fish` pulse their sandbox debug input actions,
  but no live Unity `Fish` or active logical fish pointer is detected afterward in this scene.
  These commands intentionally report `failed` until the fish/debug spawn path is proven.

## Command File Aliases

The DLL polls `FishingCompanion_command.txt` in the game directory.

- `auto_cast`, `auto_catch`, `stop_all`
- `auto_scout`, `mark_spot`, `clear_spot`, `scan_fish`
- `hitch`, `start_hooking`, `alternative_action`, `toggle_podsak`, `toggle_reel`, `set_toggle_reel`
- `cut_line`, `set_clip`, `rig_clip`, `bait1`, `bait2`, `bobber_depth`, `rodrest`, `rod_slot`
- `manual_roll`, `roll_boost`, `switch_speed`, `auto_reel`
- `repair_rod`, `catch_fish`, `spawn_fish`, `fish_jump`, `level_up`, `debug_hitch`
- `snapshot`, `diagnostics`, `refresh`

## Observed State Fields

These are read-only diagnostics for now.

- `FishingSet + 0x150`: state-like value, observed `0x82815`
- `FishingSet + 0x158`: state-like value, observed `0x8227C`
- `FishingSet + 0x160`: state bitmask candidate, IDA shows it is written near the end of `FishingSet` method RVA `0xC928C0`
- `Rod + 0x110`: rod load/bend candidate, computed in the `Rod::FixedUpdate` path
- `Reel + 0xA8`: reel float candidate, changes during active fishing
- `Reel + 0x150`: nested reel state pointer used by `Reel::Update`
- `(Reel + 0x150) + 0x1C`, `+0x20`, `+0x28`: nested state/input/model pointers and flags used for compact live status

## Coordinate Tracking

The DLL writes compact coordinate snapshots to `FishingCompanion_fishing_coords_v4.csv`.

- `Fisher + 0x70`: primary Fisher Vector3 candidate
- `Fisher + 0x164`: secondary Fisher Vector3 candidate
- `Rod + 0x98`: rod origin/base candidate
- `Rod + 0xA4`: rod world/current vector candidate
- `Rod + 0xC8`: rod local/tip-offset candidate; IDA shows `Rod` method RVA `0x6DA1D0` returns this Vector3
- `Rod + 0xF4`: rod velocity/motion candidate
- `LureComplex + 0x4C`: local lure Vector3 candidate
- `LureComplex + 0x38 -> LureSimple + 0xE8`: raw lure world-position candidate; IDA shows `LureComplex.hednmaggbil()` returns this nested Vector3, but it can be `0/0/0` while inactive
- `LureSimple + 0xF4`: lure velocity/motion candidate
- `best_lure`: uses raw LureSimple position when non-zero; otherwise estimates `Rod+0xA4 + LureComplex+0x4C`
- `Fish + 0xD8`: primary Fish Vector3 candidate, used for nearest-fish tracking when non-zero
- `Fish + 0xCC`: alternate Fish Vector3 candidate, used as a fallback for nearest-fish tracking
- `LureComplex + 0x58`: active logical fish pointer candidate (`gclanlcddlf`)
- `FishingSet.pmdnlpafcol()` RVA `0xC8FB60`: active logical fish pointer candidate
- `FishingSet.pchlpdepaai()` RVA `0xC9B240` + `Synth_1402_Closure.pfokppcgekh(Guid)` RVA `0x810F30`: dictionary lookup for active logical fish
- `closest_fish`: selected by distance to `best_lure`; if lure is unavailable, distance to `Fisher+0x70` is used; non-Unity-object candidates are rejected by IL2CPP class name

## Next Reverse Targets

- `BiteAlarmComplex`, `BiteAlarmCarpSignal`, and `FishingSetIdOwner_FishBiteMeta` for bite detection and rod-rest workflows
- Bite/strike timing fields for when to switch from scouting to auto reel
- Durability/energy-style fields should remain read-only until exact offsets and value ranges are verified; use `Debug.RepairRod` for sandbox repair tests instead of raw writes
