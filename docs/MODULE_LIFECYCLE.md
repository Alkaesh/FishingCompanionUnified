# Module Lifecycle

Fishing Companion loads SDK modules after the ImGui context and built-in tabs are ready.

## Load Order

1. `Overlay::InitImGuiOnce`
2. `Theme::LoadFonts`
3. `Theme::Apply`
4. ImGui Win32/DX11 backend initialization
5. Built-in tab registration
6. `ModuleLoader::LoadAll`
7. Each `mods/*.dll` receives `FCSDK_ModuleInit`
8. Tabs registered during a successful init are committed to the menu

## Module Folder

The loader scans:

```text
<directory of FishingCompanion.dll>/mods/*.dll
```

If the folder does not exist, the host creates it.

## Successful Module

A module is considered loaded when:

- the DLL loads with `LoadLibraryW`
- it exports `FCSDK_ModuleInit`
- `FCSDK_ModuleInit()` returns `FCSDK_TRUE`

The module handle is retained until host shutdown.
Tabs registered during `FCSDK_ModuleInit` belong to that module handle and are removed automatically before the DLL unloads.

## Failed Module

A module is marked failed when:

- the DLL cannot be loaded
- `FCSDK_ModuleInit` is missing
- `FCSDK_ModuleInit()` returns `FCSDK_FALSE`

The SDK tab lists loaded and failed modules.

## Unload

During overlay shutdown, the loader removes module-owned tabs, calls optional `FCSDK_ModuleShutdown`, then calls `FreeLibrary` for every loaded module.

If a module owns resources, release them in `FCSDK_ModuleShutdown`. Do not keep ImGui pointers or host callbacks past shutdown.

## Scope and Safety

SDK modules should be used for:

- UI panels
- overlay HUD widgets
- settings
- timers
- allowed telemetry/log/save-file integrations

SDK modules should not implement:

- anti-cheat bypasses
- stealth injection
- unauthorized memory access
- multiplayer advantage automation
