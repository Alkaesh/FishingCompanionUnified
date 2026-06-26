#include <windows.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "../include/il2cpp_interaction_sdk.hpp"
#include "project_offsets.hpp"

namespace {

using ExampleUpdateFn = void (*)(void*);

enum class HookInstallState {
    Installed,
    BackendMissing,
    TargetMissing,
    Failed
};

ExampleUpdateFn g_originalExampleUpdate = nullptr;
il2cpp_runtime::InteractionContext g_context;
il2cpp_runtime::NoopHookBackend g_hookBackend;

std::wstring process_directory() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);

    std::wstring value(path);
    const auto pos = value.find_last_of(L"\\/");
    if (pos == std::wstring::npos) {
        return L".";
    }
    return value.substr(0, pos);
}

void log_line(const std::string& text) {
    const std::filesystem::path path = std::filesystem::path(process_directory()) /
        L"il2cpp_interaction_module.log";
    std::ofstream out(path, std::ios::out | std::ios::app);
    if (out) {
        out << text << '\n';
    }
}

void write_status(const std::string& text) {
    const std::filesystem::path path = std::filesystem::path(process_directory()) /
        L"il2cpp_interaction_status.txt";
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (out) {
        out << text << '\n';
    }
}

std::string hex_value(uintptr_t value) {
    std::ostringstream out;
    out << "0x" << std::uppercase << std::hex << value;
    return out.str();
}

void log_target_table() {
    for (const auto& raw : project_offsets::targets) {
        const auto target = il2cpp_runtime::InteractionContext::view(raw);
        const uintptr_t absolute = target.kind && std::string(target.kind) == "method"
            ? g_context.absolute(target.offset)
            : 0;

        log_line(
            std::string("target ") +
            (target.alias ? target.alias : "") + "::" +
            (target.member ? target.member : "") +
            " kind=" + (target.kind ? target.kind : "") +
            " offset=" + hex_value(target.offset) +
            " absolute=" + hex_value(absolute));
    }
}

void hk_example_update(void* self) {
    g_context.capture(
        "ExampleController",
        self,
        reinterpret_cast<uintptr_t>(hk_example_update));

    if (g_originalExampleUpdate) {
        g_originalExampleUpdate(self);
    }
}

HookInstallState install_hooks() {
    const auto update = il2cpp_runtime::InteractionContext::find(
        project_offsets::targets,
        "RF4.Client.Water.InteractionController",
        "Update(0)",
        "method");

    if (!update) {
        log_line("example Update target was not found in generated offsets");
        return HookInstallState::TargetMissing;
    }

    il2cpp_runtime::HookSpec spec{
        "ExampleController",
        update->offset,
        reinterpret_cast<void*>(&hk_example_update),
        reinterpret_cast<void**>(&g_originalExampleUpdate)
    };

    // Replace NoopHookBackend with an authorized backend implementation.
    // g_hookBackend.install(spec) should hook g_context.absolute(spec.method_rva).
    if (!g_hookBackend.install(spec)) {
        log_line("hook backend is not configured; SDK context is ready");
        return HookInstallState::BackendMissing;
    }

    return g_hookBackend.enable_all()
        ? HookInstallState::Installed
        : HookInstallState::Failed;
}

bool tick_actions() {
    void* example = g_context.instance("ExampleController");
    if (!example) {
        return false;
    }

    const auto field = il2cpp_runtime::InteractionContext::find(
        project_offsets::targets,
        "RF4.Client.Water.InteractionController",
        "followWaterFlow",
        "field");

    if (!field) {
        return false;
    }

    il2cpp_runtime::Field<bool> followWaterFlow(field->offset);
    const auto current = followWaterFlow.read(example);
    if (current) {
        // Read/write path is wired. Keep value unchanged in the template.
        followWaterFlow.write(example, *current);
        return true;
    }

    return false;
}

DWORD WINAPI worker_thread(void*) {
    il2cpp_runtime::ThreadAttach attach;
    log_line("interaction module loaded");
    if (!attach.attached()) {
        log_line("il2cpp_thread_attach failed");
        write_status("not ready: il2cpp_thread_attach failed");
        return 0;
    }

    if (!g_context.ready()) {
        log_line("GameAssembly.dll was not found");
        write_status("not ready: GameAssembly.dll was not found");
        return 0;
    }

    log_target_table();

    const HookInstallState hookState = install_hooks();
    if (hookState == HookInstallState::Failed) {
        log_line("failed to install hooks");
        write_status("not ready: failed to install hooks");
        return 0;
    }

    if (hookState == HookInstallState::Installed) {
        log_line("hooks installed");
        write_status("ready: hooks installed; waiting for captured instances");
    } else if (hookState == HookInstallState::BackendMissing) {
        log_line("hooks pending: hook backend is not configured");
        write_status("sdk ready, but no hook backend is configured; no this pointer can be captured yet");
    } else {
        log_line("hooks pending: target was not found");
        write_status("sdk ready, but example hook target was not found in generated offsets");
    }

    uint64_t ticks = 0;
    for (;;) {
        const bool acted = tick_actions();
        if ((ticks++ % 50) == 0) {
            write_status(acted
                ? "running: captured instance found and action tick executed"
                : "running: no captured instance yet");
        }
        Sleep(100);
    }
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(nullptr, 0, worker_thread, module, 0, nullptr);
        if (thread) {
            CloseHandle(thread);
        }
    } else if (reason == DLL_PROCESS_DETACH) {
        g_hookBackend.shutdown();
    }
    return TRUE;
}
