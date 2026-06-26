# Quickstart

This guide creates a minimal SDK module that adds a new tab to the Fishing Companion menu.

## 1. Include the SDK Headers

```cpp
#include "FCSDK.h"
#include "FCSDK_UI.h"
#include "imgui.h"
```

Use the headers from:

```text
src/SDK/FCSDK.h
src/SDK/FCSDK_UI.h
third_party/imgui/imgui.h
```

## 2. Implement a Render Callback

```cpp
static void RenderMyTab(void*)
{
    void* context = FCSDK_GetImGuiContext();
    if (!context)
        return;

    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(context));

    fc::sdk::ui::Section("My Module");

    if (fc::sdk::ui::BeginCard("##my_card", ImVec2(0, 96)))
    {
        fc::sdk::ui::Metric("State", "Ready", fc::sdk::ui::Accent::Cyan);
        ImGui::TextWrapped("This UI is rendered by an external SDK module.");
    }
    fc::sdk::ui::EndCard();
}
```

## 3. Export `FCSDK_ModuleInit`

```cpp
extern "C" __declspec(dllexport) FCSDK_Bool FCSDK_ModuleInit()
{
    FCSDK_TabDesc tab{};
    tab.size = sizeof(tab);
    tab.title = "My Module";
    tab.render = &RenderMyTab;
    tab.user_data = nullptr;

    return FCSDK_RegisterTab(&tab);
}
```

## 4. Build and Drop In

Build your module as a 64-bit DLL and place it next to the host DLL:

```text
build/cmake/msvc-x64/Release/mods/MyModule.dll
```

Example MSVC command from the repository root:

```bat
cl /std:c++17 /EHsc /LD /I src\SDK /I third_party\imgui ^
  examples\ExampleMod.cpp ^
  third_party\imgui\imgui.cpp ^
  third_party\imgui\imgui_draw.cpp ^
  third_party\imgui\imgui_tables.cpp ^
  third_party\imgui\imgui_widgets.cpp ^
  /link /LIBPATH:build\cmake\msvc-x64\Release FishingCompanion.lib ^
  /OUT:build\cmake\msvc-x64\Release\mods\ExampleMod.dll
```

When Fishing Companion initializes, it scans `mods/*.dll` and calls `FCSDK_ModuleInit`.

## Notes

- The callback runs inside an active ImGui frame.
- External DLL modules that compile their own ImGui code must call `ImGui::SetCurrentContext((ImGuiContext*)FCSDK_GetImGuiContext())` before drawing.
- Use the same Dear ImGui version as the host. Check it with `FCSDK_GetImGuiVersion()`.
- Do not store ImGui pointers across frames.
- Keep module render code fast; expensive work should happen outside the render callback.
- Only use allowed data sources. The SDK is not a cheat or anti-cheat bypass framework.
