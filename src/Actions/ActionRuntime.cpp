#include "ActionRuntime.h"

#include "game_actions.hpp"
#include "../Core/Overlay.h"

#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

struct ActionSet {
    void* toggle_reel = nullptr;
    void* start_hooking = nullptr;
    void* alternative_action = nullptr;
    void* toggle_podsak = nullptr;
    void* fishing_set_toggle_reel = nullptr;
    void* cut_fishing_line = nullptr;
    void* fishing_set_clip = nullptr;
    void* rig_clip = nullptr;
    void* hot_swap_bait1 = nullptr;
    void* hot_swap_bait2 = nullptr;
    void* change_bobber_depth = nullptr;
    void* rod_to_rodrest = nullptr;
    void* rod_slot = nullptr;
    void* hand_item_hot_swap = nullptr;
    void* switch_throw_mode = nullptr;
    void* hitch = nullptr;
    void* return_to_idle = nullptr;
    void* change_throw_distance = nullptr;
    void* manual_roll = nullptr;
    void* manual_roll_boost = nullptr;
    void* switch_reel_speed = nullptr;
    void* change_transmission_mode = nullptr;
    void* toggle_auto_roll_mode = nullptr;
    void* reset_auto_roll_mode = nullptr;
    void* change_friction = nullptr;
    void* roll_speed_mode = nullptr;
    void* change_roll_speed = nullptr;
    void* toggle_engine = nullptr;
    void* toggle_transmission = nullptr;
    void* debug_return_to_idle = nullptr;
    void* debug_catch_fish = nullptr;
    void* debug_repair_rod = nullptr;
    void* debug_spawn_fish = nullptr;
    void* debug_fish_jump = nullptr;
    void* debug_level_up = nullptr;
    void* debug_hitch = nullptr;
};

struct ProbeSet {
    void* fishing_set = nullptr;
    void* fisher = nullptr;
    void* rod = nullptr;
    void* reel = nullptr;
    void* lure_complex = nullptr;
};

struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct SystemGuid {
    std::uint8_t bytes[16]{};
};

struct SensorState {
    uint64_t fishing_set_150 = 0;
    uint64_t fishing_set_158 = 0;
    uint64_t fishing_set_160 = 0;
    float rod_load = 0.0f;
    float reel_value = 0.0f;
    Vector3 fisher_pos{};
    Vector3 fisher_alt_pos{};
    Vector3 rod_origin{};
    Vector3 rod_mid{};
    Vector3 rod_tip{};
    Vector3 rod_velocity{};
    Vector3 lure_local{};
    uintptr_t lure_simple = 0;
    Vector3 lure_pos{};
    Vector3 lure_estimated_pos{};
    Vector3 best_lure_pos{};
    Vector3 lure_velocity{};
    float fisher_to_lure = 0.0f;
    float rod_tip_to_lure = 0.0f;
    float rod_span = 0.0f;
    float marked_spot_distance = 0.0f;
    uintptr_t reel_state = 0;
    uintptr_t reel_state_input = 0;
    uintptr_t reel_state_model = 0;
    unsigned int reel_state_flags = 0;
    unsigned int reel_input_20 = 0;
    unsigned int reel_input_21 = 0;
    unsigned int reel_model_d0 = 0;
    unsigned int reel_model_d1 = 0;
    bool has_fishing_set = false;
    bool has_fisher = false;
    bool has_rod = false;
    bool has_reel = false;
    bool has_lure = false;
    bool has_lure_simple = false;
    bool has_best_lure_pos = false;
    bool lure_pos_from_child = false;
    bool best_lure_estimated = false;
    bool has_marked_spot = false;
    size_t fish_count = 0;
    uintptr_t fishing_setup = 0;
    uintptr_t fish_bite_meta = 0;
    uintptr_t logical_fish_lure = 0;
    uintptr_t logical_fish_set = 0;
    uintptr_t logical_fish_guid = 0;
    uintptr_t closest_fish = 0;
    int closest_fish_source = 0;
    Vector3 closest_fish_pos{};
    Vector3 closest_fish_alt_pos{};
    float closest_fish_to_lure = 0.0f;
    float closest_fish_to_fisher = 0.0f;
    bool has_closest_fish = false;
};

struct LureVectorCandidate {
    std::string path;
    uintptr_t offset = 0;
    Vector3 value{};
    float dist_to_estimated = 0.0f;
    float dist_to_fisher = 0.0f;
    float dist_to_rod = 0.0f;
};

std::atomic_bool g_running{false};
HANDLE g_thread = nullptr;
std::mutex g_mutex;
std::condition_variable g_cv;
std::deque<fc::actions::Command> g_queue;
fc::actions::Status g_status;
ActionSet g_actions;
ProbeSet g_probe;
std::atomic_bool g_diagnostics_enabled{false};
std::atomic_ullong g_diagnostic_snapshots{0};
std::chrono::steady_clock::time_point g_next_diagnostic_snapshot{};
std::atomic_bool g_auto_reel_enabled{false};
std::atomic_ullong g_auto_reel_ticks{0};
std::chrono::steady_clock::time_point g_next_auto_reel_tick{};
bool g_has_marked_spot = false;
Vector3 g_marked_spot{};
std::vector<void*> g_fish_instances;
std::chrono::steady_clock::time_point g_next_fish_scan{};

constexpr uintptr_t k_synth_1402_pfokppcgekh_method = 0x810F30;
constexpr uintptr_t k_synth_1402_bjkkdngcmfm_method = 0x814BD0;
constexpr uintptr_t k_synth_3787_jnbclacafdf_method = 0xD5F500;
constexpr uintptr_t k_synth_5570_string_ctor_method = 0x11B5DB0;
constexpr uintptr_t k_fishing_set_setup_field = 0x68;
constexpr uintptr_t k_synth_5570_fish_bite_meta_field = 0x30;
constexpr uintptr_t k_fishing_set_rig_connector_field = 0x90;
constexpr uintptr_t k_fish_bite_meta_owner_value_field = 0x20;
constexpr uintptr_t k_codegen_init_runtime_metadata_method = 0x328770;
constexpr uintptr_t k_internal_object_new_method = 0x35BFF0;
constexpr uintptr_t k_internal_array_new_specific_method = 0x35B310;
constexpr uintptr_t k_runtime_class_init_method = 0x36EE00;
constexpr uintptr_t k_gkfghccolil_class_global = 0x3E4CDC8;
constexpr uintptr_t k_hlbkfjnodei_owner_array_class_global = 0x3E96F98;
constexpr uintptr_t k_fish_size_params_class_global = 0x3EC09C0;
constexpr uintptr_t k_synth_1402_class_global = 0x3EB76D0;
constexpr uintptr_t k_fish_spawn_mode_class_global = 0x3ED92C0;
constexpr uintptr_t k_fish_position_source_class_global = 0x3EBCC18;
constexpr uintptr_t k_fish_position_bounds_class_global = 0x3EBCC20;
constexpr uintptr_t k_lure_simple_state_field = 0x30;
constexpr uintptr_t k_lure_state_world_position_field = 0xC0;
constexpr uintptr_t k_lure_simple_legacy_position_field = 0xE8;

const char* command_name(fc::actions::Command command);
ProbeSet capture_probe();

std::wstring process_directory()
{
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring value(path);
    const auto pos = value.find_last_of(L"\\/");
    return pos == std::wstring::npos ? L"." : value.substr(0, pos);
}

void log_line(const std::string& text)
{
    const std::filesystem::path path =
        std::filesystem::path(process_directory()) / L"FishingCompanion_actions.log";
    std::ofstream out(path, std::ios::out | std::ios::app);
    if (out)
        out << text << '\n';
}

bool sleep_interruptible(DWORD total_ms, DWORD step_ms = 50)
{
    if (total_ms == 0)
        return g_running.load();

    DWORD elapsed = 0;
    const DWORD step = step_ms == 0 ? total_ms : step_ms;
    while (g_running.load() && elapsed < total_ms) {
        const DWORD chunk = std::min(step, total_ms - elapsed);
        Sleep(chunk);
        elapsed += chunk;
    }
    return g_running.load();
}

std::string normalize_command_text(std::string text)
{
    text.erase(
        std::remove_if(text.begin(), text.end(), [](unsigned char ch) {
            return ch == '\r' || ch == '\n' || ch == '\t' || ch == ' ';
        }),
        text.end());

    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        if (ch == '-' || ch == '.')
            return '_';
        return static_cast<char>(std::tolower(ch));
    });

    return text;
}

std::optional<fc::actions::Command> command_from_text(const std::string& raw)
{
    const std::string text = normalize_command_text(raw);

    if (text == "hitch")
        return fc::actions::Command::Hitch;
    if (text == "start_hooking" || text == "starthooking")
        return fc::actions::Command::StartHooking;
    if (text == "alternative_action" || text == "alt_action" || text == "alternative")
        return fc::actions::Command::AlternativeAction;
    if (text == "toggle_podsak" || text == "podsack" || text == "podsak")
        return fc::actions::Command::TogglePodsak;
    if (text == "toggle_reel" || text == "togglereel")
        return fc::actions::Command::ToggleReel;
    if (text == "fishing_set_toggle_reel" || text == "set_toggle_reel")
        return fc::actions::Command::FishingSetToggleReel;
    if (text == "cut_fishing_line" || text == "cut_line")
        return fc::actions::Command::CutFishingLine;
    if (text == "fishing_set_clip" || text == "set_clip")
        return fc::actions::Command::FishingSetClip;
    if (text == "rig_clip")
        return fc::actions::Command::RigClip;
    if (text == "hot_swap_bait1" || text == "bait1")
        return fc::actions::Command::HotSwapBait1;
    if (text == "hot_swap_bait2" || text == "bait2")
        return fc::actions::Command::HotSwapBait2;
    if (text == "change_bobber_depth" || text == "bobber_depth")
        return fc::actions::Command::ChangeBobberDepth;
    if (text == "rod_to_rodrest" || text == "rodrest")
        return fc::actions::Command::RodToRodrest;
    if (text == "rod_slot")
        return fc::actions::Command::RodSlot;
    if (text == "hand_item_hot_swap" || text == "hand_hot_swap")
        return fc::actions::Command::HandItemHotSwap;
    if (text == "switch_throw_mode" || text == "switchthrowmode")
        return fc::actions::Command::SwitchThrowMode;
    if (text == "change_throw_distance" || text == "changethrowdistance")
        return fc::actions::Command::ChangeThrowDistance;
    if (text == "return_idle" || text == "returntoidle")
        return fc::actions::Command::ReturnIdle;
    if (text == "auto_cast" || text == "autocast")
        return fc::actions::Command::AutoCast;
    if (text == "auto_catch" || text == "autocatch")
        return fc::actions::Command::AutoCatch;
    if (text == "auto_scout" || text == "autoscout" || text == "scout_cast")
        return fc::actions::Command::AutoScout;
    if (text == "stop_all" || text == "stop")
        return fc::actions::Command::StopAll;
    if (text == "mark_spot" || text == "mark")
        return fc::actions::Command::MarkSpot;
    if (text == "clear_spot" || text == "unmark")
        return fc::actions::Command::ClearSpot;
    if (text == "scan_fish" || text == "fish_scan" || text == "fish")
        return fc::actions::Command::ScanFish;
    if (text == "refresh")
        return fc::actions::Command::Refresh;
    if (text == "manual_roll" || text == "manualroll")
        return fc::actions::Command::ManualRoll;
    if (text == "manual_roll_boost" || text == "manualrollboost" || text == "roll_boost")
        return fc::actions::Command::ManualRollBoost;
    if (text == "switch_reel_speed" || text == "switch_speed" || text == "switchspeed")
        return fc::actions::Command::SwitchReelSpeed;
    if (text == "change_transmission_mode" || text == "transmission")
        return fc::actions::Command::ChangeTransmissionMode;
    if (text == "toggle_auto_roll_mode" || text == "auto_roll")
        return fc::actions::Command::ToggleAutoRollMode;
    if (text == "reset_auto_roll_mode" || text == "reset_auto")
        return fc::actions::Command::ResetAutoRollMode;
    if (text == "change_friction" || text == "friction")
        return fc::actions::Command::ChangeFriction;
    if (text == "roll_speed_mode" || text == "speed_mode")
        return fc::actions::Command::RollSpeedMode;
    if (text == "change_roll_speed" || text == "change_speed")
        return fc::actions::Command::ChangeRollSpeed;
    if (text == "toggle_engine" || text == "engine")
        return fc::actions::Command::ToggleEngine;
    if (text == "toggle_transmission" || text == "gearbox")
        return fc::actions::Command::ToggleTransmission;
    if (text == "debug_catch_fish" || text == "catch_fish")
        return fc::actions::Command::DebugCatchFish;
    if (text == "debug_repair_rod" || text == "repair_rod")
        return fc::actions::Command::DebugRepairRod;
    if (text == "debug_spawn_fish" || text == "spawn_fish")
        return fc::actions::Command::DebugSpawnFish;
    if (text == "debug_fish_jump" || text == "fish_jump")
        return fc::actions::Command::DebugFishJump;
    if (text == "debug_level_up" || text == "level_up")
        return fc::actions::Command::DebugLevelUp;
    if (text == "debug_hitch")
        return fc::actions::Command::DebugHitch;
    if (text == "toggle_auto_reel" || text == "auto_reel")
        return fc::actions::Command::ToggleAutoReel;
    if (text == "snapshot" || text == "snapshot_diagnostics")
        return fc::actions::Command::SnapshotDiagnostics;
    if (text == "toggle_diagnostics" || text == "diagnostics")
        return fc::actions::Command::ToggleDiagnostics;

    return std::nullopt;
}

void process_command_file()
{
    const std::filesystem::path path =
        std::filesystem::path(process_directory()) / L"FishingCompanion_command.txt";

    std::error_code ec;
    if (!std::filesystem::exists(path, ec))
        return;

    std::ifstream in(path);
    std::string text;
    std::getline(in, text);
    in.close();

    std::filesystem::remove(path, ec);

    const auto command = command_from_text(text);
    if (!command) {
        log_line("command_file: unknown command: " + text);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_queue.push_back(*command);
        g_status.queued = static_cast<unsigned int>(g_queue.size());
        g_status.message = std::string("queued from file: ") + command_name(*command);
        g_status.auto_reel_enabled = g_auto_reel_enabled;
        g_status.auto_reel_ticks = g_auto_reel_ticks;
    }
}

void set_message(const std::string& message)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_status.message = message;
    g_status.running = g_running.load();
    g_status.diagnostics_enabled = g_diagnostics_enabled;
    g_status.diagnostic_snapshots = g_diagnostic_snapshots;
    g_status.auto_reel_enabled = g_auto_reel_enabled;
    g_status.auto_reel_ticks = g_auto_reel_ticks;
}

const char* command_name(fc::actions::Command command)
{
    switch (command) {
    case fc::actions::Command::Hitch:
        return "hitch";
    case fc::actions::Command::StartHooking:
        return "start_hooking";
    case fc::actions::Command::AlternativeAction:
        return "alternative_action";
    case fc::actions::Command::TogglePodsak:
        return "toggle_podsak";
    case fc::actions::Command::ToggleReel:
        return "toggle_reel";
    case fc::actions::Command::FishingSetToggleReel:
        return "fishing_set_toggle_reel";
    case fc::actions::Command::CutFishingLine:
        return "cut_fishing_line";
    case fc::actions::Command::FishingSetClip:
        return "fishing_set_clip";
    case fc::actions::Command::RigClip:
        return "rig_clip";
    case fc::actions::Command::HotSwapBait1:
        return "hot_swap_bait1";
    case fc::actions::Command::HotSwapBait2:
        return "hot_swap_bait2";
    case fc::actions::Command::ChangeBobberDepth:
        return "change_bobber_depth";
    case fc::actions::Command::RodToRodrest:
        return "rod_to_rodrest";
    case fc::actions::Command::RodSlot:
        return "rod_slot";
    case fc::actions::Command::HandItemHotSwap:
        return "hand_item_hot_swap";
    case fc::actions::Command::SwitchThrowMode:
        return "switch_throw_mode";
    case fc::actions::Command::ChangeThrowDistance:
        return "change_throw_distance";
    case fc::actions::Command::ReturnIdle:
        return "return_idle";
    case fc::actions::Command::AutoCast:
        return "auto_cast";
    case fc::actions::Command::AutoCatch:
        return "auto_catch";
    case fc::actions::Command::AutoScout:
        return "auto_scout";
    case fc::actions::Command::StopAll:
        return "stop_all";
    case fc::actions::Command::MarkSpot:
        return "mark_spot";
    case fc::actions::Command::ClearSpot:
        return "clear_spot";
    case fc::actions::Command::ScanFish:
        return "scan_fish";
    case fc::actions::Command::Refresh:
        return "refresh";
    case fc::actions::Command::ManualRoll:
        return "manual_roll";
    case fc::actions::Command::ManualRollBoost:
        return "manual_roll_boost";
    case fc::actions::Command::SwitchReelSpeed:
        return "switch_reel_speed";
    case fc::actions::Command::ChangeTransmissionMode:
        return "change_transmission_mode";
    case fc::actions::Command::ToggleAutoRollMode:
        return "toggle_auto_roll_mode";
    case fc::actions::Command::ResetAutoRollMode:
        return "reset_auto_roll_mode";
    case fc::actions::Command::ChangeFriction:
        return "change_friction";
    case fc::actions::Command::RollSpeedMode:
        return "roll_speed_mode";
    case fc::actions::Command::ChangeRollSpeed:
        return "change_roll_speed";
    case fc::actions::Command::ToggleEngine:
        return "toggle_engine";
    case fc::actions::Command::ToggleTransmission:
        return "toggle_transmission";
    case fc::actions::Command::DebugCatchFish:
        return "debug_catch_fish";
    case fc::actions::Command::DebugRepairRod:
        return "debug_repair_rod";
    case fc::actions::Command::DebugSpawnFish:
        return "debug_spawn_fish";
    case fc::actions::Command::DebugFishJump:
        return "debug_fish_jump";
    case fc::actions::Command::DebugLevelUp:
        return "debug_level_up";
    case fc::actions::Command::DebugHitch:
        return "debug_hitch";
    case fc::actions::Command::ToggleAutoReel:
        return "toggle_auto_reel";
    case fc::actions::Command::SnapshotDiagnostics:
        return "snapshot_diagnostics";
    case fc::actions::Command::ToggleDiagnostics:
        return "toggle_diagnostics";
    }

    return "unknown";
}

void* action_for_command(fc::actions::Command command, const ActionSet& actions)
{
    switch (command) {
    case fc::actions::Command::Hitch:
        return actions.hitch;
    case fc::actions::Command::StartHooking:
        return actions.start_hooking;
    case fc::actions::Command::AlternativeAction:
        return actions.alternative_action;
    case fc::actions::Command::TogglePodsak:
        return actions.toggle_podsak;
    case fc::actions::Command::ToggleReel:
        return actions.toggle_reel;
    case fc::actions::Command::FishingSetToggleReel:
        return actions.fishing_set_toggle_reel;
    case fc::actions::Command::CutFishingLine:
        return actions.cut_fishing_line;
    case fc::actions::Command::FishingSetClip:
        return actions.fishing_set_clip;
    case fc::actions::Command::RigClip:
        return actions.rig_clip;
    case fc::actions::Command::HotSwapBait1:
        return actions.hot_swap_bait1;
    case fc::actions::Command::HotSwapBait2:
        return actions.hot_swap_bait2;
    case fc::actions::Command::ChangeBobberDepth:
        return actions.change_bobber_depth;
    case fc::actions::Command::RodToRodrest:
        return actions.rod_to_rodrest;
    case fc::actions::Command::RodSlot:
        return actions.rod_slot;
    case fc::actions::Command::HandItemHotSwap:
        return actions.hand_item_hot_swap;
    case fc::actions::Command::SwitchThrowMode:
        return actions.switch_throw_mode;
    case fc::actions::Command::ChangeThrowDistance:
        return actions.change_throw_distance;
    case fc::actions::Command::ReturnIdle:
        return actions.return_to_idle;
    case fc::actions::Command::ManualRoll:
        return actions.manual_roll;
    case fc::actions::Command::ManualRollBoost:
        return actions.manual_roll_boost;
    case fc::actions::Command::SwitchReelSpeed:
        return actions.switch_reel_speed;
    case fc::actions::Command::ChangeTransmissionMode:
        return actions.change_transmission_mode;
    case fc::actions::Command::ToggleAutoRollMode:
        return actions.toggle_auto_roll_mode;
    case fc::actions::Command::ResetAutoRollMode:
        return actions.reset_auto_roll_mode;
    case fc::actions::Command::ChangeFriction:
        return actions.change_friction;
    case fc::actions::Command::RollSpeedMode:
        return actions.roll_speed_mode;
    case fc::actions::Command::ChangeRollSpeed:
        return actions.change_roll_speed;
    case fc::actions::Command::ToggleEngine:
        return actions.toggle_engine;
    case fc::actions::Command::ToggleTransmission:
        return actions.toggle_transmission;
    case fc::actions::Command::DebugCatchFish:
        return actions.debug_catch_fish;
    case fc::actions::Command::DebugRepairRod:
        return actions.debug_repair_rod;
    case fc::actions::Command::DebugSpawnFish:
        return actions.debug_spawn_fish;
    case fc::actions::Command::DebugFishJump:
        return actions.debug_fish_jump;
    case fc::actions::Command::DebugLevelUp:
        return actions.debug_level_up;
    case fc::actions::Command::DebugHitch:
        return actions.debug_hitch;
    default:
        return nullptr;
    }
}

bool has_required_actions(const ActionSet& actions)
{
    return actions.toggle_reel &&
           actions.start_hooking &&
           actions.switch_throw_mode &&
           actions.hitch &&
           actions.return_to_idle &&
           actions.change_throw_distance;
}

void enable_actions(const ActionSet& actions)
{
    game_actions::enable_input_action(actions.toggle_reel);
    game_actions::enable_input_action(actions.start_hooking);
    game_actions::enable_input_action(actions.alternative_action);
    game_actions::enable_input_action(actions.toggle_podsak);
    game_actions::enable_input_action(actions.fishing_set_toggle_reel);
    game_actions::enable_input_action(actions.cut_fishing_line);
    game_actions::enable_input_action(actions.fishing_set_clip);
    game_actions::enable_input_action(actions.rig_clip);
    game_actions::enable_input_action(actions.hot_swap_bait1);
    game_actions::enable_input_action(actions.hot_swap_bait2);
    game_actions::enable_input_action(actions.change_bobber_depth);
    game_actions::enable_input_action(actions.rod_to_rodrest);
    game_actions::enable_input_action(actions.rod_slot);
    game_actions::enable_input_action(actions.hand_item_hot_swap);
    game_actions::enable_input_action(actions.switch_throw_mode);
    game_actions::enable_input_action(actions.hitch);
    game_actions::enable_input_action(actions.return_to_idle);
    game_actions::enable_input_action(actions.change_throw_distance);
    game_actions::enable_input_action(actions.manual_roll);
    game_actions::enable_input_action(actions.manual_roll_boost);
    game_actions::enable_input_action(actions.switch_reel_speed);
    game_actions::enable_input_action(actions.change_transmission_mode);
    game_actions::enable_input_action(actions.toggle_auto_roll_mode);
    game_actions::enable_input_action(actions.reset_auto_roll_mode);
    game_actions::enable_input_action(actions.change_friction);
    game_actions::enable_input_action(actions.roll_speed_mode);
    game_actions::enable_input_action(actions.change_roll_speed);
    game_actions::enable_input_action(actions.toggle_engine);
    game_actions::enable_input_action(actions.toggle_transmission);
    game_actions::enable_input_action(actions.debug_return_to_idle);
    game_actions::enable_input_action(actions.debug_catch_fish);
    game_actions::enable_input_action(actions.debug_repair_rod);
    game_actions::enable_input_action(actions.debug_spawn_fish);
    game_actions::enable_input_action(actions.debug_fish_jump);
    game_actions::enable_input_action(actions.debug_level_up);
    game_actions::enable_input_action(actions.debug_hitch);
}

void log_ptr(const char* name, void* ptr)
{
    char buffer[128]{};
    sprintf_s(buffer, "%s = 0x%p", name, ptr);
    log_line(buffer);
}

std::filesystem::path diagnostic_path()
{
    return std::filesystem::path(process_directory()) / L"FishingCompanion_fishing_diag_full.csv";
}

std::filesystem::path coordinate_path()
{
    return std::filesystem::path(process_directory()) / L"FishingCompanion_fishing_coords_v4.csv";
}

std::string hex_ptr(void* ptr)
{
    std::ostringstream out;
    out << "0x" << std::uppercase << std::hex << std::setw(16) << std::setfill('0')
        << reinterpret_cast<uintptr_t>(ptr);
    return out.str();
}

std::string hex_u64(uint64_t value)
{
    std::ostringstream out;
    out << "0x" << std::uppercase << std::hex << std::setw(16) << std::setfill('0')
        << value;
    return out.str();
}

bool valid_vector(const Vector3& value)
{
    return std::isfinite(value.x) &&
           std::isfinite(value.y) &&
           std::isfinite(value.z) &&
           std::fabs(value.x) < 1000000.0f &&
           std::fabs(value.y) < 1000000.0f &&
           std::fabs(value.z) < 1000000.0f;
}

bool near_zero_vector(const Vector3& value)
{
    return std::fabs(value.x) < 0.001f &&
           std::fabs(value.y) < 0.001f &&
           std::fabs(value.z) < 0.001f;
}

Vector3 add_vectors(const Vector3& a, const Vector3& b)
{
    return Vector3{a.x + b.x, a.y + b.y, a.z + b.z};
}

float distance_between(const Vector3& a, const Vector3& b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

std::string format_vec(const Vector3& value)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(2)
        << value.x << '/' << value.y << '/' << value.z;
    return out.str();
}

bool zero_guid(const SystemGuid& guid)
{
    for (std::uint8_t byte : guid.bytes) {
        if (byte != 0)
            return false;
    }
    return true;
}

bool copy_object_class_names_guarded(
    void* (*object_get_class)(void*),
    const char* (*class_get_name)(void*),
    const char* (*class_get_namespace)(void*),
    void* object,
    char* name,
    size_t name_size,
    char* namespaze,
    size_t namespace_size)
{
    __try {
        void* klass = object_get_class(object);
        if (!klass)
            return false;

        const char* raw_name = class_get_name(klass);
        const char* raw_namespace = class_get_namespace(klass);
        if (!raw_name || !raw_namespace)
            return false;

        strncpy_s(name, name_size, raw_name, _TRUNCATE);
        strncpy_s(namespaze, namespace_size, raw_namespace, _TRUNCATE);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool get_fishing_set_guid_guarded(
    SystemGuid* guid,
    SystemGuid* (*fn)(SystemGuid*, void*),
    void* fishing_set)
{
    __try {
        fn(guid, fishing_set);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool fish_from_guid_guarded(
    void* (*fn)(const SystemGuid*, void*, void*),
    const SystemGuid* guid,
    uintptr_t* result)
{
    if (result)
        *result = 0;

    __try {
        if (result)
            *result = reinterpret_cast<uintptr_t>(fn(guid, nullptr, nullptr));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool object_is_fish(void* object)
{
    if (!object)
        return false;
    if (!il2cpp_runtime::is_readable_span(object, sizeof(void*)))
        return false;

    il2cpp_runtime::Module module;
    if (!module.valid())
        return false;

    auto object_get_class = reinterpret_cast<void* (*)(void*)>(
        GetProcAddress(module.handle(), "il2cpp_object_get_class"));
    auto class_get_name = reinterpret_cast<const char* (*)(void*)>(
        GetProcAddress(module.handle(), "il2cpp_class_get_name"));
    auto class_get_namespace = reinterpret_cast<const char* (*)(void*)>(
        GetProcAddress(module.handle(), "il2cpp_class_get_namespace"));
    if (!object_get_class || !class_get_name || !class_get_namespace)
        return false;

    char name[96]{};
    char namespaze[160]{};
    if (!copy_object_class_names_guarded(
            object_get_class,
            class_get_name,
            class_get_namespace,
            object,
            name,
            sizeof(name),
            namespaze,
            sizeof(namespaze))) {
        return false;
    }

    return std::string(name) == "Fish" &&
        std::string(namespaze) == "RF4.Client.FishingScene";
}

uintptr_t fishing_set_active_fish(void* fishing_set)
{
    if (!fishing_set)
        return 0;

    il2cpp_runtime::InstanceMethod<void*> getFish(
        action_offsets::RF4_Client_FishingScene_FishingSet_pmdnlpafcol_0_method);
    return reinterpret_cast<uintptr_t>(getFish.call(fishing_set).value_or(nullptr));
}

std::optional<SystemGuid> fishing_set_guid(void* fishing_set)
{
    if (!fishing_set)
        return std::nullopt;

    using GetGuidFn = SystemGuid* (*)(SystemGuid*, void*);
    il2cpp_runtime::Module module;
    const auto address = module.executable_address(
        action_offsets::RF4_Client_FishingScene_FishingSet_pchlpdepaai_0_method);
    auto fn = address ? reinterpret_cast<GetGuidFn>(*address) : nullptr;
    if (!fn)
        return std::nullopt;

    SystemGuid guid{};
    if (!get_fishing_set_guid_guarded(&guid, fn, fishing_set)) {
        log_line("fishing_set_guid: guarded call failed");
        return std::nullopt;
    }
    if (zero_guid(guid))
        return std::nullopt;
    return guid;
}

uintptr_t fish_from_guid(const SystemGuid& guid)
{
    using FishFromGuidFn = void* (*)(const SystemGuid*, void*, void*);
    il2cpp_runtime::Module module;
    const auto address = module.executable_address(k_synth_1402_pfokppcgekh_method);
    auto fn = address ? reinterpret_cast<FishFromGuidFn>(*address) : nullptr;
    if (!fn)
        return 0;

    uintptr_t result = 0;
    if (!fish_from_guid_guarded(fn, &guid, &result)) {
        log_line("fish_from_guid: guarded call failed");
        return 0;
    }
    return result;
}

std::string probe_summary(const ProbeSet& probe)
{
    std::ostringstream out;
    out << "FishingSet=" << hex_ptr(probe.fishing_set)
        << " Fisher=" << hex_ptr(probe.fisher)
        << " Rod=" << hex_ptr(probe.rod)
        << " Reel=" << hex_ptr(probe.reel)
        << " Lure=" << hex_ptr(probe.lure_complex)
        << " Fish=" << g_fish_instances.size();
    return out.str();
}

void scan_fish_instances(bool force = false)
{
    const auto now = std::chrono::steady_clock::now();
    if (!force &&
        g_next_fish_scan.time_since_epoch().count() != 0 &&
        now < g_next_fish_scan) {
        return;
    }

    g_next_fish_scan = now + std::chrono::seconds(2);
    il2cpp_runtime::ThreadAttach attach;
    if (!attach.attached()) {
        log_line("fish_scan: il2cpp thread attach failed");
        return;
    }

    g_fish_instances = game_actions::find_all_fish();
}

template <typename T>
std::optional<T> read_process_value(void* instance, uintptr_t offset)
{
    if (!instance)
        return std::nullopt;

    T value{};
    SIZE_T bytes_read = 0;
    const auto address = reinterpret_cast<const void*>(
        reinterpret_cast<uintptr_t>(instance) + offset);
    if (!ReadProcessMemory(GetCurrentProcess(), address, &value, sizeof(value), &bytes_read))
        return std::nullopt;
    if (bytes_read != sizeof(value))
        return std::nullopt;
    return value;
}

template <typename T>
std::optional<T> read_absolute_value(uintptr_t address)
{
    if (!address)
        return std::nullopt;

    T value{};
    SIZE_T bytes_read = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address),
                           &value, sizeof(value), &bytes_read))
        return std::nullopt;
    if (bytes_read != sizeof(value))
        return std::nullopt;
    return value;
}

template <typename T>
bool write_absolute_value(uintptr_t address, const T& value)
{
    if (!address)
        return false;

    SIZE_T bytes_written = 0;
    if (!WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address),
                            &value, sizeof(value), &bytes_written)) {
        return false;
    }
    return bytes_written == sizeof(value);
}

bool readable_memory(uintptr_t address, size_t size = 1)
{
    if (address < 0x10000 || size == 0)
        return false;

    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(reinterpret_cast<const void*>(address), &mbi, sizeof(mbi)) != sizeof(mbi))
        return false;

    if (mbi.State != MEM_COMMIT)
        return false;

    const DWORD protect = mbi.Protect & 0xFF;
    if (protect == PAGE_NOACCESS || protect == PAGE_EXECUTE || (mbi.Protect & PAGE_GUARD))
        return false;

    const uintptr_t region_start = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
    const uintptr_t region_end = region_start + mbi.RegionSize;
    return address >= region_start && address + size >= address && address + size <= region_end;
}

bool likely_pointer(uintptr_t value)
{
    if (value < 0x10000)
        return false;
    return readable_memory(value, sizeof(void*));
}

std::string lower_copy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool copy_c_string_guarded(const char* source, char* dest, size_t dest_size)
{
    if (!source || !dest || dest_size == 0)
        return false;

    __try {
        size_t i = 0;
        for (; i + 1 < dest_size; ++i) {
            const char ch = source[i];
            dest[i] = ch;
            if (ch == '\0')
                return true;
        }
        dest[i] = '\0';
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (dest_size)
            dest[0] = '\0';
        return false;
    }
}

struct Il2CppClassFunctions {
    void* (*object_get_class)(void*) = nullptr;
    const char* (*class_get_name)(void*) = nullptr;
    const char* (*class_get_namespace)(void*) = nullptr;
};

struct RawObjectClassInfo {
    void* klass = nullptr;
    const char* namespaze = nullptr;
    const char* name = nullptr;
};

bool get_raw_object_class_info_guarded(
    const Il2CppClassFunctions& fns,
    void* object,
    RawObjectClassInfo* info)
{
    if (!object || !info || !fns.object_get_class || !fns.class_get_name ||
        !fns.class_get_namespace) {
        return false;
    }

    __try {
        info->klass = fns.object_get_class(object);
        if (!info->klass)
            return false;
        info->name = fns.class_get_name(info->klass);
        info->namespaze = fns.class_get_namespace(info->klass);
        return info->name != nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        info->klass = nullptr;
        info->name = nullptr;
        info->namespaze = nullptr;
        return false;
    }
}

std::optional<Il2CppClassFunctions> il2cpp_class_functions()
{
    il2cpp_runtime::Module module;
    if (!module.valid())
        return std::nullopt;

    Il2CppClassFunctions fns{
        reinterpret_cast<void* (*)(void*)>(
            GetProcAddress(module.handle(), "il2cpp_object_get_class")),
        reinterpret_cast<const char* (*)(void*)>(
            GetProcAddress(module.handle(), "il2cpp_class_get_name")),
        reinterpret_cast<const char* (*)(void*)>(
            GetProcAddress(module.handle(), "il2cpp_class_get_namespace")),
    };

    if (!fns.object_get_class || !fns.class_get_name || !fns.class_get_namespace)
        return std::nullopt;
    return fns;
}

struct ObjectDescription {
    uintptr_t address = 0;
    uintptr_t klass = 0;
    std::string namespaze;
    std::string name;
    std::string full_name;
};

std::optional<ObjectDescription> describe_il2cpp_object(uintptr_t address)
{
    if (!likely_pointer(address))
        return std::nullopt;

    const auto fns = il2cpp_class_functions();
    if (!fns)
        return std::nullopt;

    RawObjectClassInfo raw{};
    if (!get_raw_object_class_info_guarded(*fns, reinterpret_cast<void*>(address), &raw))
        return std::nullopt;

    char name[160]{};
    char namespaze[240]{};
    if (!copy_c_string_guarded(raw.name, name, sizeof(name)) || name[0] == '\0')
        return std::nullopt;
    copy_c_string_guarded(raw.namespaze, namespaze, sizeof(namespaze));

    ObjectDescription desc{};
    desc.address = address;
    desc.klass = reinterpret_cast<uintptr_t>(raw.klass);
    desc.name = name;
    desc.namespaze = namespaze;
    desc.full_name = desc.namespaze.empty() ? desc.name : desc.namespaze + "." + desc.name;
    return desc;
}

bool description_contains(const ObjectDescription& desc, const char* needle)
{
    const std::string lower = lower_copy(desc.full_name);
    return lower.find(lower_copy(needle)) != std::string::npos;
}

void* find_il2cpp_class_by_scan(const char* namespaze, const char* class_name)
{
    il2cpp_runtime::Module module;
    if (!module.valid())
        return nullptr;

    auto domain_get = reinterpret_cast<void* (*)()>(
        GetProcAddress(module.handle(), "il2cpp_domain_get"));
    auto domain_get_assemblies = reinterpret_cast<const void** (*)(void*, size_t*)>(
        GetProcAddress(module.handle(), "il2cpp_domain_get_assemblies"));
    auto assembly_get_image = reinterpret_cast<const void* (*)(const void*)>(
        GetProcAddress(module.handle(), "il2cpp_assembly_get_image"));
    auto image_get_name = reinterpret_cast<const char* (*)(const void*)>(
        GetProcAddress(module.handle(), "il2cpp_image_get_name"));
    auto image_get_class_count = reinterpret_cast<size_t (*)(const void*)>(
        GetProcAddress(module.handle(), "il2cpp_image_get_class_count"));
    auto image_get_class = reinterpret_cast<void* (*)(const void*, size_t)>(
        GetProcAddress(module.handle(), "il2cpp_image_get_class"));
    auto class_get_name = reinterpret_cast<const char* (*)(void*)>(
        GetProcAddress(module.handle(), "il2cpp_class_get_name"));
    auto class_get_namespace = reinterpret_cast<const char* (*)(void*)>(
        GetProcAddress(module.handle(), "il2cpp_class_get_namespace"));

    if (!domain_get || !domain_get_assemblies || !assembly_get_image || !image_get_name ||
        !image_get_class_count || !image_get_class || !class_get_name || !class_get_namespace) {
        return nullptr;
    }

    void* domain = domain_get();
    if (!domain)
        return nullptr;

    size_t assembly_count = 0;
    const void** assemblies = domain_get_assemblies(domain, &assembly_count);
    if (!assemblies)
        return nullptr;

    const std::string expected_namespace = namespaze ? namespaze : "";
    const std::string expected_name = class_name ? class_name : "";
    for (size_t i = 0; i < assembly_count; ++i) {
        const void* image = assembly_get_image(assemblies[i]);
        const char* image_name = image ? image_get_name(image) : nullptr;
        if (!image || !image_name || std::string(image_name) != "Assembly-CSharp.dll")
            continue;

        const size_t class_count = image_get_class_count(image);
        for (size_t class_index = 0; class_index < class_count; ++class_index) {
            void* klass = image_get_class(image, class_index);
            if (!klass)
                continue;

            const char* raw_name = class_get_name(klass);
            const char* raw_namespace = class_get_namespace(klass);
            if (!raw_name || expected_name != raw_name)
                continue;

            const std::string actual_namespace = raw_namespace ? raw_namespace : "";
            if (expected_namespace.empty() || expected_namespace == actual_namespace)
                return klass;
        }
    }

    return nullptr;
}

using RuntimeMetadataInitFn = void (*)(uintptr_t*);
using RuntimeClassInitFn = void (*)(void*);
using InternalObjectNewFn = void* (*)(void*);
using InternalArrayNewSpecificFn = void* (*)(void*, uintptr_t);
using GcWriteBarrierSetFieldFn = void (*)(void*, void**, void*);
using IndexedStaticLookupFn = uintptr_t (*)(int, void*);

bool call_runtime_metadata_init_guarded(RuntimeMetadataInitFn fn, uintptr_t* slot)
{
    __try {
        fn(slot);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool call_runtime_class_init_guarded(RuntimeClassInitFn fn, void* klass)
{
    __try {
        fn(klass);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void* call_internal_object_new_guarded(InternalObjectNewFn fn, void* klass)
{
    __try {
        return fn(klass);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

void* call_internal_array_new_specific_guarded(
    InternalArrayNewSpecificFn fn,
    void* klass,
    uintptr_t length)
{
    __try {
        return fn(klass, length);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

bool call_gc_write_barrier_guarded(
    GcWriteBarrierSetFieldFn fn,
    void* object,
    void** target,
    void* value)
{
    __try {
        fn(object, target, value);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool call_indexed_static_lookup_guarded(
    IndexedStaticLookupFn fn,
    int key,
    void* klass,
    uintptr_t* result)
{
    if (result)
        *result = 0;

    __try {
        const uintptr_t value = fn(key, klass);
        if (result)
            *result = value;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

uintptr_t initialized_class_global(uintptr_t global_rva, const char* label)
{
    il2cpp_runtime::Module module;
    if (!module.valid())
        return 0;

    const uintptr_t slot_address = module.address(global_rva);
    const auto init_metadata_address =
        module.executable_address(k_codegen_init_runtime_metadata_method);
    auto init_metadata = init_metadata_address
        ? reinterpret_cast<RuntimeMetadataInitFn>(*init_metadata_address)
        : nullptr;
    if (!slot_address || !init_metadata) {
        std::ostringstream out;
        out << "metadata_init: missing function/slot label=" << label
            << " slot=" << hex_u64(slot_address);
        log_line(out.str());
        return 0;
    }

    if (!call_runtime_metadata_init_guarded(
            init_metadata,
            reinterpret_cast<uintptr_t*>(slot_address))) {
        std::ostringstream out;
        out << "metadata_init: exception label=" << label
            << " slot=" << hex_u64(slot_address);
        log_line(out.str());
        return 0;
    }

    const uintptr_t klass = read_absolute_value<uintptr_t>(slot_address).value_or(0);
    if (!klass) {
        std::ostringstream out;
        out << "metadata_init: empty class global label=" << label
            << " slot=" << hex_u64(slot_address);
        log_line(out.str());
    }
    return klass;
}

bool guarded_runtime_class_init(void* klass, const char* label)
{
    if (!klass)
        return false;

    il2cpp_runtime::Module module;
    const auto class_init_address = module.executable_address(k_runtime_class_init_method);
    auto class_init = class_init_address
        ? reinterpret_cast<RuntimeClassInitFn>(*class_init_address)
        : nullptr;
    if (!class_init)
        return false;

    const uintptr_t klass_address = reinterpret_cast<uintptr_t>(klass);
    const auto init_flag = read_absolute_value<std::uint32_t>(klass_address + 0xE0);
    if (init_flag && *init_flag != 0)
        return true;

    if (call_runtime_class_init_guarded(class_init, klass))
        return true;

    std::ostringstream out;
    out << "class_init: exception label=" << label
        << " class=" << hex_ptr(klass);
    log_line(out.str());
    return false;
}

void* allocate_internal_object_from_class_global(uintptr_t global_rva, const char* label)
{
    il2cpp_runtime::Module module;
    const auto object_new_address = module.executable_address(k_internal_object_new_method);
    auto object_new = object_new_address
        ? reinterpret_cast<InternalObjectNewFn>(*object_new_address)
        : nullptr;
    const uintptr_t klass = initialized_class_global(global_rva, label);
    if (!object_new || !klass) {
        std::ostringstream out;
        out << "internal_alloc: unavailable label=" << label
            << " object_new=" << hex_u64(reinterpret_cast<uintptr_t>(object_new))
            << " class=" << hex_u64(klass);
        log_line(out.str());
        return nullptr;
    }

    void* object = call_internal_object_new_guarded(
        object_new,
        reinterpret_cast<void*>(klass));
    if (!object) {
        std::ostringstream out;
        out << "internal_alloc: null/exception label=" << label
            << " class=" << hex_u64(klass);
        log_line(out.str());
    }
    return object;
}

void* create_empty_owner_array()
{
    il2cpp_runtime::Module module;
    const auto array_new_specific_address =
        module.executable_address(k_internal_array_new_specific_method);
    auto array_new_specific = array_new_specific_address
        ? reinterpret_cast<InternalArrayNewSpecificFn>(*array_new_specific_address)
        : nullptr;
    const uintptr_t klass = initialized_class_global(
        k_hlbkfjnodei_owner_array_class_global,
        "HlbkfjnodeiOwner[]");
    if (!array_new_specific || !klass)
        return nullptr;

    void* array = call_internal_array_new_specific_guarded(
        array_new_specific,
        reinterpret_cast<void*>(klass),
        0);
    if (!array)
        log_line("fabricate_meta: empty owner array allocation exception");
    return array;
}

bool write_managed_reference_field(void* object, uintptr_t offset, void* value)
{
    if (!object)
        return false;

    void** target = reinterpret_cast<void**>(
        reinterpret_cast<uintptr_t>(object) + offset);

    il2cpp_runtime::Module module;
    auto write_barrier = reinterpret_cast<GcWriteBarrierSetFieldFn>(
        GetProcAddress(module.handle(), "il2cpp_gc_wbarrier_set_field"));
    if (write_barrier) {
        if (call_gc_write_barrier_guarded(write_barrier, object, target, value))
            return true;
        log_line("managed_ref_write: write barrier exception");
        return false;
    }

    log_line("managed_ref_write: il2cpp_gc_wbarrier_set_field unavailable");
    return false;
}

float fish_size_scale()
{
    const uintptr_t klass = initialized_class_global(
        k_fish_size_params_class_global,
        "fish size params");
    if (!klass)
        return 1.0f;

    guarded_runtime_class_init(reinterpret_cast<void*>(klass), "fish size params");
    const uintptr_t static_fields =
        read_absolute_value<uintptr_t>(klass + 0xB8).value_or(0);
    const float scale =
        static_fields ? read_absolute_value<float>(static_fields + 0x14).value_or(1.0f) : 1.0f;
    if (!std::isfinite(scale) || scale <= 0.0f || scale > 1000.0f)
        return 1.0f;
    return scale;
}

uintptr_t indexed_static_lookup(int key, uintptr_t class_global_rva, const char* label)
{
    il2cpp_runtime::Module module;
    const auto lookup_address = module.executable_address(0x55E0);
    auto lookup = lookup_address
        ? reinterpret_cast<IndexedStaticLookupFn>(*lookup_address)
        : nullptr;
    const uintptr_t klass = initialized_class_global(class_global_rva, label);
    if (!lookup || !klass)
        return 0;

    uintptr_t value = 0;
    if (!call_indexed_static_lookup_guarded(
            lookup,
            key,
            reinterpret_cast<void*>(klass),
            &value)) {
        std::ostringstream out;
        out << "spawn_branch: lookup exception key=" << key
            << " label=" << label
            << " class=" << hex_u64(klass);
        log_line(out.str());
        return 0;
    }
    return value;
}

void log_spawn_branch_diagnostics(const char* reason)
{
    const uintptr_t synth_class = initialized_class_global(
        k_synth_1402_class_global,
        "Synth_1402");
    if (synth_class)
        guarded_runtime_class_init(reinterpret_cast<void*>(synth_class), "Synth_1402");

    const uintptr_t static_fields =
        synth_class ? read_absolute_value<uintptr_t>(synth_class + 0xB8).value_or(0) : 0;
    const uintptr_t owner_map =
        static_fields ? read_absolute_value<uintptr_t>(static_fields + 0x88).value_or(0) : 0;
    const std::uint32_t fish_counter =
        static_fields ? read_absolute_value<std::uint32_t>(static_fields + 0x90).value_or(0) : 0;
    const uintptr_t fish_event =
        static_fields ? read_absolute_value<uintptr_t>(static_fields + 0x10).value_or(0) : 0;
    const uintptr_t fish_created_event =
        static_fields ? read_absolute_value<uintptr_t>(static_fields + 0x98).value_or(0) : 0;
    const std::uint8_t local_spawn_enabled =
        static_fields ? read_absolute_value<std::uint8_t>(static_fields + 0xB8).value_or(0) : 0;

    const uintptr_t spawn_mode = indexed_static_lookup(
        12,
        k_fish_spawn_mode_class_global,
        "fish spawn mode");
    const uintptr_t position_source = indexed_static_lookup(
        41,
        k_fish_position_source_class_global,
        "fish position source");
    const uintptr_t position_bounds = indexed_static_lookup(
        6,
        k_fish_position_bounds_class_global,
        "fish position bounds");

    std::ostringstream out;
    out << "spawn_branch[" << reason << "]: "
        << "Synth1402Class=" << hex_u64(synth_class)
        << " static=" << hex_u64(static_fields)
        << " localEnabled=" << static_cast<unsigned int>(local_spawn_enabled)
        << " ownerMap=" << hex_u64(owner_map)
        << " counter=" << fish_counter
        << " fishEvent=" << hex_u64(fish_event)
        << " fishCreatedEvent=" << hex_u64(fish_created_event)
        << " mode12=" << hex_u64(spawn_mode)
        << " pos41=" << hex_u64(position_source)
        << " bounds6=" << hex_u64(position_bounds);
    log_line(out.str());
}

void* allocate_il2cpp_object(const char* namespaze, const char* class_name)
{
    il2cpp_runtime::Il2CppResolver resolver;
    if (!resolver.ready()) {
        std::ostringstream out;
        out << "allocate_il2cpp_object: resolver not ready class="
            << (class_name ? class_name : "");
        log_line(out.str());
        return nullptr;
    }

    void* klass = resolver.klass("Assembly-CSharp.dll", namespaze, class_name);
    if (!klass)
        klass = find_il2cpp_class_by_scan(namespaze, class_name);
    if (!klass) {
        std::ostringstream out;
        out << "allocate_il2cpp_object: class not found ns="
            << (namespaze ? namespaze : "")
            << " name=" << (class_name ? class_name : "");
        log_line(out.str());
        return nullptr;
    }

    il2cpp_runtime::Module module;
    auto object_new = reinterpret_cast<void* (*)(void*)>(
        GetProcAddress(module.handle(), "il2cpp_object_new"));
    if (!object_new) {
        log_line("allocate_il2cpp_object: il2cpp_object_new export missing");
        return nullptr;
    }

    void* object = nullptr;
    object = call_internal_object_new_guarded(object_new, klass);
    if (!object) {
        std::ostringstream out;
        out << "allocate_il2cpp_object: object_new returned null/exception class="
            << hex_ptr(klass);
        log_line(out.str());
        return nullptr;
    }
    return object;
}

void* new_il2cpp_string(const char* text)
{
    il2cpp_runtime::Module module;
    auto string_new = reinterpret_cast<void* (*)(const char*)>(
        GetProcAddress(module.handle(), "il2cpp_string_new"));
    return string_new ? string_new(text) : nullptr;
}

uintptr_t create_direct_debug_fish_bite_meta(int fish_family, void* fish_name, std::string* path)
{
    if (!fish_name)
        fish_name = new_il2cpp_string("debug_spawn");
    if (!fish_name) {
        log_line("fabricate_meta_direct: string allocation failed");
        return 0;
    }

    void* meta = allocate_internal_object_from_class_global(
        k_gkfghccolil_class_global,
        "gkfghccolil");
    if (!meta)
        return 0;

    void* empty_owner_array = create_empty_owner_array();
    if (!empty_owner_array) {
        log_line("fabricate_meta_direct: empty owner array allocation failed");
        return 0;
    }

    constexpr std::int32_t k_debug_fish_weight = 1000;
    constexpr float k_debug_bite_power = 1.0f;
    const float computed_size =
        static_cast<float>(k_debug_fish_weight) * 0.001f * fish_size_scale() * 2.0f;

    const uintptr_t meta_address = reinterpret_cast<uintptr_t>(meta);
    bool wrote = true;
    wrote &= write_managed_reference_field(meta, 0x20, fish_name);
    wrote &= write_absolute_value<std::int32_t>(meta_address + 0x28, fish_family);
    wrote &= write_absolute_value<float>(meta_address + 0x2C, k_debug_bite_power);
    wrote &= write_absolute_value<std::int32_t>(meta_address + 0x30, k_debug_fish_weight);
    wrote &= write_absolute_value<float>(meta_address + 0x34, 10000000.0f);
    wrote &= write_absolute_value<float>(meta_address + 0x38, 1.0f);
    wrote &= write_absolute_value<float>(meta_address + 0x40, computed_size);
    wrote &= write_absolute_value<float>(meta_address + 0x44, 10.0f);
    wrote &= write_absolute_value<float>(meta_address + 0x48, 7.0f);
    wrote &= write_absolute_value<float>(meta_address + 0x4C, 2.0f);
    wrote &= write_managed_reference_field(meta, 0x78, empty_owner_array);
    if (!wrote) {
        std::ostringstream out;
        out << "fabricate_meta_direct: field write failed meta=" << hex_ptr(meta);
        log_line(out.str());
        return 0;
    }

    const auto meta_desc = describe_il2cpp_object(reinterpret_cast<uintptr_t>(meta));
    if (!meta_desc || !description_contains(*meta_desc, "gkfghccolil")) {
        std::ostringstream out;
        out << "fabricate_meta_direct: invalid meta meta=" << hex_ptr(meta);
        if (meta_desc)
            out << " class=" << meta_desc->full_name;
        log_line(out.str());
        return 0;
    }

    if (path) {
        std::ostringstream out;
        out << "internal gkfghccolil(qword_183E4CDC8) family=" << fish_family
            << " weight=" << k_debug_fish_weight
            << " bite=" << k_debug_bite_power
            << " size=" << computed_size;
        *path = out.str();
    }

    std::ostringstream out;
    out << "fabricate_meta_direct: ok family=" << fish_family
        << " meta=" << hex_ptr(meta)
        << " class=" << meta_desc->full_name;
    log_line(out.str());
    return reinterpret_cast<uintptr_t>(meta);
}

using Synth5570StringCtorFn = void (*)(void*, void*, int, int, float, void*);

bool guarded_synth_5570_string_ctor(
    Synth5570StringCtorFn fn,
    void* instance,
    void* fish_name,
    int fish_family,
    int fish_weight,
    float bite_power)
{
    __try {
        fn(instance, fish_name, fish_family, fish_weight, bite_power, nullptr);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

uintptr_t create_debug_fish_bite_meta(int fish_family, std::string* path)
{
    void* model = allocate_il2cpp_object("", "Synth_5570_Closure");
    void* name = new_il2cpp_string("debug_spawn");
    if (!name) {
        std::ostringstream out;
        out << "fabricate_meta: allocation failed family=" << fish_family
            << " model=" << hex_ptr(model)
            << " name=" << hex_ptr(name);
        log_line(out.str());
        return 0;
    }
    if (!model) {
        std::ostringstream out;
        out << "fabricate_meta: model allocation failed, using direct meta family="
            << fish_family
            << " name=" << hex_ptr(name);
        log_line(out.str());
        return create_direct_debug_fish_bite_meta(fish_family, name, path);
    }

    il2cpp_runtime::Module module;
    const auto ctor_address = module.executable_address(k_synth_5570_string_ctor_method);
    auto ctor = ctor_address
        ? reinterpret_cast<Synth5570StringCtorFn>(*ctor_address)
        : nullptr;
    if (!ctor) {
        log_line("fabricate_meta: Synth_5570 string ctor address is null");
        return create_direct_debug_fish_bite_meta(fish_family, name, path);
    }

    constexpr int k_debug_fish_weight = 1000;
    constexpr float k_debug_bite_power = 1.0f;
    if (!guarded_synth_5570_string_ctor(
            ctor, model, name, fish_family, k_debug_fish_weight, k_debug_bite_power)) {
        std::ostringstream out;
        out << "fabricate_meta: ctor exception family=" << fish_family
            << " model=" << hex_ptr(model);
        log_line(out.str());
        return create_direct_debug_fish_bite_meta(fish_family, name, path);
    }

    const uintptr_t meta =
        read_absolute_value<uintptr_t>(
            reinterpret_cast<uintptr_t>(model) + k_synth_5570_fish_bite_meta_field)
            .value_or(0);
    const auto meta_desc = describe_il2cpp_object(meta);
    if (!meta_desc || !description_contains(*meta_desc, "gkfghccolil")) {
        std::ostringstream out;
        out << "fabricate_meta: invalid meta family=" << fish_family
            << " model=" << hex_ptr(model)
            << " meta=" << hex_u64(meta);
        if (meta_desc)
            out << " class=" << meta_desc->full_name;
        log_line(out.str());
        return create_direct_debug_fish_bite_meta(fish_family, name, path);
    }

    if (path) {
        std::ostringstream out;
        out << "Synth_5570_Closure.ctor(debug_spawn," << fish_family
            << ",1000,1.0)+0x30 " << meta_desc->full_name;
        *path = out.str();
    }
    return meta;
}

SensorState read_sensor_state(const ProbeSet& probe)
{
    SensorState state{};
    state.has_fishing_set = probe.fishing_set != nullptr;
    state.has_fisher = probe.fisher != nullptr;
    state.has_rod = probe.rod != nullptr;
    state.has_reel = probe.reel != nullptr;
    state.has_lure = probe.lure_complex != nullptr;

    state.fishing_set_150 = read_process_value<uint64_t>(probe.fishing_set, 0x150).value_or(0);
    state.fishing_set_158 = read_process_value<uint64_t>(probe.fishing_set, 0x158).value_or(0);
    state.fishing_set_160 = read_process_value<uint64_t>(probe.fishing_set, 0x160).value_or(0);
    state.rod_load = read_process_value<float>(probe.rod, 0x110).value_or(0.0f);
    state.reel_value = read_process_value<float>(probe.reel, 0xA8).value_or(0.0f);

    state.fisher_pos = read_process_value<Vector3>(probe.fisher, 0x70).value_or(Vector3{});
    state.fisher_alt_pos = read_process_value<Vector3>(probe.fisher, 0x164).value_or(Vector3{});
    state.rod_origin = read_process_value<Vector3>(probe.rod, 0x98).value_or(Vector3{});
    state.rod_mid = read_process_value<Vector3>(probe.rod, 0xA4).value_or(Vector3{});
    state.rod_tip = read_process_value<Vector3>(probe.rod, 0xC8).value_or(Vector3{});
    state.rod_velocity = read_process_value<Vector3>(probe.rod, 0xF4).value_or(Vector3{});
    state.lure_local = read_process_value<Vector3>(probe.lure_complex, 0x4C).value_or(Vector3{});
    state.lure_simple = read_process_value<uintptr_t>(probe.lure_complex, 0x38).value_or(0);
    state.has_lure_simple = state.lure_simple != 0;
    if (state.lure_simple) {
        const uintptr_t lure_state =
            read_absolute_value<uintptr_t>(
                state.lure_simple + k_lure_simple_state_field).value_or(0);
        std::optional<Vector3> child_lure_pos;
        if (lure_state) {
            child_lure_pos =
                read_absolute_value<Vector3>(lure_state + k_lure_state_world_position_field);
        }
        if (child_lure_pos && valid_vector(*child_lure_pos) && !near_zero_vector(*child_lure_pos)) {
            state.lure_pos = *child_lure_pos;
            state.lure_pos_from_child = true;
        } else {
            state.lure_pos =
                read_absolute_value<Vector3>(
                    state.lure_simple + k_lure_simple_legacy_position_field).value_or(Vector3{});
        }
        state.lure_velocity = read_absolute_value<Vector3>(state.lure_simple + 0xF4).value_or(Vector3{});
    }
    state.lure_estimated_pos = add_vectors(state.rod_mid, state.lure_local);
    if (valid_vector(state.lure_pos) && !near_zero_vector(state.lure_pos)) {
        state.best_lure_pos = state.lure_pos;
        state.has_best_lure_pos = true;
    } else if (valid_vector(state.lure_estimated_pos) && !near_zero_vector(state.lure_estimated_pos)) {
        state.best_lure_pos = state.lure_estimated_pos;
        state.has_best_lure_pos = true;
        state.best_lure_estimated = true;
    }
    if (valid_vector(state.fisher_pos) && state.has_best_lure_pos)
        state.fisher_to_lure = distance_between(state.fisher_pos, state.best_lure_pos);
    if (valid_vector(state.rod_mid) && state.has_best_lure_pos)
        state.rod_tip_to_lure = distance_between(state.rod_mid, state.best_lure_pos);
    if (valid_vector(state.rod_origin) && valid_vector(state.rod_tip))
        state.rod_span = distance_between(state.rod_origin, state.rod_tip);
    state.has_marked_spot = g_has_marked_spot;
    if (state.has_marked_spot && state.has_best_lure_pos)
        state.marked_spot_distance = distance_between(g_marked_spot, state.best_lure_pos);

    state.fishing_setup =
        read_process_value<uintptr_t>(probe.fishing_set, k_fishing_set_setup_field).value_or(0);
    if (state.fishing_setup) {
        state.fish_bite_meta =
            read_absolute_value<uintptr_t>(
                state.fishing_setup + k_synth_5570_fish_bite_meta_field).value_or(0);
    }

    state.fish_count = g_fish_instances.size();
    state.logical_fish_lure = read_process_value<uintptr_t>(probe.lure_complex, 0x58).value_or(0);
    state.logical_fish_set = fishing_set_active_fish(probe.fishing_set);
    if (const auto guid = fishing_set_guid(probe.fishing_set))
        state.logical_fish_guid = fish_from_guid(*guid);

    Vector3 fish_reference{};
    bool has_fish_reference = false;
    if (state.has_best_lure_pos) {
        fish_reference = state.best_lure_pos;
        has_fish_reference = true;
    } else if (valid_vector(state.fisher_pos) && !near_zero_vector(state.fisher_pos)) {
        fish_reference = state.fisher_pos;
        has_fish_reference = true;
    }

    float closest_distance = std::numeric_limits<float>::max();
    auto consider_fish = [&](void* fish, int source) {
        if (!fish)
            return;
        if (!object_is_fish(fish))
            return;

        const Vector3 primary =
            read_process_value<Vector3>(fish, 0xD8).value_or(Vector3{});
        const Vector3 alternate =
            read_process_value<Vector3>(fish, 0xCC).value_or(Vector3{});
        const bool primary_valid = valid_vector(primary) && !near_zero_vector(primary);
        const bool alternate_valid = valid_vector(alternate) && !near_zero_vector(alternate);
        if (!primary_valid && !alternate_valid)
            return;

        const Vector3 candidate = primary_valid ? primary : alternate;
        const float distance = has_fish_reference ?
            distance_between(candidate, fish_reference) :
            0.0f;

        if (!state.has_closest_fish || distance < closest_distance) {
            closest_distance = distance;
            state.has_closest_fish = true;
            state.closest_fish = reinterpret_cast<uintptr_t>(fish);
            state.closest_fish_source = source;
            state.closest_fish_pos = candidate;
            state.closest_fish_alt_pos = alternate;
        }
    };

    for (void* fish : g_fish_instances) {
        consider_fish(fish, 1);
    }
    consider_fish(reinterpret_cast<void*>(state.logical_fish_lure), 2);
    consider_fish(reinterpret_cast<void*>(state.logical_fish_set), 3);
    consider_fish(reinterpret_cast<void*>(state.logical_fish_guid), 4);

    if (state.has_closest_fish) {
        if (state.has_best_lure_pos)
            state.closest_fish_to_lure =
                distance_between(state.closest_fish_pos, state.best_lure_pos);
        if (valid_vector(state.fisher_pos) && !near_zero_vector(state.fisher_pos))
            state.closest_fish_to_fisher =
                distance_between(state.closest_fish_pos, state.fisher_pos);
    }

    state.reel_state = read_process_value<uintptr_t>(probe.reel, 0x150).value_or(0);
    if (state.reel_state) {
        state.reel_state_flags =
            read_absolute_value<uint8_t>(state.reel_state + 0x1C).value_or(0);
        state.reel_state_input =
            read_absolute_value<uintptr_t>(state.reel_state + 0x20).value_or(0);
        if (state.reel_state_input) {
            state.reel_input_20 =
                read_absolute_value<uint8_t>(state.reel_state_input + 0x20).value_or(0);
            state.reel_input_21 =
                read_absolute_value<uint8_t>(state.reel_state_input + 0x21).value_or(0);
        }
        state.reel_state_model =
            read_absolute_value<uintptr_t>(state.reel_state + 0x28).value_or(0);
        if (state.reel_state_model) {
            state.reel_model_d0 =
                read_absolute_value<uint8_t>(state.reel_state_model + 0xD0).value_or(0);
            state.reel_model_d1 =
                read_absolute_value<uint8_t>(state.reel_state_model + 0xD1).value_or(0);
        }
    }

    return state;
}

const char* lure_position_source(const SensorState& state)
{
    if (!state.has_best_lure_pos)
        return "none";
    if (state.lure_pos_from_child)
        return "raw_lure_simple_0x30_0xC0";
    return state.best_lure_estimated ? "estimated_rod_mid_lure_local" : "raw_lure_simple";
}

const char* closest_fish_source_name(int source)
{
    switch (source) {
    case 1:
        return "tracked_instance";
    case 2:
        return "logical_lure";
    case 3:
        return "fishing_set";
    case 4:
        return "fishing_set_guid";
    default:
        return "none";
    }
}

const char* coordinate_quality(const SensorState& state)
{
    if (!state.has_fishing_set || !state.has_fisher || !state.has_rod || !state.has_reel)
        return "missing_core_probe";
    if (!state.has_lure)
        return "missing_lure_complex";
    if (!state.has_best_lure_pos)
        return "missing_lure_position";
    if (state.best_lure_estimated)
        return "estimated_lure_position";
    if (state.fish_count > 0 && !state.has_closest_fish)
        return "fish_position_unresolved";
    return "ok";
}

float reference_distance(const Vector3& value, const Vector3& reference)
{
    if (!valid_vector(reference) || near_zero_vector(reference))
        return std::numeric_limits<float>::max();
    return distance_between(value, reference);
}

void collect_lure_vector_candidates(
    std::vector<LureVectorCandidate>& candidates,
    const std::string& path,
    uintptr_t base,
    const SensorState& state,
    uintptr_t max_offset = 0x240)
{
    if (!base)
        return;

    for (uintptr_t offset = 0x10; offset <= max_offset; offset += sizeof(float)) {
        const auto value = read_absolute_value<Vector3>(base + offset);
        if (!value || !valid_vector(*value) || near_zero_vector(*value))
            continue;

        candidates.push_back(LureVectorCandidate{
            path,
            offset,
            *value,
            reference_distance(*value, state.lure_estimated_pos),
            reference_distance(*value, state.fisher_pos),
            reference_distance(*value, state.rod_mid),
        });
    }
}

std::string append_offset_path(const std::string& root, uintptr_t offset)
{
    std::ostringstream out;
    out << root << "+0x" << std::uppercase << std::hex << offset << "->child";
    return out.str();
}

void collect_lure_child_vector_candidates(
    std::vector<LureVectorCandidate>& candidates,
    const std::string& root,
    uintptr_t base,
    const SensorState& state)
{
    if (!base)
        return;

    for (uintptr_t offset = 0x10; offset <= 0x240; offset += sizeof(uintptr_t)) {
        const uintptr_t child = read_absolute_value<uintptr_t>(base + offset).value_or(0);
        if (!likely_pointer(child))
            continue;

        collect_lure_vector_candidates(
            candidates,
            append_offset_path(root, offset),
            child,
            state,
            0x180);
    }
}

float lure_candidate_score(const LureVectorCandidate& candidate)
{
    if (candidate.dist_to_estimated != std::numeric_limits<float>::max())
        return candidate.dist_to_estimated;
    if (candidate.dist_to_fisher != std::numeric_limits<float>::max())
        return candidate.dist_to_fisher;
    return candidate.dist_to_rod;
}

void log_lure_vector_candidates(const char* reason, const ProbeSet& probe, const SensorState& state)
{
    std::vector<LureVectorCandidate> candidates;
    candidates.reserve(128);
    collect_lure_vector_candidates(
        candidates,
        "complex",
        reinterpret_cast<uintptr_t>(probe.lure_complex),
        state);
    collect_lure_child_vector_candidates(
        candidates,
        "complex",
        reinterpret_cast<uintptr_t>(probe.lure_complex),
        state);
    collect_lure_vector_candidates(candidates, "simple", state.lure_simple, state);
    collect_lure_child_vector_candidates(candidates, "simple", state.lure_simple, state);

    std::sort(candidates.begin(), candidates.end(), [](const auto& left, const auto& right) {
        return lure_candidate_score(left) < lure_candidate_score(right);
    });

    std::ostringstream out;
    out << "lure_vector_candidates[" << reason << "]: count=" << candidates.size();
    const size_t limit = std::min<size_t>(candidates.size(), 8);
    for (size_t i = 0; i < limit; ++i) {
        const auto& candidate = candidates[i];
        out << " " << candidate.path
            << "+0x" << std::uppercase << std::hex << candidate.offset << std::dec
            << "=" << format_vec(candidate.value)
            << " dEst=" << std::fixed << std::setprecision(2) << candidate.dist_to_estimated
            << " dF=" << candidate.dist_to_fisher
            << " dR=" << candidate.dist_to_rod;
    }
    log_line(out.str());
}

void log_snapshot_quality(const char* reason, const ProbeSet& probe, const SensorState& state)
{
    std::ostringstream out;
    out << "snapshot_quality[" << reason << "]:"
        << " quality=" << coordinate_quality(state)
        << " roots=fs:" << (state.has_fishing_set ? 1 : 0)
        << "/fisher:" << (state.has_fisher ? 1 : 0)
        << "/rod:" << (state.has_rod ? 1 : 0)
        << "/reel:" << (state.has_reel ? 1 : 0)
        << "/lure:" << (state.has_lure ? 1 : 0)
        << " lure_complex=" << hex_ptr(probe.lure_complex)
        << " lure_simple=" << hex_u64(state.lure_simple)
        << " lure_source=" << lure_position_source(state)
        << " raw_lure=" << format_vec(state.lure_pos)
        << " est_lure=" << format_vec(state.lure_estimated_pos)
        << " best_lure=" << format_vec(state.best_lure_pos)
        << " fish_count=" << state.fish_count
        << " fish_source=" << closest_fish_source_name(state.closest_fish_source)
        << " closest_fish=" << hex_u64(state.closest_fish)
        << " fish_pos=" << format_vec(state.closest_fish_pos)
        << " dist_fisher_lure=" << std::fixed << std::setprecision(2) << state.fisher_to_lure
        << " dist_rod_lure=" << std::fixed << std::setprecision(2) << state.rod_tip_to_lure;
    log_line(out.str());
    log_lure_vector_candidates(reason, probe, state);
}

std::string sensor_summary(const SensorState& state)
{
    std::ostringstream out;
    out << "FS150=" << hex_u64(state.fishing_set_150)
        << " FS158=" << hex_u64(state.fishing_set_158)
        << " FS160=" << hex_u64(state.fishing_set_160)
        << " rodLoad=" << std::fixed << std::setprecision(3) << state.rod_load
        << " reel=" << std::fixed << std::setprecision(3) << state.reel_value
        << " lure=" << format_vec(state.best_lure_pos)
        << (state.best_lure_estimated ? "(est)" : "")
        << " lureSrc=" << lure_position_source(state)
        << " q=" << coordinate_quality(state)
        << " rodWorld=" << format_vec(state.rod_mid)
        << " dist(F/L)=" << std::fixed << std::setprecision(1) << state.fisher_to_lure
        << " dist(R/L)=" << std::fixed << std::setprecision(1) << state.rod_tip_to_lure;
    if (state.has_marked_spot) {
        out << " spot=" << format_vec(g_marked_spot)
            << " spotDist=" << std::fixed << std::setprecision(1) << state.marked_spot_distance;
    }
    out << " fish=" << state.fish_count;
    if (state.fishing_setup || state.fish_bite_meta) {
        out << " setup=" << hex_u64(state.fishing_setup)
            << " biteMeta=" << hex_u64(state.fish_bite_meta);
    }
    if (state.logical_fish_lure || state.logical_fish_set || state.logical_fish_guid) {
        out << " logicalFish="
            << hex_u64(state.logical_fish_lure ? state.logical_fish_lure :
                       state.logical_fish_set ? state.logical_fish_set :
                       state.logical_fish_guid);
    }
    if (state.has_closest_fish) {
        out << " fishPos=" << format_vec(state.closest_fish_pos)
            << " fishSrc=" << state.closest_fish_source
            << " fishL=" << std::fixed << std::setprecision(1) << state.closest_fish_to_lure
            << " fishF=" << std::fixed << std::setprecision(1) << state.closest_fish_to_fisher;
    }
    out
        << " rFlags=" << state.reel_state_flags
        << " in=" << state.reel_input_20 << '/' << state.reel_input_21
        << " model=" << state.reel_model_d0 << '/' << state.reel_model_d1;
    return out.str();
}

SensorState refresh_sensor_status()
{
    if (!g_probe.fishing_set || !g_probe.rod || !g_probe.reel)
        capture_probe();
    scan_fish_instances();

    const SensorState state = read_sensor_state(g_probe);
    const std::string summary = sensor_summary(state);

    std::lock_guard<std::mutex> lock(g_mutex);
    g_status.sensor_summary = summary;
    g_status.probe_summary = probe_summary(g_probe);
    g_status.diagnostics_enabled = g_diagnostics_enabled;
    g_status.diagnostic_snapshots = g_diagnostic_snapshots;
    g_status.auto_reel_enabled = g_auto_reel_enabled;
    g_status.auto_reel_ticks = g_auto_reel_ticks;
    return state;
}

ProbeSet capture_probe()
{
    ProbeSet probe{
        game_actions::find_fishing_set(),
        game_actions::find_fisher(),
        game_actions::find_rod(),
        game_actions::find_reel(),
        game_actions::find_lure_complex(),
    };

    g_probe = probe;
    scan_fish_instances(true);

    std::lock_guard<std::mutex> lock(g_mutex);
    g_status.probe_summary = probe_summary(probe);
    g_status.diagnostics_enabled = g_diagnostics_enabled;
    g_status.diagnostic_snapshots = g_diagnostic_snapshots;
    g_status.auto_reel_enabled = g_auto_reel_enabled;
    g_status.auto_reel_ticks = g_auto_reel_ticks;
    return probe;
}

constexpr uintptr_t k_rod_offsets[] = {
    0x20, 0x28, 0x30, 0x38, 0x40, 0x48, 0x50, 0x58, 0x5C, 0x60,
    0x68, 0x70, 0x78, 0x80, 0x88, 0x90, 0x98, 0xA4, 0xA8, 0xAC,
    0xB0, 0xB4, 0xB8, 0xBC, 0xC0, 0xC4, 0xC8, 0xCC, 0xD0, 0xD8,
    0xE0, 0xE8, 0xF0, 0xF4, 0xF8, 0xFC, 0x100, 0x108, 0x110,
};

constexpr uintptr_t k_fishing_set_offsets[] = {
    0x30, 0x38, 0x40, 0x48, 0x50, 0x58, 0x60, 0x61, 0x64, 0x68,
    0x70, 0x74, 0x78, 0x84, 0x88, 0x89, 0x8A, 0x90, 0x98, 0xA4,
    0xB0, 0xBC, 0xC8, 0xD0, 0xD8, 0xE0, 0xE8, 0xF0, 0xF8, 0x100,
    0x108, 0x110, 0x118, 0x120, 0x128, 0x130, 0x138, 0x140, 0x148,
    0x150, 0x158, 0x160, 0x168, 0x170, 0x178, 0x180, 0x188, 0x190, 0x198, 0x1A0,
    0x1A8, 0x1B0, 0x1B8, 0x1C0, 0x1C8, 0x1D0,
};

constexpr uintptr_t k_fisher_offsets[] = {
    0x30, 0x38, 0x40, 0x48, 0x50, 0x58, 0x60, 0x68, 0x70, 0x80,
    0x88, 0x90, 0x91, 0x92, 0x93, 0x98, 0xA0, 0xA8, 0xB0, 0xC0,
    0xC8, 0xD0, 0xD8, 0xE0, 0xE8, 0xF0, 0xF8, 0x100, 0x108,
    0x110, 0x118, 0x120, 0x128, 0x130, 0x138, 0x140, 0x148, 0x150,
    0x158, 0x160, 0x164, 0x170, 0x180, 0x188, 0x190, 0x198, 0x1A0,
    0x1A8, 0x1AC, 0x1B0, 0x1B4,
};

constexpr uintptr_t k_lure_offsets[] = {
    0x20, 0x28, 0x30, 0x38, 0x40, 0x48, 0x50, 0x58, 0x60, 0x68,
    0x70, 0x78, 0x80, 0x88, 0x90, 0x98, 0xA0, 0xA8, 0xB0, 0xB8,
    0xC0, 0xC8, 0xD0, 0xD8, 0xE0, 0xE8, 0xF0, 0xF8, 0x100,
    0x108, 0x110, 0x118, 0x120, 0x128, 0x130, 0x138, 0x140,
    0x148, 0x150, 0x158, 0x160, 0x168, 0x170, 0x178, 0x180,
    0x188, 0x190, 0x198, 0x1A0, 0x1A8, 0x1B0, 0x1B8, 0x1C0,
};

constexpr uintptr_t k_reel_offsets[] = {
    0x30, 0x38, 0x40, 0x48, 0x50, 0x58, 0x60, 0x68, 0x70, 0x78,
    0x80, 0x88, 0x90, 0x98, 0xA0, 0xA8, 0xAC, 0xB0, 0xB8, 0xC0,
    0xC8, 0xD0, 0xD8, 0xE0, 0xE4, 0xE8, 0xF0, 0xF1, 0xF4, 0xF8,
    0xFC, 0xFD, 0xFE, 0xFF, 0x100, 0x104, 0x108, 0x10C, 0x110,
    0x114, 0x118, 0x11C, 0x120, 0x128, 0x130, 0x138, 0x140,
    0x148, 0x150, 0x158,
};

void write_field_header(std::ofstream& out, const char* prefix, const uintptr_t* offsets, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        out << ',' << prefix << "_0x" << std::uppercase << std::hex << offsets[i] << "_u64";
        out << ',' << prefix << "_0x" << std::uppercase << std::hex << offsets[i] << "_f32";
        out << std::dec;
    }
}

void write_field_values(std::ofstream& out, void* instance, const uintptr_t* offsets, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        const auto u64 = read_process_value<uint64_t>(instance, offsets[i]);
        const auto f32 = read_process_value<float>(instance, offsets[i]);

        out << ',';
        if (u64)
            out << hex_u64(*u64);

        out << ',';
        if (f32 && std::isfinite(*f32))
            out << std::fixed << std::setprecision(6) << *f32;
        out << std::defaultfloat;
    }
}

std::string format_field_path(const std::string& root, uintptr_t offset)
{
    std::ostringstream out;
    out << root << "+0x" << std::uppercase << std::hex << offset;
    return out.str();
}

void log_object_field_map(
    const char* reason,
    const std::string& label,
    uintptr_t object,
    uintptr_t max_offset = 0x220)
{
    const auto parent_desc = describe_il2cpp_object(object);
    if (!parent_desc)
        return;

    {
        std::ostringstream out;
        out << "object_map[" << reason << "]: " << label
            << "=" << hex_u64(object)
            << " class=" << parent_desc->full_name;
        log_line(out.str());
    }

    for (uintptr_t offset = 0x10; offset <= max_offset; offset += sizeof(uintptr_t)) {
        const uintptr_t child = read_absolute_value<uintptr_t>(object + offset).value_or(0);
        const auto child_desc = describe_il2cpp_object(child);
        if (!child_desc)
            continue;

        std::ostringstream out;
        out << "object_map[" << reason << "]: "
            << format_field_path(label, offset)
            << "=" << hex_u64(child)
            << " class=" << child_desc->full_name;
        log_line(out.str());
    }
}

struct LiveObjectNode {
    uintptr_t object = 0;
    std::string path;
    unsigned int depth = 0;
};

struct LiveObjectMatch {
    uintptr_t object = 0;
    std::string path;
    ObjectDescription description;
};

std::optional<LiveObjectMatch> find_object_by_class_terms(
    const std::vector<LiveObjectNode>& roots,
    const std::vector<std::string>& terms,
    unsigned int max_depth,
    size_t max_nodes)
{
    std::vector<LiveObjectNode> queue = roots;
    std::unordered_set<uintptr_t> visited;
    size_t index = 0;
    size_t described = 0;

    while (index < queue.size() && described < max_nodes) {
        const LiveObjectNode node = queue[index++];
        if (!likely_pointer(node.object) || visited.find(node.object) != visited.end())
            continue;
        visited.insert(node.object);

        const auto desc = describe_il2cpp_object(node.object);
        if (!desc)
            continue;
        ++described;

        const std::string full_lower = lower_copy(desc->full_name);
        for (const std::string& term : terms) {
            if (full_lower.find(lower_copy(term)) != std::string::npos) {
                return LiveObjectMatch{node.object, node.path, *desc};
            }
        }

        if (node.depth >= max_depth)
            continue;

        for (uintptr_t offset = 0x10; offset <= 0x220; offset += sizeof(uintptr_t)) {
            const uintptr_t child =
                read_absolute_value<uintptr_t>(node.object + offset).value_or(0);
            if (!likely_pointer(child) || visited.find(child) != visited.end())
                continue;

            queue.push_back(LiveObjectNode{
                child,
                format_field_path(node.path, offset),
                node.depth + 1,
            });
        }
    }

    return std::nullopt;
}

std::vector<LiveObjectNode> live_object_roots(const ProbeSet& probe)
{
    std::vector<LiveObjectNode> roots;
    auto add_root = [&](const char* label, void* object) {
        if (object)
            roots.push_back(LiveObjectNode{reinterpret_cast<uintptr_t>(object), label, 0});
    };

    add_root("FishingSet", probe.fishing_set);
    add_root("Fisher", probe.fisher);
    add_root("Rod", probe.rod);
    add_root("Reel", probe.reel);
    add_root("LureComplex", probe.lure_complex);

    const uintptr_t rig_connector =
        read_process_value<uintptr_t>(probe.fishing_set, k_fishing_set_rig_connector_field)
            .value_or(0);
    if (rig_connector)
        roots.push_back(LiveObjectNode{rig_connector, "FishingSet+0x90", 0});

    const uintptr_t fishing_setup =
        read_process_value<uintptr_t>(probe.fishing_set, k_fishing_set_setup_field).value_or(0);
    if (fishing_setup)
        roots.push_back(LiveObjectNode{fishing_setup, "FishingSet+0x68", 0});

    return roots;
}

std::optional<LiveObjectMatch> live_fish_bite_meta_match(const ProbeSet& probe)
{
    const auto roots = live_object_roots(probe);

    if (const auto direct = find_object_by_class_terms(
            roots, {"gkfghccolil"}, 4, 900)) {
        return direct;
    }

    const auto owner = find_object_by_class_terms(
        roots,
        {"fishbitemeta", "prodidowner_fishingset"},
        4,
        900);
    if (!owner)
        return std::nullopt;

    const uintptr_t meta =
        read_absolute_value<uintptr_t>(owner->object + k_fish_bite_meta_owner_value_field)
            .value_or(0);
    const auto meta_desc = describe_il2cpp_object(meta);
    if (meta_desc && description_contains(*meta_desc, "gkfghccolil")) {
        return LiveObjectMatch{
            meta,
            format_field_path(owner->path, k_fish_bite_meta_owner_value_field),
            *meta_desc,
        };
    }

    return std::nullopt;
}

uintptr_t find_live_fish_bite_meta(const ProbeSet& probe, std::string* path)
{
    if (const auto match = live_fish_bite_meta_match(probe)) {
        if (path) {
            *path = match->path + " " + match->description.full_name;
        }
        return match->object;
    }

    return 0;
}

void log_active_fish_object_map(const char* reason, const ProbeSet& probe)
{
    const uintptr_t rig_connector =
        read_process_value<uintptr_t>(probe.fishing_set, k_fishing_set_rig_connector_field)
            .value_or(0);
    const uintptr_t set_setup =
        read_process_value<uintptr_t>(probe.fishing_set, k_fishing_set_setup_field).value_or(0);
    const uintptr_t set_meta = set_setup ?
        read_absolute_value<uintptr_t>(set_setup + k_synth_5570_fish_bite_meta_field).value_or(0) :
        0;
    const uintptr_t rig_fish = rig_connector ?
        read_absolute_value<uintptr_t>(rig_connector + 0x60).value_or(0) :
        0;
    const uintptr_t lure_fish =
        read_process_value<uintptr_t>(probe.lure_complex, 0x58).value_or(0);

    std::ostringstream out;
    out << "fish_path[" << reason << "]: "
        << "FishingSet=" << hex_ptr(probe.fishing_set)
        << " setup=" << hex_u64(set_setup)
        << " setupMeta=" << hex_u64(set_meta)
        << " RigConnector=" << hex_u64(rig_connector)
        << " RigConnector+0x60=" << hex_u64(rig_fish)
        << " LureComplex+0x58=" << hex_u64(lure_fish)
        << " (deep graph scan disabled)";
    log_line(out.str());
}

void write_vec_values(std::ofstream& out, const Vector3& value)
{
    out << ',' << std::fixed << std::setprecision(6) << value.x
        << ',' << value.y
        << ',' << value.z;
    out << std::defaultfloat;
}

void write_coordinate_header(std::ofstream& out)
{
    out << "tick_ms,reason,fisher_x,fisher_y,fisher_z,"
           "fisher_alt_x,fisher_alt_y,fisher_alt_z,"
           "rod_origin_x,rod_origin_y,rod_origin_z,"
           "rod_mid_x,rod_mid_y,rod_mid_z,"
           "rod_tip_x,rod_tip_y,rod_tip_z,"
           "rod_velocity_x,rod_velocity_y,rod_velocity_z,"
           "lure_local_x,lure_local_y,lure_local_z,"
           "lure_raw_x,lure_raw_y,lure_raw_z,"
           "lure_est_x,lure_est_y,lure_est_z,"
           "best_lure_x,best_lure_y,best_lure_z,best_lure_estimated,"
           "lure_velocity_x,lure_velocity_y,lure_velocity_z,"
           "fisher_to_lure,rod_tip_to_lure,rod_span,"
           "marked,marked_x,marked_y,marked_z,marked_distance,"
           "fish_count,fishing_setup,fish_bite_meta,"
           "logical_fish_lure,logical_fish_set,logical_fish_guid,"
           "closest_fish,closest_fish_source,fish_x,fish_y,fish_z,"
           "fish_alt_x,fish_alt_y,fish_alt_z,fish_to_lure,fish_to_fisher,"
           "fishing_set_0x150,fishing_set_0x158,fishing_set_0x160,"
           "rod_load,reel_value,reel_flags,"
           "coordinate_quality,lure_position_source,fish_position_source,"
           "has_fishing_set,has_fisher,has_rod,has_reel,has_lure,"
           "has_lure_simple,has_best_lure_pos,has_closest_fish\n";
}

bool log_coordinate_snapshot(const char* reason, const SensorState& state)
{
    const std::filesystem::path path = coordinate_path();

    std::error_code ec;
    const bool needs_header = !std::filesystem::exists(path, ec) ||
        std::filesystem::file_size(path, ec) == 0;

    std::ofstream out(path, std::ios::out | std::ios::app);
    if (!out)
        return false;

    if (needs_header)
        write_coordinate_header(out);

    out << GetTickCount64() << ',' << reason;
    write_vec_values(out, state.fisher_pos);
    write_vec_values(out, state.fisher_alt_pos);
    write_vec_values(out, state.rod_origin);
    write_vec_values(out, state.rod_mid);
    write_vec_values(out, state.rod_tip);
    write_vec_values(out, state.rod_velocity);
    write_vec_values(out, state.lure_local);
    write_vec_values(out, state.lure_pos);
    write_vec_values(out, state.lure_estimated_pos);
    write_vec_values(out, state.best_lure_pos);
    out << ',' << (state.best_lure_estimated ? 1 : 0);
    write_vec_values(out, state.lure_velocity);
    out << ',' << std::fixed << std::setprecision(6) << state.fisher_to_lure
        << ',' << state.rod_tip_to_lure
        << ',' << state.rod_span
        << ',' << (state.has_marked_spot ? 1 : 0);
    write_vec_values(out, g_marked_spot);
    out << ',' << state.marked_spot_distance
        << ',' << state.fish_count
        << ',' << hex_u64(state.fishing_setup)
        << ',' << hex_u64(state.fish_bite_meta)
        << ',' << hex_u64(state.logical_fish_lure)
        << ',' << hex_u64(state.logical_fish_set)
        << ',' << hex_u64(state.logical_fish_guid)
        << ',' << hex_u64(state.closest_fish)
        << ',' << state.closest_fish_source;
    write_vec_values(out, state.closest_fish_pos);
    write_vec_values(out, state.closest_fish_alt_pos);
    out << ',' << state.closest_fish_to_lure
        << ',' << state.closest_fish_to_fisher
        << ',' << hex_u64(state.fishing_set_150)
        << ',' << hex_u64(state.fishing_set_158)
        << ',' << hex_u64(state.fishing_set_160)
        << ',' << state.rod_load
        << ',' << state.reel_value
        << ',' << state.reel_state_flags
        << ',' << coordinate_quality(state)
        << ',' << lure_position_source(state)
        << ',' << closest_fish_source_name(state.closest_fish_source)
        << ',' << (state.has_fishing_set ? 1 : 0)
        << ',' << (state.has_fisher ? 1 : 0)
        << ',' << (state.has_rod ? 1 : 0)
        << ',' << (state.has_reel ? 1 : 0)
        << ',' << (state.has_lure ? 1 : 0)
        << ',' << (state.has_lure_simple ? 1 : 0)
        << ',' << (state.has_best_lure_pos ? 1 : 0)
        << ',' << (state.has_closest_fish ? 1 : 0)
        << '\n';

    return true;
}

void write_diagnostic_header(std::ofstream& out)
{
    out << "tick_ms,reason,fishing_set,fisher,rod,reel,lure_complex";
    write_field_header(out, "fishing_set", k_fishing_set_offsets, std::size(k_fishing_set_offsets));
    write_field_header(out, "fisher", k_fisher_offsets, std::size(k_fisher_offsets));
    write_field_header(out, "lure", k_lure_offsets, std::size(k_lure_offsets));
    write_field_header(out, "rod", k_rod_offsets, std::size(k_rod_offsets));
    write_field_header(out, "reel", k_reel_offsets, std::size(k_reel_offsets));
    out << ",reel_0x150_ptr,reel_0x150_0x1c_u8,reel_0x150_0x20_ptr,"
           "reel_0x150_0x20_0x20_u8,reel_0x150_0x20_0x21_u8,"
           "reel_0x150_0x28_ptr,reel_0x150_0x28_0xd0_u8,"
           "reel_0x150_0x28_0xd1_u8\n";
}

bool log_diagnostic_snapshot(const char* reason)
{
    if (!g_probe.fishing_set || !g_probe.fisher || !g_probe.rod || !g_probe.reel)
        capture_probe();
    scan_fish_instances(true);

    const ProbeSet probe = g_probe;
    const SensorState sensor = read_sensor_state(probe);
    const std::filesystem::path path = diagnostic_path();

    std::error_code ec;
    const bool needs_header = !std::filesystem::exists(path, ec) ||
        std::filesystem::file_size(path, ec) == 0;

    std::ofstream out(path, std::ios::out | std::ios::app);
    if (!out)
        return false;

    if (needs_header)
        write_diagnostic_header(out);

    out << GetTickCount64()
        << ',' << reason
        << ',' << hex_ptr(probe.fishing_set)
        << ',' << hex_ptr(probe.fisher)
        << ',' << hex_ptr(probe.rod)
        << ',' << hex_ptr(probe.reel)
        << ',' << hex_ptr(probe.lure_complex);

    write_field_values(out, probe.fishing_set, k_fishing_set_offsets, std::size(k_fishing_set_offsets));
    write_field_values(out, probe.fisher, k_fisher_offsets, std::size(k_fisher_offsets));
    write_field_values(out, probe.lure_complex, k_lure_offsets, std::size(k_lure_offsets));
    write_field_values(out, probe.rod, k_rod_offsets, std::size(k_rod_offsets));
    write_field_values(out, probe.reel, k_reel_offsets, std::size(k_reel_offsets));

    out << ',' << hex_u64(sensor.reel_state)
        << ',' << sensor.reel_state_flags
        << ',' << hex_u64(sensor.reel_state_input)
        << ',' << sensor.reel_input_20
        << ',' << sensor.reel_input_21
        << ',' << hex_u64(sensor.reel_state_model)
        << ',' << sensor.reel_model_d0
        << ',' << sensor.reel_model_d1;
    out << '\n';

    const bool coordinate_ok = log_coordinate_snapshot(reason, sensor);
    if (std::string(reason) != "periodic") {
        log_snapshot_quality(reason, probe, sensor);
        log_active_fish_object_map(reason, probe);
    }

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        ++g_diagnostic_snapshots;
        g_status.diagnostic_snapshots = g_diagnostic_snapshots;
        g_status.diagnostics_enabled = g_diagnostics_enabled;
        g_status.auto_reel_enabled = g_auto_reel_enabled;
        g_status.auto_reel_ticks = g_auto_reel_ticks;
        g_status.probe_summary = probe_summary(probe);
        g_status.sensor_summary = sensor_summary(sensor);
    }

    return coordinate_ok;
}

ActionSet discover_actions()
{
    il2cpp_runtime::ThreadAttach attach;
    if (!attach.attached()) {
        log_line("action discovery: il2cpp thread attach failed");
        return {};
    }

    void* input = game_actions::find_fishing_scene_input_controller();
    void* set = game_actions::find_fishing_set();
    void* fisher = game_actions::find_fisher();
    void* rod = game_actions::find_rod();
    void* reel = game_actions::find_reel();
    void* lure = game_actions::find_lure_complex();
    void* input_actions = game_actions::input_system_actions();

    g_probe = ProbeSet{set, fisher, rod, reel, lure};
    scan_fish_instances(true);

    ActionSet actions{
        game_actions::fishing_toggle_reel_action(),
        game_actions::fishing_start_hooking_action(),
        game_actions::fishing_alternative_action(),
        game_actions::fishing_toggle_podsak_action(),
        game_actions::fishing_set_toggle_reel_action(),
        game_actions::fishing_set_cut_fishing_line_action(),
        game_actions::fishing_set_change_reel_clip_position_action(),
        game_actions::fishing_rig_change_reel_clip_position_action(),
        game_actions::fishing_rig_hot_swap_bait1_action(),
        game_actions::fishing_rig_hot_swap_bait2_action(),
        game_actions::fishing_rig_change_bobber_depth_action(),
        game_actions::interactions_rod_to_rodrest_action(),
        game_actions::interactions_rod_slot_action(),
        game_actions::hand_item_hot_swap_action(),
        game_actions::fishing_set_switch_throw_mode_action(),
        game_actions::fishing_set_hitch_action(),
        game_actions::fishing_set_return_to_idle_action(),
        game_actions::hand_item_change_throw_distance_action(),
        game_actions::fishing_reel_manual_roll_action(),
        game_actions::fishing_reel_manual_roll_boost_action(),
        game_actions::fishing_reel_switch_speed_action(),
        game_actions::fishing_reel_change_transmission_mode_action(),
        game_actions::fishing_reel_toggle_auto_roll_mode_action(),
        game_actions::fishing_reel_reset_auto_roll_mode_action(),
        game_actions::fishing_reel_change_friction_action(),
        game_actions::fishing_reel_roll_speed_mode_action(),
        game_actions::fishing_reel_change_roll_speed_action(),
        game_actions::fishing_reel_toggle_engine_action(),
        game_actions::fishing_reel_toggle_transmission_action(),
        game_actions::debug_return_to_idle_action(),
        game_actions::debug_catch_fish_action(),
        game_actions::debug_repair_rod_action(),
        game_actions::debug_spawn_fish_action(),
        game_actions::debug_fish_jump_action(),
        game_actions::debug_level_up_action(),
        game_actions::debug_hitch_action(),
    };

    log_line("action discovery");
    log_ptr("FishingSceneInputController", input);
    log_ptr("FishingSet", set);
    log_ptr("Fisher", fisher);
    log_ptr("Rod", rod);
    log_ptr("Reel", reel);
    log_ptr("LureComplex", lure);
    log_ptr("InputSystemActions", input_actions);
    log_ptr("Fishing.ToggleReel", actions.toggle_reel);
    log_ptr("Fishing.StartHooking", actions.start_hooking);
    log_ptr("Fishing.AlternativeAction", actions.alternative_action);
    log_ptr("Fishing.TogglePodsak", actions.toggle_podsak);
    log_ptr("FishingSet.ToggleReel", actions.fishing_set_toggle_reel);
    log_ptr("FishingSet.CutFishingLine", actions.cut_fishing_line);
    log_ptr("FishingSet.ChangeReelClipPosition", actions.fishing_set_clip);
    log_ptr("FishingRig.ChangeReelClipPosition", actions.rig_clip);
    log_ptr("FishingRig.HotSwapBait1", actions.hot_swap_bait1);
    log_ptr("FishingRig.HotSwapBait2", actions.hot_swap_bait2);
    log_ptr("FishingRig.ChangeBobberDepth", actions.change_bobber_depth);
    log_ptr("Interactions.RodToRodrest", actions.rod_to_rodrest);
    log_ptr("Interactions.RodSlot", actions.rod_slot);
    log_ptr("HandItem.HotSwap", actions.hand_item_hot_swap);
    log_ptr("FishingSet.SwitchThrowMode", actions.switch_throw_mode);
    log_ptr("FishingSet.Hitch", actions.hitch);
    log_ptr("FishingSet.ReturnToIdle", actions.return_to_idle);
    log_ptr("HandItem.ChangeThrowDistance", actions.change_throw_distance);
    log_ptr("FishingReel.ManualRoll", actions.manual_roll);
    log_ptr("FishingReel.ManualRollBoost", actions.manual_roll_boost);
    log_ptr("FishingReel.SwitchSpeed", actions.switch_reel_speed);
    log_ptr("FishingReel.ChangeTransmissionMode", actions.change_transmission_mode);
    log_ptr("FishingReel.ToggleAutoRollMode", actions.toggle_auto_roll_mode);
    log_ptr("FishingReel.ResetAutoRollMode", actions.reset_auto_roll_mode);
    log_ptr("FishingReel.ChangeFriction", actions.change_friction);
    log_ptr("FishingReel.RollSpeedMode", actions.roll_speed_mode);
    log_ptr("FishingReel.ChangeRollSpeed", actions.change_roll_speed);
    log_ptr("FishingReel.ToggleEngine", actions.toggle_engine);
    log_ptr("FishingReel.ToggleTransmission", actions.toggle_transmission);
    log_ptr("Debug.ReturnToIdle", actions.debug_return_to_idle);
    log_ptr("Debug.CatchFish", actions.debug_catch_fish);
    log_ptr("Debug.RepairRod", actions.debug_repair_rod);
    log_ptr("Debug.SpawnFish", actions.debug_spawn_fish);
    log_ptr("Debug.FishJump", actions.debug_fish_jump);
    log_ptr("Debug.LevelUp", actions.debug_level_up);
    log_ptr("Debug.Hitch", actions.debug_hitch);

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_status.probe_summary = probe_summary(g_probe);
    }

    game_actions::capture_common_instances();
    enable_actions(actions);
    return actions;
}

void update_ready_status(bool ready, const std::string& message)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_status.ready = ready;
    g_status.message = message;
    g_status.running = g_running.load();
    g_status.diagnostics_enabled = g_diagnostics_enabled;
    g_status.diagnostic_snapshots = g_diagnostic_snapshots;
    g_status.auto_reel_enabled = g_auto_reel_enabled;
    g_status.auto_reel_ticks = g_auto_reel_ticks;
}

bool take_command(fc::actions::Command& command)
{
    std::unique_lock<std::mutex> lock(g_mutex);
    g_cv.wait_for(lock, std::chrono::milliseconds(250), [] {
        return !g_queue.empty() || !g_running.load();
    });

    if (!g_running.load() || g_queue.empty())
        return false;

    command = g_queue.front();
    g_queue.pop_front();
    g_status.queued = static_cast<unsigned int>(g_queue.size());
    return true;
}

bool perform_command(fc::actions::Command command, const ActionSet& actions)
{
    void* action = action_for_command(command, actions);
    if (!action)
        return false;

    return game_actions::pulse_input_action(action);
}

bool hold_input_action(void* action, DWORD hold_ms)
{
    if (!action)
        return false;

    const bool pressed = game_actions::set_trigger_button_state(action, true);
    const bool started = game_actions::change_input_action_phase(
        action,
        game_actions::input_internal::InputActionPhase::Started);
    const bool performed = game_actions::change_input_action_phase(
        action,
        game_actions::input_internal::InputActionPhase::Performed);

    if (pressed || started || performed)
        sleep_interruptible(hold_ms);

    const bool released = game_actions::set_trigger_button_state(action, false);
    const bool canceled = game_actions::change_input_action_phase(
        action,
        game_actions::input_internal::InputActionPhase::Canceled);

    return pressed && performed && released && canceled;
}

bool send_key_state(WORD vk, bool pressed)
{
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk;
    input.ki.dwFlags = pressed ? 0 : KEYEVENTF_KEYUP;
    return SendInput(1, &input, sizeof(INPUT)) == 1;
}

bool tap_key(WORD vk, DWORD hold_ms = 55)
{
    if (!send_key_state(vk, true))
        return false;

    sleep_interruptible(hold_ms);
    return send_key_state(vk, false);
}

bool tap_key_combo(WORD modifier, WORD key, DWORD hold_ms = 55)
{
    bool ok = send_key_state(modifier, true);
    sleep_interruptible(25);
    ok = tap_key(key, hold_ms) && ok;
    sleep_interruptible(25);
    ok = send_key_state(modifier, false) && ok;
    return ok;
}

bool send_mouse_event(DWORD flags)
{
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = flags;
    return SendInput(1, &input, sizeof(INPUT)) == 1;
}

bool send_left_mouse_hold(DWORD hold_ms, bool shift_boost = false)
{
    fc::Overlay::Get().SetMenuVisible(false);
    sleep_interruptible(250);

    bool ok = true;
    if (shift_boost) {
        ok = send_key_state(VK_SHIFT, true);
        sleep_interruptible(40);
    }

    if (!send_mouse_event(MOUSEEVENTF_LEFTDOWN)) {
        if (shift_boost)
            send_key_state(VK_SHIFT, false);
        return false;
    }

    sleep_interruptible(hold_ms);
    ok = send_mouse_event(MOUSEEVENTF_LEFTUP) && ok;

    if (shift_boost) {
        sleep_interruptible(40);
        ok = send_key_state(VK_SHIFT, false) && ok;
    }

    return ok;
}

bool send_fish_fight_hold(DWORD hold_ms)
{
    fc::Overlay::Get().SetMenuVisible(false);
    sleep_interruptible(250);

    bool ok = send_mouse_event(MOUSEEVENTF_RIGHTDOWN);
    sleep_interruptible(60);
    ok = send_mouse_event(MOUSEEVENTF_LEFTDOWN) && ok;

    sleep_interruptible(hold_ms);

    ok = send_mouse_event(MOUSEEVENTF_LEFTUP) && ok;
    sleep_interruptible(50);
    ok = send_mouse_event(MOUSEEVENTF_RIGHTUP) && ok;
    return ok;
}

bool prepare_reel_for_retrieve()
{
    fc::Overlay::Get().SetMenuVisible(false);
    sleep_interruptible(180);

    bool ok = tap_key(VK_RETURN);
    sleep_interruptible(140);

    for (int i = 0; i < 5; ++i) {
        ok = tap_key_combo('R', VK_ADD) && ok;
        sleep_interruptible(90);
    }

    return ok;
}

bool has_detected_fish(const SensorState& state)
{
    return state.has_closest_fish ||
        state.logical_fish_lure != 0 ||
        state.logical_fish_set != 0 ||
        state.logical_fish_guid != 0;
}

using SpawnActiveFishFn = uintptr_t (*)(SystemGuid*, void*);

bool guarded_active_fish_spawn_call(
    SpawnActiveFishFn fn,
    SystemGuid guid,
    void* fish_bite_meta,
    uintptr_t* result,
    DWORD* exception_code)
{
    if (result)
        *result = 0;
    if (exception_code)
        *exception_code = 0;

    __try {
        const uintptr_t call_result = fn(&guid, fish_bite_meta);
        if (result)
            *result = call_result;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (exception_code)
            *exception_code = 1;
        return false;
    }
}

bool call_spawn_rva_path(
    const char* label,
    uintptr_t rva,
    const SystemGuid& guid,
    uintptr_t fish_bite_meta)
{
    il2cpp_runtime::Module module;
    const auto address = module.executable_address(rva);
    auto fn = address ? reinterpret_cast<SpawnActiveFishFn>(*address) : nullptr;
    if (!fn || !fish_bite_meta)
        return false;

    DWORD exception_code = 0;
    uintptr_t result = 0;
    if (!guarded_active_fish_spawn_call(
            fn,
            guid,
            reinterpret_cast<void*>(fish_bite_meta),
            &result,
            &exception_code)) {
        std::ostringstream out;
        out << "spawn_fish direct: " << label << " exception 0x"
            << std::hex << std::uppercase << exception_code;
        log_line(out.str());
        return false;
    }

    std::ostringstream out;
    out << "spawn_fish direct: " << label
        << " returned " << hex_u64(result);
    log_line(out.str());
    return true;
}

bool call_active_fish_spawn_path(const SystemGuid& guid, uintptr_t fish_bite_meta)
{
    const bool created = call_spawn_rva_path(
        "Synth_1402.bjkkdngcmfm",
        k_synth_1402_bjkkdngcmfm_method,
        guid,
        fish_bite_meta);
    const bool scheduled = call_spawn_rva_path(
        "Synth_3787.jnbclacafdf",
        k_synth_3787_jnbclacafdf_method,
        guid,
        fish_bite_meta);
    return created || scheduled;
}

bool perform_active_fish_spawn()
{
    const SensorState before = refresh_sensor_status();
    if (!before.has_fishing_set) {
        log_line("spawn_fish direct: missing FishingSet");
        return false;
    }

    il2cpp_runtime::ThreadAttach attach;
    if (!attach.attached()) {
        log_line("spawn_fish direct: il2cpp thread attach failed");
        return false;
    }

    const auto guid = fishing_set_guid(g_probe.fishing_set);
    if (!guid) {
        log_line("spawn_fish direct: FishingSet guid is empty");
        return false;
    }

    uintptr_t fish_bite_meta = before.fish_bite_meta;
    std::string meta_path;
    if (fish_bite_meta)
        meta_path = "FishingSet+0x68+0x30 Synth_5570_Closure.nbhppkkldpm";

    auto try_spawn_with_meta = [&](uintptr_t meta, const std::string& path, const char* reason) {
        if (!meta)
            return false;

        if (!path.empty()) {
            std::ostringstream out;
            out << "spawn_fish direct: using meta "
                << hex_u64(meta)
                << " path=" << path;
            log_line(out.str());
        }

        log_spawn_branch_diagnostics("before_call");
        if (!call_active_fish_spawn_path(*guid, meta)) {
            log_coordinate_snapshot("spawn_fish_direct_call_failed", before);
            return false;
        }
        log_spawn_branch_diagnostics("after_call");

        sleep_interruptible(2600);
        scan_fish_instances(true);
        const SensorState after = refresh_sensor_status();
        log_coordinate_snapshot(reason, after);

        const bool detected = has_detected_fish(after);
        std::ostringstream out;
        out << "spawn_fish direct: " << reason
            << (detected ? " detected active fish" : " no fish detected");
        log_line(out.str());
        return detected;
    };

    if (fish_bite_meta && try_spawn_with_meta(fish_bite_meta, meta_path, "spawn_fish_direct_live"))
        return true;

    if (!fish_bite_meta) {
        std::ostringstream out;
        out << "spawn_fish direct: missing setup/meta setup="
            << hex_u64(before.fishing_setup)
            << " biteMeta=" << hex_u64(before.fish_bite_meta);
        log_line(out.str());
        log_coordinate_snapshot("spawn_fish_direct_missing_meta", before);
        log_active_fish_object_map("spawn_fish_direct_missing_meta", g_probe);
    }

    for (int fish_family = 0; fish_family < 4; ++fish_family) {
        std::string fabricated_path;
        const uintptr_t fabricated_meta =
            create_debug_fish_bite_meta(fish_family, &fabricated_path);
        if (!fabricated_meta) {
            std::ostringstream out;
            out << "spawn_fish direct: failed to fabricate meta family=" << fish_family;
            log_line(out.str());
            continue;
        }

        std::ostringstream reason;
        reason << "spawn_fish_direct_fabricated_" << fish_family;
        if (try_spawn_with_meta(fabricated_meta, fabricated_path, reason.str().c_str()))
            return true;
    }

    return false;
}

void set_command_result_observed(
    fc::actions::Command command,
    bool result,
    bool confirmed,
    const std::string& observation)
{
    const std::string name = command_name(command);
    const std::string result_text =
        result ? (confirmed ? "confirmed" : "called") : "failed";
    const std::string text = name + ": " + result_text;

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_status.last_command = name;
        g_status.last_result = result_text;
        g_status.last_effect_confirmed = confirmed;
        g_status.last_observation = observation;
        g_status.message = text;
        g_status.busy = false;
        g_status.running = g_running.load();
        g_status.diagnostics_enabled = g_diagnostics_enabled;
        g_status.diagnostic_snapshots = g_diagnostic_snapshots;
        g_status.auto_reel_enabled = g_auto_reel_enabled;
        g_status.auto_reel_ticks = g_auto_reel_ticks;
    }

    log_line(text);
    if (!observation.empty())
        log_line(name + " observed: " + observation);
}

void set_command_result(fc::actions::Command command, bool result)
{
    set_command_result_observed(
        command,
        result,
        false,
        result ? "InputAction/call accepted; gameplay effect not verified for this button"
               : "call returned false");
}

void set_busy(fc::actions::Command command)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_status.busy = true;
    g_status.last_command = command_name(command);
    g_status.message = std::string("running: ") + command_name(command);
    g_status.running = g_running.load();
    g_status.diagnostics_enabled = g_diagnostics_enabled;
    g_status.diagnostic_snapshots = g_diagnostic_snapshots;
    g_status.auto_reel_enabled = g_auto_reel_enabled;
    g_status.auto_reel_ticks = g_auto_reel_ticks;
}

void update_diagnostics_status()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_status.diagnostics_enabled = g_diagnostics_enabled;
    g_status.diagnostic_snapshots = g_diagnostic_snapshots;
    g_status.auto_reel_enabled = g_auto_reel_enabled;
    g_status.auto_reel_ticks = g_auto_reel_ticks;
}

void toggle_periodic_diagnostics()
{
    const bool enabled = !g_diagnostics_enabled.load();
    g_diagnostics_enabled.store(enabled);
    g_next_diagnostic_snapshot = {};
    update_diagnostics_status();
    log_line(enabled ? "fishing diagnostics enabled" : "fishing diagnostics disabled");
}

void update_auto_reel_status(const char* message = nullptr)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_status.auto_reel_enabled = g_auto_reel_enabled;
    g_status.auto_reel_ticks = g_auto_reel_ticks;
    if (message)
        g_status.message = message;
}

void toggle_auto_reel()
{
    const bool enabled = !g_auto_reel_enabled.load();
    g_auto_reel_enabled.store(enabled);
    g_next_auto_reel_tick = {};
    update_auto_reel_status(
        enabled ? "auto reel enabled" : "auto reel disabled");
    log_line(enabled ? "auto_reel: enabled" : "auto_reel: disabled");
}

void maybe_auto_reel(const ActionSet& actions)
{
    (void)actions;

    if (!g_auto_reel_enabled.load())
        return;

    const auto now = std::chrono::steady_clock::now();
    if (g_next_auto_reel_tick.time_since_epoch().count() != 0 && now < g_next_auto_reel_tick)
        return;

    const SensorState before = refresh_sensor_status();
    const bool active_fishing =
        before.fishing_set_150 != 0 ||
        before.fishing_set_158 != 0 ||
        before.fishing_set_160 != 0 ||
        before.reel_state_flags != 0 ||
        std::fabs(before.reel_value) > 0.05f ||
        before.fisher_to_lure > 1.2f;
    const int interval_ms = active_fishing ? 1150 : 1800;
    g_next_auto_reel_tick = now + std::chrono::milliseconds(interval_ms);

    const bool fish_fight = before.fish_count > 0 || has_detected_fish(before);

    const unsigned long long tick = g_auto_reel_ticks.load();
    if (!fish_fight && (tick % 12) == 0)
        prepare_reel_for_retrieve();

    const bool ok = fish_fight
        ? send_fish_fight_hold(active_fishing ? 850 : 520)
        : send_left_mouse_hold(active_fishing ? 700 : 420, true);
    const unsigned long long new_tick = ++g_auto_reel_ticks;

    if ((new_tick % 20) == 0) {
        const SensorState after = refresh_sensor_status();
        std::ostringstream out;
        out << (ok ? "auto_reel: real tick " : "auto_reel: real tick failed ")
            << (fish_fight ? "mode=RMB+LMB " : "mode=Shift+LMB ")
            << "reel_delta=" << std::fixed << std::setprecision(3)
            << (before.reel_value - after.reel_value)
            << " distance_delta=" << (before.fisher_to_lure - after.fisher_to_lure)
            << " " << sensor_summary(after);
        log_line(out.str());
    }

    update_auto_reel_status(ok ? "auto reel running" : "auto reel action failed");
}

bool perform_real_retrieve(
    fc::actions::Command command,
    DWORD hold_ms,
    bool shift_boost,
    bool prepare_reel,
    bool fish_fight)
{
    const SensorState before = refresh_sensor_status();
    bool ok = true;

    if (!fish_fight && prepare_reel)
        ok = prepare_reel_for_retrieve() && ok;

    ok = (fish_fight ? send_fish_fight_hold(hold_ms)
                     : send_left_mouse_hold(hold_ms, shift_boost)) && ok;

    sleep_interruptible(650);
    const SensorState after = refresh_sensor_status();
    const float reel_delta = before.reel_value - after.reel_value;
    const float distance_delta = before.fisher_to_lure - after.fisher_to_lure;
    const float lure_delta = distance_between(before.best_lure_pos, after.best_lure_pos);
    const bool reel_changed = std::fabs(reel_delta) > 0.05f;
    const bool confirmed =
        ok &&
        (std::fabs(distance_delta) > 0.08f ||
         lure_delta > 0.08f ||
         (fish_fight && reel_changed));

    std::ostringstream observation;
    observation << "real "
        << (fish_fight ? "RMB+LMB fish-fight hold " : (shift_boost ? "Shift+LMB hold " : "LMB hold "))
        << hold_ms << "ms"
        << "; reel_delta=" << std::fixed << std::setprecision(3) << reel_delta
        << " distance_delta=" << distance_delta
        << " lure_delta=" << lure_delta
        << " fish=" << after.fish_count
        << " before_reel=" << before.reel_value
        << " after_reel=" << after.reel_value
        << " before_dist=" << before.fisher_to_lure
        << " after_dist=" << after.fisher_to_lure
        << (reel_changed && !confirmed ? "; reel state changed, tackle return not verified" : "");

    set_command_result_observed(command, ok, confirmed, observation.str());
    return confirmed;
}

bool perform_return_idle(const ActionSet& actions)
{
    const SensorState before = refresh_sensor_status();
    bool ok = false;

    if (actions.return_to_idle)
        ok = game_actions::pulse_input_action(actions.return_to_idle) || ok;

    sleep_interruptible(120);

    if (actions.debug_return_to_idle)
        ok = game_actions::pulse_input_action(actions.debug_return_to_idle) || ok;

    sleep_interruptible(850);
    const SensorState after = refresh_sensor_status();
    const float distance_delta = before.fisher_to_lure - after.fisher_to_lure;
    const float lure_delta = distance_between(before.best_lure_pos, after.best_lure_pos);
    const bool setup_cleared =
        (before.fishing_set_150 || before.fishing_set_158 || before.fishing_set_160) &&
        !after.fishing_set_150 &&
        !after.fishing_set_158 &&
        !after.fishing_set_160;
    const bool confirmed =
        ok &&
        (setup_cleared ||
         std::fabs(distance_delta) > 0.08f ||
         lure_delta > 0.08f);

    std::ostringstream observation;
    observation << "FishingSet.ReturnToIdle + Debug.ReturnToIdle"
        << "; setup_cleared=" << (setup_cleared ? "true" : "false")
        << " distance_delta=" << std::fixed << std::setprecision(3) << distance_delta
        << " lure_delta=" << lure_delta
        << " before_flags=" << hex_u64(before.fishing_set_160)
        << " after_flags=" << hex_u64(after.fishing_set_160);

    set_command_result_observed(
        fc::actions::Command::ReturnIdle,
        ok,
        confirmed,
        observation.str());
    return confirmed;
}

void maybe_log_periodic_diagnostics()
{
    if (!g_diagnostics_enabled.load())
        return;

    const auto now = std::chrono::steady_clock::now();
    if (g_next_diagnostic_snapshot.time_since_epoch().count() != 0 &&
        now < g_next_diagnostic_snapshot) {
        return;
    }

    g_next_diagnostic_snapshot = now + std::chrono::milliseconds(500);
    log_diagnostic_snapshot("periodic");
}

bool perform_auto_cast(const ActionSet& actions)
{
    const SensorState before = refresh_sensor_status();

    if (std::fabs(before.reel_value) > 0.5f || before.fisher_to_lure > 2.0f) {
        set_busy(fc::actions::Command::ManualRollBoost);
        perform_real_retrieve(
            fc::actions::Command::ManualRollBoost,
            8500,
            true,
            true,
            before.fish_count > 0 || has_detected_fish(before));
        sleep_interruptible(800);
    }

    const SensorState cast_before = refresh_sensor_status();
    const fc::actions::Command sequence[] = {
        fc::actions::Command::SwitchThrowMode,
        fc::actions::Command::ChangeThrowDistance,
        fc::actions::Command::Hitch,
    };

    bool ok = true;
    for (fc::actions::Command command : sequence) {
        set_busy(command);
        const bool result = perform_command(command, actions);
        set_command_result(command, result);
        ok = ok && result;
        sleep_interruptible(90);
    }

    set_busy(fc::actions::Command::StartHooking);
    sleep_interruptible(250);
    const bool cast_hold = send_left_mouse_hold(1600);
    set_command_result_observed(
        fc::actions::Command::StartHooking,
        cast_hold,
        false,
        cast_hold ? "sent real left mouse hold for 1600ms; waiting for lure movement"
                  : "real left mouse hold failed");
    ok = ok && cast_hold;

    sleep_interruptible(1800);
    const SensorState after = refresh_sensor_status();
    const float lure_delta = distance_between(cast_before.best_lure_pos, after.best_lure_pos);
    const bool observed =
        ok &&
        cast_before.has_best_lure_pos &&
        after.has_best_lure_pos &&
        lure_delta > 1.0f;

    std::ostringstream observation;
    observation << "lure delta=" << std::fixed << std::setprecision(2) << lure_delta
        << "m before=" << format_vec(cast_before.best_lure_pos)
        << " after=" << format_vec(after.best_lure_pos)
        << " fish=" << after.fish_count;
    set_command_result_observed(
        fc::actions::Command::AutoCast,
        ok,
        observed,
        observation.str());
    return observed;
}

void set_auto_reel_enabled(bool enabled)
{
    if (g_auto_reel_enabled.load() == enabled)
        return;

    g_auto_reel_enabled.store(enabled);
    g_next_auto_reel_tick = {};
    update_auto_reel_status(
        enabled ? "auto reel enabled" : "auto reel disabled");
    log_line(enabled ? "auto_reel: enabled" : "auto_reel: disabled");
}

void set_diagnostics_enabled(bool enabled)
{
    if (g_diagnostics_enabled.load() == enabled)
        return;

    g_diagnostics_enabled.store(enabled);
    g_next_diagnostic_snapshot = {};
    update_diagnostics_status();
    log_line(enabled ? "fishing diagnostics enabled" : "fishing diagnostics disabled");
}

void perform_auto_catch(const ActionSet& actions)
{
    bool ok = perform_auto_cast(actions);
    set_auto_reel_enabled(true);
    ok = log_diagnostic_snapshot("auto_catch") && ok;
    set_command_result(fc::actions::Command::AutoCatch, ok);
}

bool mark_current_spot(const char* reason)
{
    const SensorState state = refresh_sensor_status();
    Vector3 spot = state.best_lure_pos;
    if (!valid_vector(spot))
        spot = state.rod_mid;
    if (!valid_vector(spot))
        return false;

    g_marked_spot = spot;
    g_has_marked_spot = true;

    const SensorState marked_state = refresh_sensor_status();
    log_coordinate_snapshot(reason, marked_state);

    std::lock_guard<std::mutex> lock(g_mutex);
    g_status.message = std::string("marked spot: ") + format_vec(g_marked_spot);
    g_status.sensor_summary = sensor_summary(marked_state);
    return true;
}

void clear_marked_spot()
{
    g_has_marked_spot = false;
    g_marked_spot = {};
    refresh_sensor_status();

    std::lock_guard<std::mutex> lock(g_mutex);
    g_status.message = "marked spot cleared";
}

void perform_auto_scout(const ActionSet& actions)
{
    bool ok = perform_auto_cast(actions);
    set_diagnostics_enabled(true);
    sleep_interruptible(750);
    scan_fish_instances(true);
    ok = mark_current_spot("auto_scout_mark") && ok;
    ok = log_diagnostic_snapshot("auto_scout") && ok;
    set_command_result(fc::actions::Command::AutoScout, ok);
}

bool perform_fish_scan()
{
    scan_fish_instances(true);
    const SensorState state = refresh_sensor_status();
    const bool ok = log_coordinate_snapshot("scan_fish", state);

    std::lock_guard<std::mutex> lock(g_mutex);
    g_status.message = std::string("fish scanned: ") +
        std::to_string(state.fish_count);
    g_status.sensor_summary = sensor_summary(state);
    return ok;
}

bool perform_verified_fish_debug_command(fc::actions::Command command, const ActionSet& actions)
{
    if (command == fc::actions::Command::DebugSpawnFish) {
        if (perform_active_fish_spawn())
            return true;
        log_line("spawn_fish direct: falling back to debug InputAction pulse");
    }

    const bool pulse_ok = perform_command(command, actions);
    sleep_interruptible(700);
    scan_fish_instances(true);
    const SensorState state = refresh_sensor_status();
    log_coordinate_snapshot(command_name(command), state);
    return pulse_ok && has_detected_fish(state);
}

void perform_stop_all(const ActionSet& actions)
{
    set_auto_reel_enabled(false);
    set_diagnostics_enabled(false);

    bool ok = true;
    if (actions.return_to_idle)
        ok = game_actions::pulse_input_action(actions.return_to_idle);

    set_command_result(fc::actions::Command::StopAll, ok);
}

DWORD WINAPI worker_thread(void*)
{
    log_line("FishingCompanion action runtime started");

    update_ready_status(false, "discovering actions");

    while (g_running.load()) {
        if (!has_required_actions(g_actions)) {
            g_actions = discover_actions();

            if (!has_required_actions(g_actions)) {
                update_ready_status(false, "waiting for fishing actions");
                sleep_interruptible(1000);
                continue;
            }

            update_ready_status(true, "actions ready");
            log_line("actions ready");
        }

        process_command_file();
        refresh_sensor_status();
        maybe_auto_reel(g_actions);
        maybe_log_periodic_diagnostics();

        fc::actions::Command command{};
        if (!take_command(command))
            continue;

        if (command == fc::actions::Command::Refresh) {
            set_busy(command);
            update_ready_status(false, "refreshing actions");
            g_actions = {};
            set_command_result(command, true);
            continue;
        }

        if (command == fc::actions::Command::ToggleDiagnostics) {
            set_busy(command);
            toggle_periodic_diagnostics();
            if (g_diagnostics_enabled.load())
                log_diagnostic_snapshot("enabled");
            set_command_result(command, true);
            continue;
        }

        if (command == fc::actions::Command::SnapshotDiagnostics) {
            set_busy(command);
            set_command_result(command, log_diagnostic_snapshot("manual"));
            continue;
        }

        if (command == fc::actions::Command::ToggleAutoReel) {
            set_busy(command);
            toggle_auto_reel();
            set_command_result(command, true);
            continue;
        }

        if (command == fc::actions::Command::AutoCast) {
            perform_auto_cast(g_actions);
            continue;
        }

        if (command == fc::actions::Command::AutoCatch) {
            perform_auto_catch(g_actions);
            continue;
        }

        if (command == fc::actions::Command::AutoScout) {
            perform_auto_scout(g_actions);
            continue;
        }

        if (command == fc::actions::Command::StopAll) {
            perform_stop_all(g_actions);
            continue;
        }

        if (command == fc::actions::Command::MarkSpot) {
            set_busy(command);
            set_command_result(command, mark_current_spot("mark_spot"));
            continue;
        }

        if (command == fc::actions::Command::ClearSpot) {
            set_busy(command);
            clear_marked_spot();
            set_command_result(command, true);
            continue;
        }

        if (command == fc::actions::Command::ScanFish) {
            set_busy(command);
            set_command_result(command, perform_fish_scan());
            continue;
        }

        if (command == fc::actions::Command::ReturnIdle) {
            set_busy(command);
            perform_return_idle(g_actions);
            continue;
        }

        if (command == fc::actions::Command::ManualRoll) {
            set_busy(command);
            perform_real_retrieve(command, 3200, false, false, false);
            continue;
        }

        if (command == fc::actions::Command::ManualRollBoost) {
            set_busy(command);
            const SensorState state = refresh_sensor_status();
            perform_real_retrieve(
                command,
                6500,
                true,
                true,
                state.fish_count > 0 || has_detected_fish(state));
            continue;
        }

        if (command == fc::actions::Command::DebugSpawnFish ||
            command == fc::actions::Command::DebugFishJump ||
            command == fc::actions::Command::DebugCatchFish) {
            set_busy(command);
            set_command_result(command, perform_verified_fish_debug_command(command, g_actions));
            continue;
        }

        set_busy(command);
        set_command_result(command, perform_command(command, g_actions));
    }

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_status.running = false;
        g_status.ready = false;
        g_status.busy = false;
        g_status.message = "action runtime stopped";
        g_status.queued = 0;
        g_status.diagnostics_enabled = g_diagnostics_enabled;
        g_status.diagnostic_snapshots = g_diagnostic_snapshots;
        g_status.auto_reel_enabled = g_auto_reel_enabled;
        g_status.auto_reel_ticks = g_auto_reel_ticks;
    }

    log_line("FishingCompanion action runtime stopped");
    return 0;
}

} // namespace

namespace fc::actions {

bool Start()
{
    bool expected = false;
    if (!g_running.compare_exchange_strong(expected, true))
        return true;

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_status = {};
        g_status.running = true;
        g_status.message = "starting action runtime";
        g_status.diagnostics_enabled = g_diagnostics_enabled;
        g_status.diagnostic_snapshots = g_diagnostic_snapshots;
        g_status.auto_reel_enabled = g_auto_reel_enabled;
        g_status.auto_reel_ticks = g_auto_reel_ticks;
    }

    g_thread = CreateThread(nullptr, 0, worker_thread, nullptr, 0, nullptr);
    if (!g_thread) {
        g_running = false;
        set_message("failed to start action runtime");
        return false;
    }

    return true;
}

void Stop()
{
    const bool was_running = g_running.exchange(false);
    g_cv.notify_all();

    if (g_thread) {
        WaitForSingleObject(g_thread, INFINITE);
        CloseHandle(g_thread);
        g_thread = nullptr;
    }

    if (!was_running)
        return;

    std::lock_guard<std::mutex> lock(g_mutex);
    g_queue.clear();
    g_status.queued = 0;
    g_status.running = false;
    g_status.ready = false;
    g_status.busy = false;
    g_status.diagnostics_enabled = g_diagnostics_enabled;
    g_status.diagnostic_snapshots = g_diagnostic_snapshots;
    g_status.auto_reel_enabled = g_auto_reel_enabled;
    g_status.auto_reel_ticks = g_auto_reel_ticks;
}

void Queue(Command command)
{
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!g_running.load()) {
            g_status.message = "action runtime is not running";
            return;
        }

        g_queue.push_back(command);
        g_status.queued = static_cast<unsigned int>(g_queue.size());
        g_status.message = std::string("queued: ") + command_name(command);
        g_status.auto_reel_enabled = g_auto_reel_enabled;
        g_status.auto_reel_ticks = g_auto_reel_ticks;
    }

    g_cv.notify_one();
}

Status GetStatus()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    Status copy = g_status;
    copy.running = g_running.load();
    copy.queued = static_cast<unsigned int>(g_queue.size());
    copy.diagnostics_enabled = g_diagnostics_enabled;
    copy.diagnostic_snapshots = g_diagnostic_snapshots;
    copy.auto_reel_enabled = g_auto_reel_enabled;
    copy.auto_reel_ticks = g_auto_reel_ticks;
    return copy;
}

} // namespace fc::actions
