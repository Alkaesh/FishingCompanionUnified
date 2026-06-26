# IL2CPP Runtime Offsets Dumper

Small Windows DLL for authorized IL2CPP builds. When loaded into a Unity IL2CPP
process, it resolves exported `il2cpp_*` functions from `GameAssembly.dll` and
writes method RVAs, field offsets, aliases, and selected target probes to:

```text
<game folder>\il2cpp_offsets.txt
<game folder>\il2cpp_offsets_grouped.txt
<game folder>\il2cpp_class_aliases.txt
<game folder>\il2cpp_resolved_targets.txt
```

Output format:

```text
assembly;type;alias;member;kind;offset_hex
Assembly-CSharp.dll;PlayerController;PlayerController;Update;method;0x123456
Assembly-CSharp.dll;fidfbihahij;Auto_MarketID;.ctor(0);method;0x49C5F0
Assembly-CSharp.dll;fidfbihahij;Auto_MarketID;pMarketID;field;0x10
```

Columns:

```text
assembly;type;alias;member;kind;offset_hex
```

Example:

```text
Assembly-CSharp.dll;gdnidinfjmh;Auto_FishingSetId_Context;fishingSetId;field;0x10
```

Means:

```text
assembly   Assembly-CSharp.dll
type       gdnidinfjmh, original runtime name, obfuscated here
alias      Auto_FishingSetId_Context, generated readable label
member     fishingSetId
kind       field
offset     0x10 inside the object/struct
```

For methods, `offset_hex` is an RVA relative to `GameAssembly.dll`:

```text
real address = GameAssembly.dll base + method RVA
```

`il2cpp_offsets_grouped.txt` writes the same data in a more readable form:

```text
[Assembly-CSharp.dll] PlayerController
  method  Update(0)                                  RVA 0x123456
  field   health                                     offset 0x38
```

If class names look random, the game is obfuscated. The dumper can still show
real runtime offsets, but the original semantic class names are not recoverable
from IL2CPP exports alone.

## Aliases

The dumper writes:

```text
<game folder>\il2cpp_class_aliases.txt
```

Aliases are best-effort labels generated from readable fields and methods. For
example, an obfuscated class with fields `fishingSetId` and `context` may become:

```text
Auto_FishingSetId_Context
```

This is not true recovery of original source names. It is a readable resolver
layer so you can work with stable names instead of `gdnidinfjmh`.

For manual names, copy `il2cpp_aliases.example.txt` next to the game executable
as `il2cpp_aliases.txt`:

```text
Assembly-CSharp.dll;gdnidinfjmh;FishingSet
Assembly-CSharp.dll;fidfbihahij;MarketItem
```

Manual aliases override generated aliases and are also written into
`il2cpp_offsets.txt`.

## Build

Visual Studio Developer PowerShell:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

The DLL will be created at:

```text
build\cmake\msvc-x64\Release\il2cpp_runtime_offsets.dll
```

If CMake is not installed, open an x64 Visual Studio Developer Command Prompt
and run:

```cmd
build-msvc.bat
```

That direct MSVC build writes:

```text
build\il2cpp_runtime_offsets.dll
```

## Use

Load the DLL into a Windows Unity IL2CPP process that you own or are allowed to
analyze. After it runs, check `il2cpp_offsets.txt` next to the game's executable.

This project does not include an injector and does not bypass anti-cheat,
packing, DRM, or other protections.

## Filtering

By default, the dumper skips common framework assemblies:

```text
mscorlib.dll
netstandard.dll
System*.dll
Unity*.dll
Mono.*.dll
Microsoft.*.dll
```

For a tighter dump, copy `il2cpp_dump_filter.example.txt` next to the game
executable as `il2cpp_dump_filter.txt` and edit it:

```text
include_assembly=Assembly-CSharp.dll
include_type=Player*
include_member=Update*
include_kind=method
```

Supported keys:

```text
all=true
include_assembly=...
exclude_assembly=...
include_type=...
exclude_type=...
include_member=...
exclude_member=...
include_kind=method
include_kind=field
```

Wildcards `*` and `?` are supported.

## Resolver

`include/il2cpp_offset_resolver.hpp` can read the generated
`il2cpp_offsets.txt`:

```cpp
#include "include/il2cpp_offset_resolver.hpp"

il2cpp_offsets::Resolver resolver;
resolver.load(L"il2cpp_offsets.txt");

auto rva = resolver.method_rva(
    "Assembly-CSharp.dll",
    "Weapon",
    "Fire(1)");

// New dumps can also resolve through the alias column:
auto aliased = resolver.field_offset(
    "Assembly-CSharp.dll",
    "Auto_FishingSetId_Context",
    "fishingSetId");

auto field = resolver.field_offset(
    "Assembly-CSharp.dll",
    "PlayerController",
    "health");

auto address = resolver.method_address(
    GetModuleHandleW(L"GameAssembly.dll"),
    "Assembly-CSharp.dll",
    "Weapon",
    "Fire(1)");
```

## Runtime Client

The main DLL also resolves selected targets during the same injection. It does
not call arbitrary game functions by default. This is intentional: calling an
IL2CPP method safely requires the exact native signature and, for instance
methods, a valid object pointer.

Usage:

1. Copy `il2cpp_targets.example.txt` next to the game executable as
   `il2cpp_targets.txt`.
2. Edit targets:

```text
Assembly-CSharp.dll;RF4.Client.Water.InteractionController;Update(0);method
Assembly-CSharp.dll;RF4.Client.Water.InteractionController;followWaterFlow;field
```

3. Load `il2cpp_runtime_offsets.dll`.
4. Read:

```text
<game folder>\il2cpp_resolved_targets.txt
```

The result contains method RVAs, absolute method addresses, memory protection,
first bytes, and field offsets. After a target is resolved, add a typed wrapper
only when the method signature and object lifetime are known.

`runtime_client` is kept as a smaller resolver-only helper, but the recommended
workflow is the single main DLL above.

## Finding Targets

If you do not know what to put into `il2cpp_targets.txt`, search the dump by
keywords first:

```powershell
powershell -NoProfile -File .\tools\find_il2cpp_targets.ps1 `
  -Offsets "D:\SteamLibrary\steamapps\common\RussianFishing4\il2cpp_offsets.txt" `
  -Keywords "Fishing,Player,Inventory,Item,Water,Weather,UI,Rig,Rod,Bait,Boat,Map" `
  -Output "generated\candidate_targets.txt"
```

If Windows blocks a trusted local copy of the script, unblock that file explicitly:

```powershell
Unblock-File .\tools\find_il2cpp_targets.ps1
```

Open `generated\candidate_targets.txt`, pick interesting non-comment lines, and
copy them into the game's `il2cpp_targets.txt`.

Good first targets are usually readable classes with lifecycle methods:

```text
RF4.Client.FishingScene.FishingSet
RF4.Client.FishingScene.Fisher
RF4.Client.FishingScene.Reel
RF4.Client.FishingScene.Rod
RF4.Client.FishingScene.Rig
RF4.Client.UIController
RF4.Client.Weather.WeatherManager
```

Prefer `Start(0)`, `Update(0)`, `OnDestroy(0)`, readable fields, and small
methods while exploring. Obfuscated method names with parameters are harder to
call safely without more type information.

## Broad Interaction Surface

To build a broad catalog of everything likely useful later:

```powershell
powershell -NoProfile -File .\tools\build_interaction_surface.ps1 `
  -Offsets "D:\SteamLibrary\steamapps\common\RussianFishing4\il2cpp_offsets.txt" `
  -CatalogOutput "generated\interaction_surface.md" `
  -TargetsOutput "generated\interaction_targets.txt"
```

Outputs:

```text
generated\interaction_surface.md
generated\interaction_targets.txt
generated\interaction_presets\Fishing.txt
generated\interaction_presets\InventoryItems.txt
generated\interaction_presets\PlayerInput.txt
generated\interaction_presets\UI.txt
generated\interaction_presets\WorldWaterWeather.txt
```

Use presets instead of resolving everything at once. For example, copy
`generated\interaction_presets\Fishing.txt` next to the game executable as
`il2cpp_targets.txt`, inject the dumper, then generate a C++ header from the
new `il2cpp_resolved_targets.txt`.

## Importing Dump.cs Metadata

If you have a full dump folder with `il2cpp_dump_deobf.cs`, import signatures
and field types:

```powershell
powershell -NoProfile -File .\tools\import_il2cpp_dump_cs.ps1 `
  -DumpCs "C:\Users\alga\Downloads\Telegram Desktop\dump\il2cpp_dump_deobf.cs" `
  -Offsets "D:\SteamLibrary\steamapps\common\RussianFishing4\il2cpp_offsets.txt" `
  -OutputCsv "generated\il2cpp_enriched_targets.csv" `
  -OutputTargets "generated\il2cpp_enriched_targets.txt"
```

Outputs:

```text
generated\il2cpp_enriched_targets.csv
generated\il2cpp_enriched_targets.txt
```

The CSV contains:

```text
Type, Parent, InstanceSize, Kind, Member, Access, IsStatic, ReturnType,
FieldType, Params, OffsetHex, RvaHex, Signature
```

This is the file to use when deciding whether a method is static/instance, what
arguments it takes, and what field type a field has.

## Generated SDK Layer

After `il2cpp_resolved_targets.txt` exists, generate a C++ offsets header:

```powershell
powershell -NoProfile -File .\tools\generate_offsets_header.ps1 `
  -ResolvedTargets "D:\SteamLibrary\steamapps\common\RussianFishing4\il2cpp_resolved_targets.txt" `
  -Output "generated\rf4_offsets.hpp" `
  -Namespace "rf4_offsets"
```

This creates constants like:

```cpp
inline constexpr uintptr_t RF4_Client_Water_InteractionController_Update_method = 0x4A6FC0;
inline constexpr uintptr_t RF4_Client_Water_InteractionController_followWaterFlow_field = 0x20;
```

Use them with `include/il2cpp_runtime_sdk.hpp`:

```cpp
#include "generated/rf4_offsets.hpp"
#include "include/il2cpp_runtime_sdk.hpp"

void example(void* interactionController) {
    il2cpp_runtime::Field<bool> followWaterFlow(
        rf4_offsets::RF4_Client_Water_InteractionController_followWaterFlow_field);

    auto value = followWaterFlow.read(interactionController);

    il2cpp_runtime::ThreadAttach attach;
    il2cpp_runtime::InstanceMethod<void> update(
        rf4_offsets::RF4_Client_Water_InteractionController_Update_method);

    update.call(interactionController);
}
```

`examples/usage_example.cpp` contains a compilable example. It assumes you
already have a valid object pointer. Finding object instances and deciding when
to call methods is separate work and should be done only in authorized/debug
contexts.

## Capturing `this`

For your own authorized IL2CPP project, get `this` by hooking an instance
method that naturally runs while the object is alive, usually:

```text
Awake(0)
Start(0)
Update(0)
OnEnable(0)
OnDestroy(0)
```

Native IL2CPP instance methods receive the object pointer as the first
argument:

```cpp
using UpdateFn = void (*)(void* self);

UpdateFn original_update = nullptr;

void hk_update(void* self) {
    il2cpp_runtime::InstanceTracker::get().capture("MyController", self);
    original_update(self);
}
```

After that, any action code can retrieve the object:

```cpp
void* self = il2cpp_runtime::InstanceTracker::get().instance("MyController");
```

Files:

```text
include\il2cpp_instance_tracker.hpp
examples\instance_capture_example.cpp
interaction_module\interaction_module.cpp
```

`interaction_module` is a template DLL for the future action layer. It leaves
hook installation behind `install_hooks()` so you can plug in an authorized hook
backend such as MinHook or Detours in your own project.

## Connected Interaction Module

The SDK pieces are now connected:

```text
generated\rf4_offsets.hpp
        -> interaction_module\project_offsets.hpp
        -> include\il2cpp_interaction_sdk.hpp
        -> include\il2cpp_runtime_sdk.hpp
        -> include\il2cpp_instance_tracker.hpp
        -> interaction_module\interaction_module.cpp
```

Build:

```cmd
cd interaction_module
build-interaction-module.bat
```

Output:

```text
build\il2cpp_interaction_module.dll
```

To use it with your own generated offsets:

1. Generate a header from `il2cpp_resolved_targets.txt`.
2. Edit `interaction_module\project_offsets.hpp`:

```cpp
#include "../generated/your_offsets.hpp"
namespace project_offsets = your_offsets;
```

3. In `interaction_module.cpp`, pick lifecycle targets from
   `project_offsets::targets`.
4. Replace `NoopHookBackend` with a real authorized hook backend.
5. In detours, capture `self`:

```cpp
g_context.capture("MyController", self);
```

6. In `tick_actions()`, use the captured object with `Field<T>` and
   `InstanceMethod<>`.

`examples\hook_backend_skeleton.cpp` shows the exact adapter boundary for
MinHook/Detours-style libraries.

## Notes

- Method offsets are RVAs relative to `GameAssembly.dll`.
- Field offsets are object/struct field offsets returned by
  `il2cpp_field_get_offset`.
- Some Unity versions change internal `MethodInfo` layout. This dumper reads
  the first pointer as `methodPointer`, which is correct for common IL2CPP
  builds but may need adjustment for unusual versions.
