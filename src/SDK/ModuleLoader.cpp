// ============================================================================
//  ModuleLoader.cpp - runtime loader for SDK modules.
// ============================================================================

#include "ModuleLoader.h"

#include "FCSDK.h"
#include "../GUI/Menu.h"

#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

namespace {

constexpr size_t kMaxLoaderEvents = 80;

fs::path GetHostDirectory(HMODULE hostModule)
{
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(hostModule, buffer, MAX_PATH);
    if (length == 0)
        return fs::current_path();

    return fs::path(buffer).parent_path();
}

std::string Narrow(const std::wstring& value)
{
    if (value.empty())
        return {};

    const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1)
        return {};

    std::string result(static_cast<size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), size, nullptr, nullptr);
    return result;
}

std::string TrimSystemMessage(std::string value)
{
    while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == ' '))
        value.pop_back();
    return value;
}

std::string Win32ErrorMessage(DWORD error)
{
    char* buffer = nullptr;
    const DWORD length = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&buffer),
        0,
        nullptr);

    std::string message;
    if (length > 0 && buffer)
        message.assign(buffer, length);
    if (buffer)
        LocalFree(buffer);

    if (message.empty())
        message = "unknown error";

    std::ostringstream out;
    out << "Win32 " << error << ": " << TrimSystemMessage(message);
    return out.str();
}

std::string HexException(DWORD code)
{
    std::ostringstream out;
    out << "0x" << std::hex << std::uppercase << code;
    return out.str();
}

bool CallModuleInitGuarded(FCSDK_ModuleInitFn init, FCSDK_Bool* result, DWORD* exceptionCode)
{
    if (result)
        *result = FCSDK_FALSE;
    if (exceptionCode)
        *exceptionCode = 0;

    __try {
        if (result)
            *result = init();
        else
            init();
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (exceptionCode)
            *exceptionCode = GetExceptionCode();
        return false;
    }
}

bool CallModuleShutdownGuarded(FCSDK_ModuleShutdownFn shutdown, DWORD* exceptionCode)
{
    if (exceptionCode)
        *exceptionCode = 0;

    __try {
        shutdown();
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (exceptionCode)
            *exceptionCode = GetExceptionCode();
        return false;
    }
}

} // namespace

namespace fc::sdk {

ModuleLoader& ModuleLoader::Get()
{
    static ModuleLoader instance;
    return instance;
}

void ModuleLoader::BeginModuleInit(HMODULE module)
{
    m_loadingModule = module;
    m_loadingThread = std::this_thread::get_id();
    m_pendingTabs.clear();
}

void ModuleLoader::CommitModuleInit(HMODULE module)
{
    for (std::unique_ptr<fc::ITab>& tab : m_pendingTabs)
        fc::Menu::Get().AddTab(std::move(tab), module);

    m_pendingTabs.clear();
    m_loadingModule = nullptr;
    m_loadingThread = {};
}

void ModuleLoader::RollbackModuleInit()
{
    m_pendingTabs.clear();
    m_loadingModule = nullptr;
    m_loadingThread = {};
}

bool ModuleLoader::RegisterTab(std::unique_ptr<fc::ITab> tab)
{
    if (!tab)
    {
        RecordEvent("warn", "FCSDK_RegisterTab rejected an empty tab.");
        return false;
    }

    if (!m_loadingModule || m_loadingThread != std::this_thread::get_id())
    {
        RecordEvent("warn", "FCSDK_RegisterTab was called outside FCSDK_ModuleInit.");
        return false;
    }

    m_pendingTabs.push_back(std::move(tab));
    return true;
}

void ModuleLoader::RecordEvent(std::string level, std::string message)
{
    m_events.push_back(Event{std::move(level), std::move(message)});
    if (m_events.size() > kMaxLoaderEvents)
        m_events.erase(m_events.begin(), m_events.begin() + (m_events.size() - kMaxLoaderEvents));
}

void ModuleLoader::RecordModule(std::wstring name, bool loaded, std::string detail)
{
    m_moduleRecords.push_back(ModuleRecord{std::move(name), loaded, std::move(detail)});
}

void ModuleLoader::LoadAll(HMODULE hostModule)
{
    if (m_scanned)
        return;

    m_scanned = true;
    m_moduleRecords.clear();
    m_events.clear();
    m_loadedModuleNames.clear();
    m_failedModuleNames.clear();

    const fs::path modsPath = GetHostDirectory(hostModule) / L"mods";
    m_modsDirectory = modsPath.wstring();
    RecordEvent("info", "Scanning SDK modules in " + Narrow(m_modsDirectory));

    std::error_code ec;
    fs::create_directories(modsPath, ec);
    if (ec || !fs::exists(modsPath))
    {
        RecordEvent("error", "Unable to prepare mods directory: " + ec.message());
        return;
    }

    for (const fs::directory_entry& entry : fs::directory_iterator(modsPath, ec))
    {
        if (ec)
            break;

        std::error_code entryEc;
        if (!entry.is_regular_file(entryEc))
        {
            if (entryEc)
                RecordEvent("warn", "Skipping a mods entry: " + entryEc.message());
            continue;
        }

        if (entry.path().extension() != L".dll")
            continue;

        const std::wstring moduleName = entry.path().filename().wstring();
        HMODULE module = LoadLibraryW(entry.path().c_str());
        if (!module)
        {
            const std::string detail = "LoadLibraryW failed: " + Win32ErrorMessage(GetLastError());
            RecordModule(moduleName, false, detail);
            RecordEvent("error", Narrow(moduleName) + ": " + detail);
            m_failedModuleNames.push_back(moduleName);
            continue;
        }

        auto init = reinterpret_cast<FCSDK_ModuleInitFn>(GetProcAddress(module, FCSDK_MODULE_INIT_NAME));
        auto shutdown = reinterpret_cast<FCSDK_ModuleShutdownFn>(
            GetProcAddress(module, FCSDK_MODULE_SHUTDOWN_NAME));

        if (!init)
        {
            FreeLibrary(module);
            const std::string detail = "missing " FCSDK_MODULE_INIT_NAME " export";
            RecordModule(moduleName, false, detail);
            RecordEvent("error", Narrow(moduleName) + ": " + detail);
            m_failedModuleNames.push_back(moduleName);
            continue;
        }

        BeginModuleInit(module);
        FCSDK_Bool initResult = FCSDK_FALSE;
        DWORD exceptionCode = 0;
        const bool initReturned = CallModuleInitGuarded(init, &initResult, &exceptionCode);
        if (!initReturned || initResult != FCSDK_TRUE)
        {
            RollbackModuleInit();
            FreeLibrary(module);

            const std::string detail = initReturned
                ? std::string(FCSDK_MODULE_INIT_NAME) + " returned false"
                : std::string(FCSDK_MODULE_INIT_NAME) + " raised " + HexException(exceptionCode);
            RecordModule(moduleName, false, detail);
            RecordEvent("error", Narrow(moduleName) + ": " + detail);
            m_failedModuleNames.push_back(moduleName);
            continue;
        }

        const size_t registeredTabs = m_pendingTabs.size();
        CommitModuleInit(module);
        m_modules.push_back(LoadedModule{module, shutdown, moduleName});
        m_loadedModuleNames.push_back(moduleName);
        const std::string detail = std::to_string(registeredTabs) + " tab(s) registered";
        RecordModule(moduleName, true, detail);
        RecordEvent("info", Narrow(moduleName) + ": loaded, " + detail);
    }

    if (ec)
        RecordEvent("error", "Module scan stopped: " + ec.message());
}

void ModuleLoader::UnloadAll()
{
    for (LoadedModule& module : m_modules)
    {
        if (!module.handle)
            continue;

        fc::Menu::Get().RemoveTabsByOwner(module.handle);
        if (module.shutdown)
        {
            DWORD exceptionCode = 0;
            if (!CallModuleShutdownGuarded(module.shutdown, &exceptionCode))
            {
                RecordEvent(
                    "error",
                    Narrow(module.name) + ": " FCSDK_MODULE_SHUTDOWN_NAME " raised " +
                    HexException(exceptionCode));
            }
        }
        FreeLibrary(module.handle);
        RecordEvent("info", Narrow(module.name) + ": unloaded");
    }

    m_modules.clear();
    m_loadedModuleNames.clear();
}

} // namespace fc::sdk
