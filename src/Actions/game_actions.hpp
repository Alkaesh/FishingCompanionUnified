#pragma once

#include "il2cpp_interaction_sdk.hpp"
#include "il2cpp_unity_object_finder.hpp"

#include "../generated/action_offsets.hpp"
#include "../generated/rf4_offsets.hpp"

#include <vector>

namespace game_actions {

namespace input_internal {

inline constexpr uintptr_t input_action_map_field = 0xC8;
inline constexpr uintptr_t input_action_map_state_field = 0x60;
inline constexpr DWORD input_action_pulse_hold_ms = 120;

inline constexpr uintptr_t input_action_state_fetch_action_state_method = 0x2BFAEB0;
inline constexpr uintptr_t input_action_state_change_phase_of_action_method = 0x2BFE810;
inline constexpr uintptr_t trigger_state_set_magnitude_method = 0x2C02D00;
inline constexpr uintptr_t trigger_state_set_is_button_method = 0x2C02FA0;
inline constexpr uintptr_t trigger_state_set_is_pressed_method = 0x2C02FE0;

enum class InputActionPhase : int {
    Disabled = 0,
    Waiting = 1,
    Started = 2,
    Performed = 3,
    Canceled = 4,
};

} // namespace input_internal

inline void* read_object_field(void* instance, uintptr_t fieldOffset) {
    if (!instance) {
        return nullptr;
    }

    uintptr_t address = 0;
    if (il2cpp_runtime::detail::add_overflows(
            reinterpret_cast<uintptr_t>(instance),
            fieldOffset,
            address)) {
        return nullptr;
    }

    auto* field = reinterpret_cast<void**>(address);
    if (!il2cpp_runtime::is_readable_span(field, sizeof(void*))) {
        return nullptr;
    }

    return *field;
}

inline void* input_system_actions() {
    il2cpp_runtime::StaticMethod<void*> get_actions(
        action_offsets::Synth_4212_Closure_mggjekpbmah_0_method);
    const auto result = get_actions.call();
    return result ? *result : nullptr;
}

inline void* action_at(void* inputActions, uintptr_t fieldOffset) {
    return read_object_field(inputActions, fieldOffset);
}

inline bool enable_input_action(void* inputAction) {
    if (!inputAction) {
        return false;
    }
    il2cpp_runtime::InstanceMethod<void> enable(
        action_offsets::UnityEngine_InputSystem_InputAction_Enable_0_method);
    return enable.call(inputAction);
}

inline bool input_action_is_pressed(void* inputAction) {
    if (!inputAction) {
        return false;
    }
    il2cpp_runtime::InstanceMethod<bool> isPressed(
        action_offsets::UnityEngine_InputSystem_InputAction_IsPressed_0_method);
    const auto result = isPressed.call(inputAction);
    return result && *result;
}

inline bool input_action_was_pressed_this_frame(void* inputAction) {
    if (!inputAction) {
        return false;
    }
    il2cpp_runtime::InstanceMethod<bool> wasPressed(
        action_offsets::UnityEngine_InputSystem_InputAction_WasPressedThisFrame_0_method);
    const auto result = wasPressed.call(inputAction);
    return result && *result;
}

inline int input_action_phase(void* inputAction) {
    if (!inputAction) {
        return -1;
    }
    il2cpp_runtime::InstanceMethod<int> getPhase(
        action_offsets::UnityEngine_InputSystem_InputAction_get_phase_0_method);
    const auto result = getPhase.call(inputAction);
    return result ? *result : -1;
}

inline bool input_action_enabled(void* inputAction) {
    if (!inputAction) {
        return false;
    }
    il2cpp_runtime::InstanceMethod<bool> getEnabled(
        action_offsets::UnityEngine_InputSystem_InputAction_get_enabled_0_method);
    const auto result = getEnabled.call(inputAction);
    return result && *result;
}

inline bool input_action_triggered(void* inputAction) {
    if (!inputAction) {
        return false;
    }
    il2cpp_runtime::InstanceMethod<bool> getTriggered(
        action_offsets::UnityEngine_InputSystem_InputAction_get_triggered_0_method);
    const auto result = getTriggered.call(inputAction);
    return result && *result;
}

inline void* input_action_map(void* inputAction) {
    return read_object_field(inputAction, input_internal::input_action_map_field);
}

inline void* input_action_state(void* inputAction) {
    void* map = input_action_map(inputAction);
    if (!map) {
        return nullptr;
    }
    return read_object_field(map, input_internal::input_action_map_state_field);
}

inline void* input_action_trigger_state(void* inputAction) {
    void* state = input_action_state(inputAction);
    if (!state) {
        return nullptr;
    }

    using FetchActionStateFn = void* (*)(void*, void*);
    il2cpp_runtime::Module module;
    const auto address = module.executable_address(
        input_internal::input_action_state_fetch_action_state_method);
    auto fn = address ? reinterpret_cast<FetchActionStateFn>(*address) : nullptr;
    return fn ? fn(state, inputAction) : nullptr;
}

inline bool perform_input_action_once(void* inputAction) {
    if (!inputAction) {
        return false;
    }

    enable_input_action(inputAction);

    void* state = input_action_state(inputAction);
    void* triggerState = input_action_trigger_state(inputAction);
    if (!state || !triggerState) {
        return false;
    }

    using ChangePhaseOfActionFn = bool (*)(void*, int, void*, int);
    il2cpp_runtime::Module module;
    const auto address = module.executable_address(
        input_internal::input_action_state_change_phase_of_action_method);
    auto fn = address ? reinterpret_cast<ChangePhaseOfActionFn>(*address) : nullptr;
    if (!fn) {
        return false;
    }

    return fn(
        state,
        static_cast<int>(input_internal::InputActionPhase::Performed),
        triggerState,
        static_cast<int>(input_internal::InputActionPhase::Waiting));
}

inline bool change_input_action_phase(
    void* inputAction,
    input_internal::InputActionPhase phase,
    input_internal::InputActionPhase phaseAfterPerformed =
        input_internal::InputActionPhase::Waiting) {
    if (!inputAction) {
        return false;
    }

    enable_input_action(inputAction);

    void* state = input_action_state(inputAction);
    void* triggerState = input_action_trigger_state(inputAction);
    if (!state || !triggerState) {
        return false;
    }

    using ChangePhaseOfActionFn = bool (*)(void*, int, void*, int);
    il2cpp_runtime::Module module;
    const auto address = module.executable_address(
        input_internal::input_action_state_change_phase_of_action_method);
    auto fn = address ? reinterpret_cast<ChangePhaseOfActionFn>(*address) : nullptr;
    if (!fn) {
        return false;
    }

    return fn(
        state,
        static_cast<int>(phase),
        triggerState,
        static_cast<int>(phaseAfterPerformed));
}

inline bool set_trigger_button_state(void* inputAction, bool pressed) {
    void* triggerState = input_action_trigger_state(inputAction);
    if (!triggerState) {
        return false;
    }

    il2cpp_runtime::InstanceMethod<void, bool> setIsButton(
        input_internal::trigger_state_set_is_button_method);
    il2cpp_runtime::InstanceMethod<void, bool> setIsPressed(
        input_internal::trigger_state_set_is_pressed_method);
    il2cpp_runtime::InstanceMethod<void, float> setMagnitude(
        input_internal::trigger_state_set_magnitude_method);

    const bool okButton = setIsButton.call(triggerState, true);
    const bool okPressed = setIsPressed.call(triggerState, pressed);
    const bool okMagnitude = setMagnitude.call(triggerState, pressed ? 1.0f : 0.0f);
    return okButton && okPressed && okMagnitude;
}

inline bool pulse_input_action(void* inputAction) {
    if (!inputAction) {
        return false;
    }

    set_trigger_button_state(inputAction, true);
    const bool started = change_input_action_phase(
        inputAction,
        input_internal::InputActionPhase::Started);
    const bool performed = change_input_action_phase(
        inputAction,
        input_internal::InputActionPhase::Performed);

    if (started || performed) {
        Sleep(input_internal::input_action_pulse_hold_ms);
    }

    set_trigger_button_state(inputAction, false);
    const bool canceled = change_input_action_phase(
        inputAction,
        input_internal::InputActionPhase::Canceled);

    return performed && canceled;
}

inline void* fishing_toggle_reel_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_Fishing_ToggleReel_field);
}

inline void* fishing_start_hooking_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_Fishing_StartHooking_field);
}

inline void* fishing_alternative_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_Fishing_AlternativeAction_field);
}

inline void* fishing_toggle_podsak_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_Fishing_TogglePodsak_field);
}

inline void* fishing_set_switch_throw_mode_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_FishingSet_SwitchThrowMode_field);
}

inline void* fishing_set_cut_fishing_line_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_FishingSet_CutFishingLine_field);
}

inline void* fishing_set_toggle_reel_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_FishingSet_ToggleReel_field);
}

inline void* fishing_set_change_reel_clip_position_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_FishingSet_ChangeReelClipPosition_field);
}

inline void* fishing_set_hitch_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_FishingSet_Hitch_field);
}

inline void* fishing_set_return_to_idle_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_FishingSet_ReturnToIdle_field);
}

inline void* hand_item_change_throw_distance_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_HandItem_ChangeThrowDistance_field);
}

inline void* hand_item_hot_swap_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_HandItem_HotSwap_field);
}

inline void* interactions_rod_to_rodrest_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_Interactions_RodToRodrest_field);
}

inline void* interactions_rod_slot_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_Interactions_RodSlot_field);
}

inline void* fishing_rig_hot_swap_bait1_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_FishingRig_HotSwapBait1_field);
}

inline void* fishing_rig_hot_swap_bait2_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_FishingRig_HotSwapBait2_field);
}

inline void* fishing_rig_change_reel_clip_position_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_FishingRig_ChangeReelClipPosition_field);
}

inline void* fishing_rig_change_bobber_depth_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_FishingRig_ChangeBobberDepth_field);
}

inline void* fishing_reel_manual_roll_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_FishingReel_ManualRoll_field);
}

inline void* fishing_reel_manual_roll_boost_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_FishingReel_ManualRollBoost_field);
}

inline void* fishing_reel_switch_speed_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_FishingReel_SwitchSpeed_field);
}

inline void* fishing_reel_change_transmission_mode_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_FishingReel_ChangeTransmissionMode_field);
}

inline void* fishing_reel_toggle_auto_roll_mode_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_FishingReel_ToggleAutoRollMode_field);
}

inline void* fishing_reel_reset_auto_roll_mode_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_FishingReel_ResetAutoRollMode_field);
}

inline void* fishing_reel_change_friction_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_FishingReel_ChangeFriction_field);
}

inline void* fishing_reel_roll_speed_mode_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_FishingReel_RollSpeedMode_field);
}

inline void* fishing_reel_change_roll_speed_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_FishingReel_ChangeRollSpeed_field);
}

inline void* fishing_reel_toggle_engine_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_FishingReel_ToggleEngine_field);
}

inline void* fishing_reel_toggle_transmission_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_FishingReel_ToggleTransmission_field);
}

inline void* debug_catch_fish_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_Debug_CatchFish_field);
}

inline void* debug_return_to_idle_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_Debug_ReturnToIdle_field);
}

inline void* debug_repair_rod_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_Debug_RepairRod_field);
}

inline void* debug_spawn_fish_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_Debug_SpawnFish_field);
}

inline void* debug_fish_jump_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_Debug_FishJump_field);
}

inline void* debug_level_up_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_Debug_LevelUp_field);
}

inline void* debug_hitch_action() {
    return action_at(
        input_system_actions(),
        action_offsets::UnityInput_Generated_InputSystemActions_m_Debug_Hitch_field);
}

inline void* find_fishing_scene_input_controller() {
    il2cpp_runtime::UnityObjectFinder finder(
        action_offsets::UnityEngine_Object_FindObjectsOfType_1_method);
    return finder.find_first(
        "Assembly-CSharp.dll",
        "RF4.Client.FishingScene",
        "FishingSceneInputController");
}

inline void* find_fishing_set() {
    il2cpp_runtime::UnityObjectFinder finder(
        action_offsets::UnityEngine_Object_FindObjectsOfType_1_method);
    return finder.find_first(
        "Assembly-CSharp.dll",
        "RF4.Client.FishingScene",
        "FishingSet");
}

inline void* find_fisher() {
    il2cpp_runtime::UnityObjectFinder finder(
        action_offsets::UnityEngine_Object_FindObjectsOfType_1_method);
    return finder.find_first(
        "Assembly-CSharp.dll",
        "RF4.Client.FishingScene",
        "Fisher");
}

inline void* find_rod() {
    il2cpp_runtime::UnityObjectFinder finder(
        action_offsets::UnityEngine_Object_FindObjectsOfType_1_method);
    return finder.find_first(
        "Assembly-CSharp.dll",
        "RF4.Client.FishingScene",
        "Rod");
}

inline void* find_reel() {
    il2cpp_runtime::UnityObjectFinder finder(
        action_offsets::UnityEngine_Object_FindObjectsOfType_1_method);
    return finder.find_first(
        "Assembly-CSharp.dll",
        "RF4.Client.FishingScene",
        "Reel");
}

inline void* find_lure_complex() {
    il2cpp_runtime::UnityObjectFinder finder(
        action_offsets::UnityEngine_Object_FindObjectsOfType_1_method);
    return finder.find_first(
        "Assembly-CSharp.dll",
        "RF4.Client.FishingScene",
        "LureComplex");
}

inline std::vector<void*> find_all_fish() {
    il2cpp_runtime::UnityObjectFinder finder(
        action_offsets::UnityEngine_Object_FindObjectsOfType_1_method);
    return finder.find_all(
        "Assembly-CSharp.dll",
        "RF4.Client.FishingScene",
        "Fish");
}

inline bool capture_common_instances() {
    bool captured = false;

    if (void* input = find_fishing_scene_input_controller()) {
        captured |= il2cpp_runtime::InstanceTracker::get().capture(
            "FishingSceneInputController",
            input);
    }

    if (void* set = find_fishing_set()) {
        captured |= il2cpp_runtime::InstanceTracker::get().capture(
            "FishingSet",
            set);
    }

    if (void* fisher = find_fisher()) {
        captured |= il2cpp_runtime::InstanceTracker::get().capture(
            "Fisher",
            fisher);
    }

    if (void* rod = find_rod()) {
        captured |= il2cpp_runtime::InstanceTracker::get().capture(
            "Rod",
            rod);
    }

    if (void* reel = find_reel()) {
        captured |= il2cpp_runtime::InstanceTracker::get().capture(
            "Reel",
            reel);
    }

    if (void* lure = find_lure_complex()) {
        captured |= il2cpp_runtime::InstanceTracker::get().capture(
            "LureComplex",
            lure);
    }

    return captured;
}

inline bool call_interaction_update_if_present() {
    void* self = il2cpp_runtime::InstanceTracker::get().instance(
        "InteractionController");
    if (!self) {
        return false;
    }

    il2cpp_runtime::InstanceMethod<void> update(
        rf4_offsets::RF4_Client_Water_InteractionController_Update_method);
    return update.call(self);
}

} // namespace game_actions
