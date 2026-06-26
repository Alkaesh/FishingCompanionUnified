# Architecture

`FishingCompanionUnified` is split into small ownership areas so two people can work in parallel.

Detailed ownership and branch split: `docs/MODULES.md`.

## Product DLL

- `src/Core` owns the D3D11 overlay lifecycle and input hooks.
- `src/GUI` owns menu screens and user controls.
- `src/Features` owns user-facing feature logic that is independent from IL2CPP offsets.
- `src/Actions` owns runtime commands, action IPC files, fishing diagnostics, and verified in-game behavior.
- `src/generated` contains the offsets currently used by the product DLL.

## Shared IL2CPP Runtime

- `modules/il2cpp_runtime/include` contains the safer shared runtime SDK copied from the tooling project.
- The product DLL keeps compatible copies in `src/Actions` for now, because the current action layer includes headers locally. New code should prefer the shared module when possible.

## IL2CPP Tooling

- `modules/il2cpp_tooling` contains the offset dumper, action module, interaction module, runtime client, examples, and generators from the second project.
- The tooling CMake targets are optional and are disabled by default with `FC_BUILD_IL2CPP_TOOLING=OFF`.
- Large generated exports are intentionally ignored. Regenerate them locally with the scripts in `modules/il2cpp_tooling/tools`.

## External Helpers

- `external/injector` keeps the injector binaries/scripts used for local sandbox testing.
- `tools` keeps project-level automation such as injection and structured action tests.

## Current Stability Rule

The working in-game input pattern for fish fight is preserved in `src/Actions/ActionRuntime.cpp`: hold RMB, press and hold LMB, release LMB, release RMB. Do not change this path without a real sandbox verification log.
