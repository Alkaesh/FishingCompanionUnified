# FishingCompanionUnified Status

Unified project root:

```text
D:\FishingCompanionUnified
```

Source inputs:

```text
D:\FishingCompanion
C:\Users\тимур\Downloads\Telegram Desktop\New project (2)\New project
```

Current layout:

```text
src                         product DLL, overlay, UI, action runtime
modules\il2cpp_runtime      shared safe IL2CPP runtime headers
modules\il2cpp_tooling      offset dumper, action module, interaction tooling
tools                       injection and structured action tests
external\injector           local sandbox injector
docs                        architecture and workflow docs
```

Build:

```powershell
cmake --preset msvc-x64
cmake --build --preset release
```

Structured smoke test:

```powershell
.\tools\run_action_test.ps1 -Command snapshot -WaitSeconds 1
.\tools\run_action_test.ps1 -Summary
```

Known stable runtime behavior preserved:

- Fish-fight input path: RMB down, LMB down, hold, LMB up, RMB up.
- The safer IL2CPP runtime SDK is now present in both `modules\il2cpp_runtime\include` and compatible `src\Actions` headers.
- `il2cpp_thread_detach` is opt-in only (`FC_IL2CPP_THREAD_DETACH_ON_DESTROY=0` by default) after a sandbox crash at `GameAssembly.dll+0x2AF16C`.

Latest verification:

- `cmake --build --preset release`: pass.
- `cmake --build --preset release-tooling`: pass.
- Injected `D:\FishingCompanionUnified\build\cmake\msvc-x64\Release\FishingCompanion.dll`: pass, SHA256 `C0B9BE8741DEF82153A7A03C07188F59ADA5D2B03E97C62ACD30185C6A2327A6`.
- `tools\auto_inject.ps1 -TargetPid 7800`: pass after modular CMake split.
- `run_action_test.ps1 -Command snapshot -WaitSeconds 1`: pass at `2026-06-26T10:05:42+03:00`.

GitHub readiness:

- Build outputs, logs, IDA databases, screenshots, and heavy generated IL2CPP exports are ignored.
- Use `feature/<name>` or `fix/<name>` branches.
- Push only complete tested micro-features.
