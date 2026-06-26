# Fishing Companion SDK

Fishing Companion SDK is a small C++17/ImGui extension layer for building overlay modules on top of the Fishing Companion DX11 shell.

The SDK is intentionally scoped to UI, overlays, settings panels, timers, session dashboards, and data from allowed sources such as game APIs, local logs, save files, or user-provided data. It does not provide memory scanning, anti-cheat bypasses, stealth injection helpers, or competitive automation.

## Design Goals

- **Stable public surface:** modules use `src/SDK/FCSDK.h` instead of touching internal `GUI` or `Core` files.
- **Drop-in module workflow:** place a module DLL into `mods/`; the host calls `FCSDK_ModuleInit`.
- **ImGui-native rendering:** module render callbacks run during the active ImGui frame.
- **Consistent visual system:** use `src/SDK/FCSDK_UI.h` for cards, sections, metrics, and accent colors.
- **AI-readable docs:** `llms.txt` and `docs/AI_CONTEXT.md` summarize the repo for coding assistants.

## Repository Map

```text
src/
  Core/                 DX11 hook, lifecycle, input, cursor handling
  GUI/                  built-in shell, theme, tabs
  SDK/                  public SDK headers, exports, module loader
docs/                   human and LLM friendly documentation
examples/               example SDK module source
mods/                   runtime drop-in folder created next to the built DLL
```

## Public Headers

- `src/SDK/FCSDK.h` - C ABI, versioning, tab registration, menu visibility.
- `src/SDK/FCSDK_UI.h` - style-compatible ImGui helpers for modules.
- `FCSDK_GetImGuiContext()` - host ImGui context for external module rendering.

## Runtime Module Flow

1. The host initializes ImGui and registers built-in tabs.
2. The host scans `mods/*.dll` next to `FishingCompanion.dll`.
3. For each DLL, the host looks up `FCSDK_ModuleInit`.
4. The module calls `FCSDK_RegisterTab`.
5. The tab render callback is called inside the host's ImGui frame.
6. External DLL modules set the host ImGui context before drawing if they compile their own ImGui code.

## Current Version

`0.2.0`
