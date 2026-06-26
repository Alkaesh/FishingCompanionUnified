#pragma once

#include <cstdint>
#include "../include/il2cpp_runtime_sdk.hpp"

namespace rf4_offsets {

struct TargetInfo {
    const char* assembly;
    const char* type;
    const char* alias;
    const char* member;
    const char* kind;
    uintptr_t offset;
};

inline constexpr uintptr_t RF4_Client_Water_InteractionController_Update_method = 0x4A6FC0;
inline constexpr uintptr_t RF4_Client_Water_InteractionController_followWaterFlow_field = 0x20;

inline constexpr TargetInfo targets[] = {
    {"Assembly-CSharp.dll", "RF4.Client.Water.InteractionController", "RF4.Client.Water.InteractionController", "Update(0)", "method", 0x4A6FC0},
    {"Assembly-CSharp.dll", "RF4.Client.Water.InteractionController", "RF4.Client.Water.InteractionController", "followWaterFlow", "field", 0x20},
};

} // namespace rf4_offsets
