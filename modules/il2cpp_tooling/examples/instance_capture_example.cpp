#include "../include/il2cpp_instance_tracker.hpp"
#include "../include/il2cpp_runtime_sdk.hpp"

#include <cstdint>

namespace example {

// Example target:
//   class MyController {
//       void Update();
//       bool enabled; // offset known from generated offsets
//   };
//
// Native IL2CPP instance methods receive `this` as the first argument.
// After a hook backend redirects MyController::Update to hk_update, the first
// parameter is the object pointer we want to keep.

using UpdateFn = void (*)(void* self);

inline UpdateFn original_update = nullptr;

void hk_update(void* self) {
    il2cpp_runtime::InstanceTracker::get().capture(
        "MyController",
        self,
        reinterpret_cast<uintptr_t>(hk_update));

    if (original_update) {
        original_update(self);
    }
}

void use_captured_instance(uintptr_t updateRva, uintptr_t enabledFieldOffset) {
    void* self = il2cpp_runtime::InstanceTracker::get().instance("MyController");
    if (!self) {
        return;
    }

    il2cpp_runtime::Field<bool> enabled(enabledFieldOffset);
    const auto current = enabled.read(self);
    if (current) {
        enabled.write(self, *current);
    }

    il2cpp_runtime::ThreadAttach attach;
    if (!attach.attached()) {
        return;
    }

    il2cpp_runtime::InstanceMethod<void> update(updateRva);
    update.call(self);
}

} // namespace example
