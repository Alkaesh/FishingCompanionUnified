# API Reference

Public SDK header: `src/SDK/FCSDK.h`

## Version

```cpp
uint32_t FCSDK_GetVersion(void);
const char* FCSDK_GetVersionString(void);
```

`FCSDK_GetVersion` returns a packed integer:

```text
(major << 16) | (minor << 8) | patch
```

Current version: `0.2.0`.

## Boolean Type

```cpp
typedef int FCSDK_Bool;
```

Use:

```cpp
FCSDK_TRUE
FCSDK_FALSE
```

## Render Callback

```cpp
typedef void (*FCSDK_RenderCallback)(void* user_data);
```

The host calls this function while rendering the selected tab. The callback can use ImGui directly.

## Tab Descriptor

```cpp
typedef struct FCSDK_TabDesc
{
    uint32_t size;
    const char* title;
    FCSDK_RenderCallback render;
    void* user_data;
} FCSDK_TabDesc;
```

Fields:

- `size`: must be `sizeof(FCSDK_TabDesc)`.
- `title`: UTF-8 tab label.
- `render`: callback called every visible frame.
- `user_data`: optional pointer passed back to `render`.

## Register a Tab

```cpp
FCSDK_Bool FCSDK_RegisterTab(const FCSDK_TabDesc* desc);
```

Returns `FCSDK_TRUE` if the tab was accepted.

Validation rules:

- `desc` must not be null.
- `desc->size >= sizeof(FCSDK_TabDesc)`.
- `desc->title` must not be null or empty.
- `desc->render` must not be null.

## Menu Visibility

```cpp
void FCSDK_SetMenuVisible(FCSDK_Bool visible);
FCSDK_Bool FCSDK_IsMenuVisible(void);
void FCSDK_RequestShutdown(void);
```

Use visibility calls only for UI modules that need to open or close the SDK shell.
Use `FCSDK_RequestShutdown` when an injector or module wants the host to unload through its normal shutdown path.

## Module Entry Point

```cpp
#define FCSDK_MODULE_INIT_NAME "FCSDK_ModuleInit"
#define FCSDK_MODULE_SHUTDOWN_NAME "FCSDK_ModuleShutdown"
typedef FCSDK_Bool (*FCSDK_ModuleInitFn)(void);
typedef void (*FCSDK_ModuleShutdownFn)(void);
```

External module DLLs must export:

```cpp
extern "C" __declspec(dllexport) FCSDK_Bool FCSDK_ModuleInit();
```

The host calls this function after ImGui has been initialized and built-in tabs are registered. Modules may optionally export `FCSDK_ModuleShutdown`; the host calls it before unloading that module.

## C++ Helpers

Inside C++ code, use namespace wrappers:

```cpp
fc::sdk::RegisterTab({
    .title = "Module",
    .render = &RenderModule,
    .userData = nullptr,
});

fc::sdk::SetMenuVisible(true);
bool visible = fc::sdk::IsMenuVisible();
fc::sdk::RequestShutdown();
```

These helpers are header-only wrappers around the C ABI.

## ImGui Context

```cpp
void* FCSDK_GetImGuiContext(void);
const char* FCSDK_GetImGuiVersion(void);
```

External modules that compile or link their own Dear ImGui code should set the host context before drawing:

```cpp
void* context = FCSDK_GetImGuiContext();
if (context)
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(context));
```

Use the same Dear ImGui version as the host. The host version is available through `FCSDK_GetImGuiVersion()`.
