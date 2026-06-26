# Project Audit

Date: 2026-06-25

Scope: read-only audit of the C++/Windows IL2CPP runtime tooling, action modules, controller UI, PowerShell generators, build scripts, generated artifacts, and repository layout.

Three sub-agents were used:

- C++ memory safety and Windows API misuse.
- IL2CPP/runtime architecture and offset interaction.
- Build, release, repository, and tooling hygiene.

No source files were modified during the audit.

## Executive Summary

The project has a useful separation between offset dumping, generated SDK headers, interaction templates, and action/controller modules. The main risk is that the runtime layer currently treats many native pointers, RVAs, IL2CPP object pointers, arrays, fields, and vtable slots as trusted. That creates a high crash/corruption risk when offsets are stale, a game update changes layout, or a pointer is merely committed memory but not a live IL2CPP object.

The most important fixes are:

1. Centralize RVA and memory-span validation before every function call or field access.
2. Move Unity object discovery and InputSystem mutation onto Unity's main thread.
3. Replace long-lived raw managed-object pointers with explicit lifetime handles or hook-scoped use.
4. Stop committing build outputs and full generated dumps; make builds reproducible from source plus explicit inputs.
5. Complete the CMake build so it covers every target currently built by separate `.bat` scripts.

## High Severity Findings

### 1. Unchecked RVAs Can Jump Into Invalid Memory

Files:

- `include/il2cpp_runtime_sdk.hpp:24`
- `include/il2cpp_runtime_sdk.hpp:108`
- `include/il2cpp_runtime_sdk.hpp:151`
- `generated/action_offsets.hpp:618`

`Module::address()` returns `base + rva` for any value, and `StaticMethod` / `InstanceMethod` only check that the resulting function pointer is non-null. This accepts `0`, stale offsets, sentinel-like garbage, and out-of-image values.

The generated header already contains suspicious constants such as `0xFFFF80011A960000`, which would become a bogus call target if routed through the method wrappers.

Impact:

- Access violation or jump into data/unmapped memory.
- Hard-to-debug crashes after game updates.
- Potential memory corruption if a stale RVA points into executable but wrong code.

Recommended fix:

- Add centralized RVA validation.
- Require `rva > 0`.
- Check integer overflow on `base + rva`.
- Check address is within the `GameAssembly.dll` image using PE `SizeOfImage`.
- Check page is committed and executable with `VirtualQuery`.
- Return `std::optional<Fn>` or a status object rather than a raw callable pointer.

### 2. Field Helpers Look Safe But Do Not Validate Memory Ranges

Files:

- `include/il2cpp_runtime_sdk.hpp:68`
- `include/il2cpp_runtime_sdk.hpp:75`
- `include/il2cpp_runtime_sdk.hpp:83`
- `include/game_actions.hpp:43`
- `include/il2cpp_instance_tracker.hpp:101`
- `include/il2cpp_instance_tracker.hpp:116`

`Field<T>::read/write` return `optional` / `bool`, but only guard against a null instance. Wrong offsets, stale objects, or partially valid pages can still crash or corrupt memory. Some helper paths dereference `inputActions + fieldOffset` directly.

`write_field` also accepts memory protections that are readable but not writable, such as `PAGE_READONLY` and `PAGE_EXECUTE_READ`, and does not validate that the full `sizeof(T)` span fits in the region.

Impact:

- Access violation on read/write.
- Writes to read-only or executable-read pages.
- Cross-page read/write crashes.
- Silent memory corruption when offsets no longer match layout.

Recommended fix:

- Centralize `is_readable_span(ptr, size)` and `is_writable_span(ptr, size)`.
- Reject `PAGE_GUARD`, `PAGE_NOACCESS`, and non-committed pages.
- For writes, require writable protections only.
- Check `begin + size` overflow and region bounds.
- Make unchecked raw access explicit with names like `unsafe_ptr`.

### 3. Unity APIs and InputSystem Mutation Run From Worker Threads

Files:

- `src/dllmain.cpp:871`
- `include/il2cpp_unity_object_finder.hpp:128`
- `action_module/action_module.cpp:1423`
- `action_module/action_module.cpp:1482`

The DLLs create worker threads and call IL2CPP / Unity APIs from those threads. `ThreadAttach` attaches a native thread to IL2CPP, but that does not make Unity object discovery, UnityEngine APIs, InputSystem state changes, or dispatcher/cast calls safe from a non-main thread.

Impact:

- Unity runtime assertions, deadlocks, or random crashes.
- InputSystem state corruption or inconsistent behavior.
- Game update sensitivity because threading assumptions are implicit.

Recommended fix:

- Keep file polling / IPC on the worker thread only.
- Enqueue commands into a thread-safe queue.
- Execute Unity object discovery and InputSystem mutations from a hooked `Update` / main-thread callback.
- Attach to IL2CPP only where needed and fail closed if attach fails.

### 4. Captured Instances Are Raw Managed Pointers With Weak Lifetime Guarantees

Files:

- `include/il2cpp_instance_tracker.hpp:31`
- `include/il2cpp_instance_tracker.hpp:46`

`InstanceTracker` stores raw `void*` pointers. `VirtualQuery` only proves that an address points into committed memory. It does not prove the object is alive, the expected type, not destroyed by Unity, or not re-used for something else.

Impact:

- Use-after-free.
- Calls on the wrong object type.
- Reads/writes through stale pointers that still pass memory-commit checks.

Recommended fix:

- Store `il2cpp_gchandle` or another explicit managed lifetime handle where appropriate.
- Validate expected class/type before use.
- Capture and clear on lifecycle hooks such as `OnDestroy`.
- Prefer short-lived hook-scoped use over long-lived raw pointer storage.

### 5. Hard-Coded IL2CPP Class Layout and Static Field Offsets

Files:

- `action_module/action_module.cpp:168`
- `action_module/action_module.cpp:180`
- `action_module/action_module.cpp:184`
- `action_module/action_module.cpp:195`
- `action_module/action_module.cpp:227`

The action module assumes a class global RVA plus fixed `Il2CppClass` layout offsets such as `klass + 224` and `klass + 184`, then reads/writes static fields at `+120` and `+124`.

Impact:

- Immediate crashes after Unity/IL2CPP/game updates.
- Reads from wrong class metadata layout.
- Writes to stale or wrong static field storage.

Recommended fix:

- Resolve class and fields through IL2CPP metadata APIs.
- Prefer official static field APIs when available.
- If raw layout is unavoidable, gate it behind verified Unity/metadata version checks and validate every address span before reading/writing.

### 6. Vtable Slot Reads Are Partially Validated

File:

- `action_module/action_module.cpp:379`

`compute_full_cast_power()` validates around `vtable + 0xA70`, then reads both `vtable + 0xA68` and `vtable + 0xA70`. The `+0xA68` read is not independently validated, and the slot reads happen outside the SEH-protected call.

Impact:

- Crash before the guarded function call.
- Stale or wrong vtable layout can call arbitrary wrong code.

Recommended fix:

- Validate both slots independently.
- Validate `fnPtr` is executable and inside an expected module.
- Move raw slots into named versioned constants and validate the target version before use.

### 7. IL2CPP Worker Thread in Dumper Is Not Attached

File:

- `src/dllmain.cpp:871`

The main dumper's worker thread calls `dump_offsets()` and IL2CPP exports without `il2cpp_thread_attach`.

Impact:

- IL2CPP API calls from an unattached native thread can crash, assert, or deadlock depending on runtime/version.

Recommended fix:

- Attach the worker thread to the IL2CPP domain before metadata calls.
- Detach before exit.
- Stop immediately if attach fails.

### 8. `bytes_preview` Can Read Past a Valid Region or Through Guard Pages

Files:

- `src/dllmain.cpp:705`
- `src/dllmain.cpp:714`

`write_resolved_targets()` calls `VirtualQuery` on the first byte of a method address, then `bytes_preview()` reads 16 bytes. The code does not verify that all 16 bytes are inside the same committed readable region and does not reject guard pages.

Impact:

- Access violation while producing `il2cpp_resolved_targets.txt`.

Recommended fix:

- Reject `PAGE_GUARD` and `PAGE_NOACCESS`.
- Require `MEM_COMMIT`.
- Validate the whole 16-byte preview span before reading.

### 9. `Il2CppArray` Layout and Length Are Trusted Directly

Files:

- `include/il2cpp_unity_object_finder.hpp:10`
- `include/il2cpp_unity_object_finder.hpp:133`

`UnityObjectFinder` trusts a manually declared `Il2CppArray` layout and uses `array->max_length` directly for `reserve()` and iteration.

Impact:

- Huge allocation on bad length.
- Out-of-bounds reads if the array layout or method signature is wrong.
- Crash when stale RVAs return an unexpected object.

Recommended fix:

- Validate the array header and full vector span.
- Cap maximum object count.
- Prefer exported array helpers or a versioned IL2CPP layout wrapper.

## Medium Severity Findings

### 10. `ThreadAttach` Never Detaches and Attach Failure Is Ignored

Files:

- `include/il2cpp_runtime_sdk.hpp:36`
- `action_module/action_module.cpp:1350`
- `interaction_module/interaction_module.cpp:150`

`ThreadAttach` does not call `il2cpp_thread_detach` in a destructor, and callers frequently proceed without checking `attached()`.

Impact:

- Runtime thread-registration leaks.
- IL2CPP calls can proceed after attach failure.

Recommended fix:

- Make `ThreadAttach` a move-only RAII object.
- Resolve and call `il2cpp_thread_detach` in the destructor.
- Require callers to check `attached()` before any IL2CPP interaction.

### 11. Offset Resolver Can Silently Pick the Wrong Offset

Files:

- `include/il2cpp_offset_resolver.hpp:43`
- `include/il2cpp_offset_resolver.hpp:127`

Duplicate keys overwrite silently. Columns are not trimmed. `std::stoull` is not checked for full-string consumption, so malformed values can parse partially.

Impact:

- Wrong method/field chosen with no diagnostic.
- Hard-to-trace runtime calls to stale or duplicate offsets.

Recommended fix:

- Trim all parsed fields.
- Validate full parse with the consumed-index overload of `stoull`.
- Detect and report duplicates.
- Key methods by full signature or method token when available.

### 12. Direct RVA and Vtable Calls Bypass the Generated Offset Pipeline

Files:

- `action_module/action_module.cpp:373`
- `action_module/action_module.cpp:713`
- `action_module/action_module.cpp:1178`

Large parts of `action_module` use raw slots and raw RVAs like `0xA68`, `0xE266D0`, `0x8ABF40`, and `0x8AC1F0`.

Impact:

- Generated/resolved offset validation can be bypassed.
- Version drift becomes invisible.
- Review and troubleshooting are harder.

Recommended fix:

- Move raw constants into generated or hand-maintained named constants.
- Attach source metadata: type, member, signature, game build/hash.
- Validate executable addresses before calls.

### 13. File-Based IPC Is Race-Prone

Files:

- `action_controller/action_controller.cpp:74`
- `action_module/action_module.cpp:256`
- `action_module/action_module.cpp:273`

The controller truncates and writes `il2cpp_action_commands_v2.txt`, while the module polls, reads, and truncates the same file.

Impact:

- Partial command reads.
- Lost commands.
- Conflicts when multiple controllers are open.

Recommended fix:

- Write to a temp file and atomically rename.
- Add sequence IDs and acknowledgement.
- Or replace file polling with a named pipe, mutex-protected shared file, or local IPC.

### 14. Hook Backend Lifetime and Shutdown Are Not Modeled

Files:

- `interaction_module/interaction_module.cpp:89`
- `interaction_module/interaction_module.cpp:179`

`install_hooks()` creates a backend locally, then the module loops forever. A real backend such as MinHook or Detours needs stable lifetime and explicit shutdown.

Impact:

- Hooks may outlive their owner.
- No clean unload path.
- Crash risk during DLL unload or process shutdown.

Recommended fix:

- Store the hook backend at module scope.
- Add a stop/unload command or event.
- Disable hooks before unload.
- Call `shutdown()` deterministically.

### 15. Generated Headers Accept Invalid Hex Values

Files:

- `tools/generate_offsets_header.ps1:36`
- `tools/generate_action_offsets_from_enriched.ps1:61`
- `tools/generate_cpp_wrappers.ps1:149`
- `generated/action_offsets.hpp:1159`

The generators accept non-empty hex-like values and emit them directly as `uintptr_t`, including values that are not valid module-relative RVAs.

Impact:

- Bad dump data becomes callable C++ constants.
- Runtime wrappers can jump into invalid memory.

Recommended fix:

- Validate method RVAs during generation.
- Separate raw dump values from executable-callable targets.
- Emit comments or diagnostics for rejected values.
- Require a game/module image size or resolved target validation step before generating callable wrappers.

### 16. Build Outputs and Large Derived Dumps Are in the Repository

Files/directories:

- `build/`
- `generated/il2cpp_enriched_targets.csv`
- `generated/interaction_surface.md`
- `generated/interaction_targets.txt`
- `.gitignore` is missing

The repository contains `.dll`, `.exe`, `.obj`, and large generated metadata files. Some generated files include absolute local paths such as `D:\SteamLibrary\...`.

Impact:

- Repository bloat.
- Local environment disclosure.
- Non-reproducible source state.
- Harder reviews and diffs.

Recommended fix:

- Add `.gitignore`.
- Ignore `build/`, `*.obj`, `*.dll`, `*.exe`, `*.pdb`, and full generated dumps.
- Keep small sanitized fixtures/examples only.
- Publish binaries as release artifacts, not source-controlled files.

### 17. CMake Does Not Build the Whole Project

File:

- `CMakeLists.txt:5`

CMake only defines `il2cpp_runtime_offsets`, while the project also has `action_module`, `action_controller`, `interaction_module`, and `runtime_client` built through separate `.bat` files.

Impact:

- Main documented build path does not reproduce actual project outputs.
- Different compile options across targets.

Recommended fix:

- Add all DLL/EXE targets to CMake.
- Centralize compiler definitions and warning flags.
- Link `user32`, `shell32`, and `ole32` where needed.
- Add CMake presets for MSVC x64.

### 18. Batch Build Scripts Depend on Current Directory and Shell State

Files:

- `build-msvc.bat:6`
- `action_module/build-action-module.bat:7`
- `action_controller/build-action-controller.bat:7`
- `interaction_module/build-interaction-module.bat:7`
- `runtime_client/build-runtime-client.bat:7`

The `.bat` files use relative paths and assume `cl` is already available in the current shell.

Impact:

- Running scripts from another directory can fail or write outputs to unexpected places.
- Build depends on manual Visual Studio environment setup.

Recommended fix:

- Start scripts with `pushd "%~dp0"`.
- Quote paths.
- Check `where cl`.
- Check target architecture.
- Return proper exit codes.
- Prefer CMake presets for reproducibility.

### 19. PowerShell Generators Resolve Outputs From Current Working Directory

Files:

- `tools/generate_offsets_header.ps1:93`
- `tools/import_il2cpp_dump_cs.ps1:237`

Generators write through `Join-Path (Get-Location) $Output`. Running them from another directory can create outputs outside the project tree.

Impact:

- Confusing output placement.
- CI/local mismatch.

Recommended fix:

- Resolve `$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot '..')`.
- Resolve relative outputs from `$ProjectRoot`.
- Keep absolute output paths supported explicitly.

## Low Severity Findings

### 20. Off-by-One Risk in Controller Text Read

File:

- `action_controller/action_controller.cpp:55`

`std::wstring text(length, L'\0')` is passed to `GetWindowTextW(..., length + 1)`. The API may write the null terminator past the string's logical size.

Recommended fix:

- Allocate `length + 1`.
- Call `GetWindowTextW`.
- Resize to the returned character count.

### 21. Hard-Coded Local Game Directory

File:

- `action_controller/action_controller.cpp:52`

The controller defaults to `D:\SteamLibrary\steamapps\common\RussianFishing4`.

Recommended fix:

- Load last-used path from a config file or registry.
- Allow environment/config override.
- Avoid shipping personal absolute paths in defaults.

### 22. README Recommends Broad PowerShell ExecutionPolicy Bypass

File:

- `README.md:240`

The documentation uses `powershell -ExecutionPolicy Bypass`.

Recommended fix:

- Prefer `-NoProfile`.
- Document trusted local script execution.
- Use scoped policy, `Unblock-File`, or signed scripts instead of blanket `Bypass`.

## Verification Notes

Commands attempted:

- `cmake --version`
- `cl`

Result:

- `cmake` was not available in the current PowerShell session.
- `cl` was not available in the current PowerShell session.

Because of that, a fresh build could not be verified from this environment. Existing outputs were present in `build/`, but they were not treated as proof of reproducibility.

The working tree currently has no commits and all files are untracked.

No obvious real secrets such as API keys, private keys, or passwords were found. Matches for terms like `password`, `token`, or `cert` appear to be IL2CPP metadata field names in generated files, not secret values.

## Suggested Fix Order

1. Add `.gitignore`; remove `build/` and full generated dumps from source control.
2. Add RVA validation and executable-page checks in `il2cpp_runtime_sdk.hpp`.
3. Add checked read/write memory-span helpers and route field reads/writes through them.
4. Change command execution so Unity API calls happen on the Unity main thread.
5. Replace raw instance tracking with lifetime-aware handles or clear-on-destroy hooks.
6. Add attach/detach RAII and fail-closed behavior for IL2CPP thread attach.
7. Move raw RVA/vtable constants into named, versioned offset metadata.
8. Complete CMake for all targets and add CI/smoke tests for generators.
9. Replace race-prone file IPC with atomic file protocol or named pipe.
10. Fix smaller Win32/UI issues such as `GetWindowTextW` sizing and hard-coded paths.
