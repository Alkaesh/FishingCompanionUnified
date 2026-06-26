# FishingCompanionUnified

Единая рабочая база для DLL-оверлея, action runtime, IL2CPP SDK и вспомогательного tooling.

Проект собран из двух веток работы:

- `src/` - рабочая версия меню/оверлея и action runtime, уже проверенная в sandbox.
- `modules/il2cpp_runtime/` - общий безопасный слой для IL2CPP: проверка RVA, проверка страниц памяти, RAII attach/detach IL2CPP thread.
- `modules/il2cpp_tooling/` - offset dumper, action/interaction modules, runtime client, генераторы и примеры из второго проекта.
- `tools/` - локальная автоматизация инжекта и структурированных action-тестов.
- `external/injector/` - локальный injector для sandbox-проверок.

## Build

```powershell
cmake --preset msvc-x64
cmake --build --preset release
```

Основной результат:

```text
D:\FishingCompanionUnified\build\cmake\msvc-x64\Release\FishingCompanion.dll
```

Опционально собрать tooling:

```powershell
cmake --preset msvc-x64-tooling
cmake --build --preset release-tooling
```

## Test

После инжекта DLL в sandbox-процесс:

```powershell
.\tools\run_action_test.ps1 -Command snapshot -WaitSeconds 1
.\tools\run_action_test.ps1 -Summary
```

Логи пишутся в `test_logs/`. Скриншоты используем только как дополнительную проверку, основной быстрый контроль - JSONL/text logs.

## Architecture

Подробно: `docs/architecture.md` и `docs/MODULES.md`.

Коротко:

- UI/overlay не должны напрямую разрастаться IL2CPP-логикой.
- Runtime-команды и диагностика живут в `src/Actions`.
- Новые reverse-engineering эксперименты сначала идут в `modules/il2cpp_tooling`.
- После проверки в sandbox переносим минимальный стабильный кусок в `src/Actions` или общий `modules/il2cpp_runtime`.
- `il2cpp_thread_detach` выключен по умолчанию (`FC_IL2CPP_THREAD_DETACH_ON_DESTROY=0`), потому что в текущей sandbox-сборке он привел к crash в `GameAssembly.dll`.

## Git Rule

Пушим в GitHub только готовую и протестированную микро-фичу. Подробно: `CONTRIBUTING.md`.
