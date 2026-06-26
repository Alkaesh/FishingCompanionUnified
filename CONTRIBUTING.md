# Development Workflow

This repository is shared by two developers plus Codex. Keep updates small and tested.

## Branches

- Use one branch per micro-feature: `feature/<short-name>` or `fix/<short-name>`.
- Pick the module owner area from `docs/MODULES.md` before starting work.
- Pull the latest shared branch before starting work.
- Do not mix unrelated reverse-engineering experiments, UI changes, and runtime fixes in one branch.

## Commit Rule

Push to GitHub only when the micro-feature is complete, tested, and has a concrete result.

A commit/PR should state:

- What changed.
- Which sandbox build/DLL was tested.
- Which command or log file proves the behavior.
- Any known risk or follow-up.

## Testing

Minimum local checks before push:

```powershell
cmake --preset msvc-x64
cmake --build --preset release
.\tools\run_action_test.ps1 -Command snapshot -WaitSeconds 1
.\tools\run_action_test.ps1 -Summary
```

If the machine only has Visual Studio 18 / 2026, use:

```powershell
cmake --preset msvc-x64-vs18
cmake --build --preset release-vs18
```

Use screenshots only as backup evidence. Prefer structured logs in `test_logs/` for fast repeat checks.

## Generated Files

Do not commit local build outputs, IDA databases, runtime logs, or heavy IL2CPP generated exports. Keep source scripts and small curated offset headers in Git.
