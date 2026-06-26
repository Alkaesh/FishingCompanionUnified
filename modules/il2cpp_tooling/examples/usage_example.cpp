#include "../generated/rf4_offsets.hpp"
#include "../include/il2cpp_runtime_sdk.hpp"

namespace example {

// This example intentionally does not search for objects or hook game code.
// It shows how to use generated offsets after you already have a valid object
// pointer and know the method signature.

void read_or_write_field(void* interactionController) {
    il2cpp_runtime::Field<bool> followWaterFlow(
        rf4_offsets::RF4_Client_Water_InteractionController_followWaterFlow_field);

    const auto current = followWaterFlow.read(interactionController);
    if (!current) {
        return;
    }

    // Example write. Only do this in authorized/debug builds.
    followWaterFlow.write(interactionController, *current);
}

void call_instance_update(void* interactionController) {
    il2cpp_runtime::ThreadAttach attach;
    if (!attach.attached()) {
        return;
    }

    il2cpp_runtime::InstanceMethod<void> update(
        rf4_offsets::RF4_Client_Water_InteractionController_Update_method);

    update.call(interactionController);
}

uintptr_t get_update_address() {
    il2cpp_runtime::Module gameAssembly;
    return gameAssembly.address(
        rf4_offsets::RF4_Client_Water_InteractionController_Update_method);
}

} // namespace example
