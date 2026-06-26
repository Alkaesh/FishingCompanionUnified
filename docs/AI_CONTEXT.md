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
- `src/SDK/FCSDK_UI.h`: style-compatible UI helpers.
- `src/SDK/ModuleLoader.*`: runtime module loading.
- `src/GUI/Menu.cpp`: main shell, header, left navigation, content area.
- `src/GUI/Theme.cpp`: visual system and ImGui style.
- `src/Core/Overlay.cpp`: DX11 lifecycle, cursor handling, module loading.
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
- Build and verify with:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build --config Release
```

## Current SDK Version

`0.2.0`
