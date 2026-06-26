// ============================================================================
//  FCSDK.h - public C ABI for Fishing Companion SDK modules.
// ============================================================================

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#   if defined(FCSDK_BUILD)
#       define FCSDK_API __declspec(dllexport)
#   else
#       define FCSDK_API __declspec(dllimport)
#   endif
#else
#   define FCSDK_API
#endif

#define FCSDK_VERSION_MAJOR 0
#define FCSDK_VERSION_MINOR 2
#define FCSDK_VERSION_PATCH 0

typedef int FCSDK_Bool;

enum
{
    FCSDK_FALSE = 0,
    FCSDK_TRUE = 1
};

typedef void (*FCSDK_RenderCallback)(void* user_data);
typedef FCSDK_Bool (*FCSDK_ModuleInitFn)(void);
typedef void (*FCSDK_ModuleShutdownFn)(void);

#define FCSDK_MODULE_INIT_NAME "FCSDK_ModuleInit"
#define FCSDK_MODULE_SHUTDOWN_NAME "FCSDK_ModuleShutdown"

typedef struct FCSDK_TabDesc
{
    uint32_t size;
    const char* title;
    FCSDK_RenderCallback render;
    void* user_data;
} FCSDK_TabDesc;

FCSDK_API uint32_t FCSDK_GetVersion(void);
FCSDK_API const char* FCSDK_GetVersionString(void);
FCSDK_API void* FCSDK_GetImGuiContext(void);
FCSDK_API const char* FCSDK_GetImGuiVersion(void);
FCSDK_API FCSDK_Bool FCSDK_RegisterTab(const FCSDK_TabDesc* desc);
FCSDK_API void FCSDK_SetMenuVisible(FCSDK_Bool visible);
FCSDK_API FCSDK_Bool FCSDK_IsMenuVisible(void);
FCSDK_API void FCSDK_RequestShutdown(void);

#ifdef __cplusplus
}

namespace fc::sdk {

struct TabDesc
{
    const char* title = nullptr;
    FCSDK_RenderCallback render = nullptr;
    void* userData = nullptr;
};

constexpr uint32_t Version()
{
    return (FCSDK_VERSION_MAJOR << 16) | (FCSDK_VERSION_MINOR << 8) | FCSDK_VERSION_PATCH;
}

inline bool RegisterTab(const TabDesc& tab)
{
    FCSDK_TabDesc desc{};
    desc.size = sizeof(desc);
    desc.title = tab.title;
    desc.render = tab.render;
    desc.user_data = tab.userData;
    return FCSDK_RegisterTab(&desc) == FCSDK_TRUE;
}

inline void* GetImGuiContext()
{
    return FCSDK_GetImGuiContext();
}

inline const char* GetImGuiVersion()
{
    return FCSDK_GetImGuiVersion();
}

inline void SetMenuVisible(bool visible)
{
    FCSDK_SetMenuVisible(visible ? FCSDK_TRUE : FCSDK_FALSE);
}

inline bool IsMenuVisible()
{
    return FCSDK_IsMenuVisible() == FCSDK_TRUE;
}

inline void RequestShutdown()
{
    FCSDK_RequestShutdown();
}

} // namespace fc::sdk
#endif
