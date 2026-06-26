#include "../include/il2cpp_interaction_sdk.hpp"

namespace example {

// This skeleton shows the boundary between the SDK and a hook library.
// Implement it with an authorized backend such as MinHook or Detours in your
// own project.

class ExampleHookBackend final : public il2cpp_runtime::IHookBackend {
public:
    bool install(const il2cpp_runtime::HookSpec& spec) override {
        il2cpp_runtime::InteractionContext context;
        const uintptr_t targetAddress = context.absolute(spec.method_rva);
        if (!targetAddress || !spec.detour || !spec.original) {
            return false;
        }

        // MinHook-style pseudocode:
        // MH_CreateHook(
        //     reinterpret_cast<void*>(targetAddress),
        //     spec.detour,
        //     spec.original);
        //
        // Detours-style pseudocode:
        // *spec.original = reinterpret_cast<void*>(targetAddress);
        // DetourAttach(spec.original, spec.detour);

        return false;
    }

    bool enable_all() override {
        // MH_EnableHook(MH_ALL_HOOKS);
        return false;
    }

    void shutdown() override {
        // MH_DisableHook(MH_ALL_HOOKS);
        // MH_Uninitialize();
    }
};

} // namespace example
