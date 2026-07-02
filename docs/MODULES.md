# Module Map

This project is now split into build modules and ownership areas. Use this file when choosing parallel work so branches do not touch the same files without a reason.

## Build Modules

| CMake target | Source area | Owner focus | Public boundary |
| --- | --- | --- | --- |
| `fc_entry` | `src/dllmain.cpp` | DLL attach/detach and main worker thread | `DllMain`, `MainThread` |
| `fc_core` | `src/Core` | DX11 overlay hooks, input routing, shutdown lifecycle | `fc::Overlay`, `fc::Input` |
| `fc_actions` | `src/Actions` | Runtime commands, action queue, diagnostics, sensor snapshots | `fc::actions::Start/Stop/Queue/GetStatus` |
| `fc_features` | `src/Features` | UI-independent app features: key binding and persistent settings | `fc::KeyBinder`, `fc::Settings` |
| `fc_gui` | `src/GUI` | Menu shell, built-in tabs, shared host UI helpers and palette | `fc::Menu`, `fc::ITab`, `fc::gui::ui`, `fc::Palette` |
| `fc_sdk` | `src/SDK` | External module ABI and lifecycle | `FCSDK_*`, `fc::sdk::ModuleLoader` |
| `fc_third_party` | `third_party/imgui`, `third_party/minhook`, `third_party/json` | Vendored UI/hook/JSON dependencies | Do not edit unless upgrading vendor code |
| `FishingCompanion` | composed DLL | Product DLL | links all modules |

## Non-DLL Modules

| Area | Purpose |
| --- | --- |
| `modules/il2cpp_runtime` | Shared safe IL2CPP helpers for future common runtime code. |
| `modules/il2cpp_tooling` | Standalone offset/action/interaction discovery tools. |
| `src/generated` | Curated generated headers used by the product DLL. |
| `tools` | Injection, smoke tests, and structured log verification. |
| `docs` | Architecture, SDK, and workflow docs. |

## Parallel Work Tracks

1. Runtime behavior track
   - Main files: `src/Actions`, `src/generated`, `tools/run_action_test.ps1`.
   - Good tasks: command verification, diagnostics, sensor paths, safer IL2CPP calls.
   - Required proof: structured action test log or coordinate CSV diff.

2. UI and workflow track
   - Main files: `src/GUI`, `src/Features`.
   - Good tasks: tabs, status views, key binding UX, log viewers.
   - Required proof: build plus screenshot only when layout changed.

3. SDK/module ecosystem track
   - Main files: `src/SDK`, `examples`, `docs/API_REFERENCE.md`, `docs/MODULE_LIFECYCLE.md`.
   - Good tasks: external module samples, SDK helpers, lifecycle compatibility.
   - Required proof: example module builds and loads from `mods`.

4. Core stability track
   - Main files: `src/Core`, `src/dllmain.cpp`.
   - Good tasks: hook lifecycle, unload reliability, DX11 reset handling.
   - Required proof: unload/reload with `tools/auto_inject.ps1`.

5. Tooling/research track
   - Main files: `modules/il2cpp_tooling`, `modules/il2cpp_runtime`.
   - Good tasks: target discovery, offset generation, standalone probes.
   - Required proof: tooling build plus generated output path check.

## Dependency Rules

- `fc_entry` may start and stop `fc_core` and `fc_actions`.
- `fc_core` may own overlay lifecycle and load `fc_sdk` modules.
- `fc_gui` may read `fc_actions` status and queue commands, but should not scan IL2CPP directly.
- `fc_actions` owns runtime/game-facing behavior. Keep UI code out of this module except the current menu-hide call through `fc::Overlay`.
- `fc_sdk` must preserve C ABI compatibility. Add exports carefully and document them.
- `modules/il2cpp_tooling` can experiment freely; only move small verified pieces into `src`.

## Branch Split Suggestion

- Developer A: `feature/runtime-<name>` for `fc_actions` and generated offsets.
- Developer B: `feature/ui-<name>` for `fc_gui` and `fc_features`.
- Codex: `fix/core-<name>` or `fix/sdk-<name>` for lifecycle and ABI hardening.

Do not push a branch until its module-level proof is attached in the PR or commit notes.
