// ============================================================================
//  Settings.cpp - JSON-backed user settings store.
// ----------------------------------------------------------------------------
//  Uses the vendored nlohmann/json single-header. The settings file lives next
//  to the host process exe (same convention as the Actions logs) so it survives
//  reloads of the injected DLL.
// ============================================================================

#include "Settings.h"
#include "../Core/Input.h"

#include <Windows.h>
#include "imgui.h"
#include <nlohmann/json.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace fc {

namespace {

constexpr const char* kFileName = "FishingCompanion_settings.json";
constexpr float kMinScale = 0.8f;
constexpr float kMaxScale = 1.6f;

std::wstring ExeDirectory()
{
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0)
        return {};

    return fs::path(buffer).parent_path().wstring();
}

std::wstring SettingsPath()
{
    return (fs::path(ExeDirectory()) / kFileName).wstring();
}

} // namespace

Settings::Settings() = default;

Settings& Settings::Get()
{
    static Settings instance;
    return instance;
}

void Settings::Load()
{
    if (m_loaded)
        return;
    m_loaded = true;

    const std::wstring path = SettingsPath();
    std::ifstream in(path);
    if (!in.is_open())
        return;

    json data;
    try
    {
        in >> data;
    }
    catch (...)
    {
        // Corrupt or partial file - keep defaults and let the next save
        // overwrite it cleanly.
        return;
    }

    if (data.contains("toggle_key") && data["toggle_key"].is_number_integer())
        m_toggleKey = data["toggle_key"].get<int>();
    if (data.contains("unload_key") && data["unload_key"].is_number_integer())
        m_unloadKey = data["unload_key"].get<int>();
    if (data.contains("auto_fish_key") && data["auto_fish_key"].is_number_integer())
        m_autoFishKey = data["auto_fish_key"].get<int>();
    if (data.contains("ui_scale") && data["ui_scale"].is_number_float())
    {
        const float scale = data["ui_scale"].get<float>();
        if (scale >= kMinScale && scale <= kMaxScale)
            m_uiScale = scale;
    }
}

void Settings::ApplyToRuntime()
{
    // Input holds function-local statics; we push them explicitly. A value
    // of 0 (unbound) is intentionally skipped so we never blank a real default.
    if (m_toggleKey != 0)
        Input::ToggleKey() = m_toggleKey;
    if (m_unloadKey != 0)
        Input::UnloadKey() = m_unloadKey;
    if (m_autoFishKey != 0)
        Input::AutoFishKey() = m_autoFishKey;

    ImGui::GetIO().FontGlobalScale = m_uiScale;
}

void Settings::LoadAndApply()
{
    Load();
    ApplyToRuntime();
}

void Settings::CaptureFromRuntime()
{
    // Seed the store from whatever the runtime currently holds so the first
    // save (and the Settings tab) reflects reality.
    if (m_toggleKey == 0)
        m_toggleKey = Input::ToggleKey();
    if (m_unloadKey == 0)
        m_unloadKey = Input::UnloadKey();
    if (m_autoFishKey == 0)
        m_autoFishKey = Input::AutoFishKey();
}

void Settings::SaveIfDirty()
{
    if (!m_dirty)
        return;
    m_dirty = false;

    json data;
    data["toggle_key"] = m_toggleKey;
    data["unload_key"] = m_unloadKey;
    data["auto_fish_key"] = m_autoFishKey;
    data["ui_scale"] = m_uiScale;

    const std::wstring dir = ExeDirectory();
    if (dir.empty())
        return;

    std::error_code ec;
    fs::create_directories(dir, ec);

    const std::wstring path = SettingsPath();
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open())
        return;

    out << data.dump(2);
}

void Settings::SetUiScale(float scale)
{
    if (scale < kMinScale)
        scale = kMinScale;
    if (scale > kMaxScale)
        scale = kMaxScale;

    Mark(scale != m_uiScale);
    m_uiScale = scale;
}

} // namespace fc
