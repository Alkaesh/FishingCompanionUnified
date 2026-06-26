# AI Context

Use this file when giving the repository to an AI coding assistant.

## Project Identity

Fishing Companion is a C++17 Windows DLL overlay built with Dear ImGui, DirectX 11, and MinHook. The repo is being shaped into an SDK-style mod UI shell.

## Main Goal

Let developers create overlay modules without editing internal files. Public extension points live in `src/SDK`.

## Safe Scope

Allowed:

- ImGui UI
- settings panels
- HUD widgets
- timers
- statistics from allowed APIs, logs, save files, or user input
- module loading from a local `mods` folder

Not allowed:

- anti-cheat bypasses
- stealth mechanisms
- unauthorized memory reads/writes
- multiplayer automation that gives unfair advantage

## Important Files

- `src/SDK/FCSDK.h`: public C ABI and C++ wrappers.
- `src/SDK/FCSDK_UI.h`: public style-compatible UI helpers for external modules (separate palette from the host).
- `src/SDK/ModuleLoader.*`: runtime module loading and host-side loader diagnostics.
- `src/Actions/ActionRuntime.*`: action queue, status model, file logging, and recent UI event buffer.
- `src/GUI/Menu.cpp`: Byster-branded shell, top navigation, compact toolbar, content panel, keyboard tab nav, and per-frame settings flush.
- `src/GUI/Theme.h`: `fc::Palette` host color constants plus `fc::Color`/`fc::ColorU32` converters (single source of host colors).
- `src/GUI/Theme.cpp`: ImGui style built from the palette.
- `src/GUI/UI.h`: `fc::gui::ui` host-internal helpers (cards, status lines, search/highlight, path helpers) - the host counterpart of the public SDK UI layer.
- `src/GUI/ActionsTable.h`: data-driven action catalog (`kActions[]`) for the Actions grid and top search.
- `src/GUI/Tabs/ActionsTab.cpp`: action buttons rendered from `ActionsTable`, runtime status, search results, readiness-aware states.
- `src/GUI/Tabs/LogsTab.cpp`: unified runtime and SDK event viewer with filters, search highlight, auto-scroll, Copy, and log-folder opener.
- `src/GUI/Tabs/HealthTab.cpp`: compact diagnostics for action runtime, SDK modules, loader events, and paths.
- `src/GUI/Tabs/SdkTab.cpp`: SDK status, loaded module table, and loader event log.
- `src/Features/Settings.*`: persistent hotkeys and interface scale (`FishingCompanion_settings.json` next to the host exe).
- `src/Core/Overlay.cpp`: DX11 lifecycle, cursor handling, module loading.
- `third_party/json/nlohmann/json.hpp`: vendored nlohmann/json single-header, used only by `fc_features` for settings persistence.
- `docs/QUICKSTART.md`: how to write a module.
- `examples/ExampleMod.cpp`: sample module source.

## Coding Rules

- Keep public SDK APIs backward-compatible.
- Add new public API through `src/SDK/FCSDK.h`.
- Prefer C ABI exports for module interoperability.
- Put convenience C++ wrappers behind the C ABI.
- Do not require modules to include internal `Core` or `GUI` headers.
- External modules that compile their own ImGui code must call `ImGui::SetCurrentContext((ImGuiContext*)FCSDK_GetImGuiContext())` before rendering.
- Keep module ImGui headers/version aligned with `FCSDK_GetImGuiVersion()`.
- Keep module render callbacks fast.
- Use `FCSDK_UI.h` helpers for visual consistency.
- Keep built-in host UI labels in short English ASCII until a dedicated localization layer exists.
- Default host tabs should expose implemented behavior only; do not register placeholder tabs or controls without backing runtime.
- Build and verify with:

```powershell
cmake --preset msvc-x64
cmake --build --preset release

# On Visual Studio 18 / 2026 machines:
cmake --preset msvc-x64-vs18
cmake --build --preset release-vs18
```

- `tools/auto_inject.ps1` defaults to the VS18 `release-vs18` preset and `build/cmake/vs18-x64/Release/FishingCompanion.dll`. It should stay a local developer helper: clear errors, no stealth behavior, no binaries committed.

## Current SDK Version

`0.2.0`
