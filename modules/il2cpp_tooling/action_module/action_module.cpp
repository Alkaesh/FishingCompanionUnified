#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <system_error>
#include <vector>

#include "../include/game_actions.hpp"

#pragma comment(linker, \
    "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' " \
    "version='6.0.0.0' processorArchitecture='*' " \
    "publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace {

HMODULE g_module{};
HFONT g_uiFont{};
HFONT g_headingFont{};
HBRUSH g_backgroundBrush{};
HWND g_menuWindow{};
HWND g_messageLabel{};
volatile LONG g_unloading{};

constexpr COLORREF kBackgroundColor = RGB(248, 249, 250);
constexpr int kWindowWidth = 800;
constexpr int kWindowHeight = 570;
constexpr UINT kCloseMenuForUnloadMessage = WM_APP + 101;

constexpr int kStatusButton = 200;
constexpr int kHitchButton = 201;
constexpr int kStartHookingButton = 202;
constexpr int kToggleReelButton = 203;
constexpr int kSwitchThrowModeButton = 204;
constexpr int kChangeThrowDistanceButton = 205;
constexpr int kReturnIdleButton = 206;
constexpr int kAutoCastButton = 207;
constexpr int kOpenLogButton = 208;
constexpr int kQuitButton = 209;
constexpr int kDirectProbeButton = 210;
constexpr int kDirectCastButton = 211;
constexpr int kSdkCastButton = 216;
constexpr int kSdkAutoCastButton = 218;
constexpr int kDispatcherPulseButton = 219;
constexpr int kForceReadyButton = 220;
constexpr int kForceReadyCastButton = 221;
constexpr int kAutoCastOnButton = 222;
constexpr int kAutoCastOffButton = 223;
constexpr int kAutoCastTickButton = 224;
constexpr int kDumpRefsButton = 225;
constexpr int kQuickPullButton = 226;
constexpr int kAutoCycleButton = 228;
constexpr int kRigInfoButton = 229;
constexpr int kCastPower1kButton = 230;
constexpr int kCastPower10kButton = 231;
constexpr int kCastInfoButton = 232;
constexpr int kMessageLabel = 300;

std::wstring process_directory() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring value(path);
    const auto pos = value.find_last_of(L"\\/");
    return pos == std::wstring::npos ? L"." : value.substr(0, pos);
}

void log_line(const std::string& text) {
    const std::filesystem::path path =
        std::filesystem::path(process_directory()) / L"il2cpp_action_module.log";
    std::ofstream out(path, std::ios::out | std::ios::app);
    if (out) {
        out << text << '\n';
    }
}

void clear_log_file() {
    const std::filesystem::path path =
        std::filesystem::path(process_directory()) / L"il2cpp_action_module.log";
    std::ofstream clear(path, std::ios::out | std::ios::trunc);
}

void log_ptr(const char* name, void* ptr) {
    char buffer[128]{};
    sprintf_s(buffer, "%s = 0x%p", name, ptr);
    log_line(buffer);
}

bool is_readable_address(const void* ptr, size_t size = sizeof(void*)) {
    return il2cpp_runtime::is_readable_span(ptr, size);
}

bool is_writable_address(void* ptr, size_t size = sizeof(void*)) {
    return il2cpp_runtime::is_writable_span(ptr, size);
}

bool is_executable_address(const void* ptr, size_t size = 1) {
    return il2cpp_runtime::is_executable_span(ptr, size);
}

void* offset_address(void* base, uintptr_t offset) {
    if (!base) {
        return nullptr;
    }

    uintptr_t address = 0;
    if (il2cpp_runtime::detail::add_overflows(
            reinterpret_cast<uintptr_t>(base),
            offset,
            address)) {
        return nullptr;
    }
    return reinterpret_cast<void*>(address);
}

bool is_printable_ascii(const char* text) {
    if (!text || !is_readable_address(text, 1)) {
        return false;
    }

    for (size_t i = 0; i < 128; ++i) {
        if (!is_readable_address(text + i, 1)) {
            return false;
        }
        const unsigned char ch = static_cast<unsigned char>(text[i]);
        if (ch == 0) {
            return i > 0;
        }
        if (ch < 0x20 || ch > 0x7E) {
            return false;
        }
    }
    return false;
}

std::string il2cpp_type_name(void* object) {
    if (!object || !is_readable_address(object, sizeof(void*))) {
        return {};
    }

    HMODULE gameAssembly = GetModuleHandleW(L"GameAssembly.dll");
    if (!gameAssembly) {
        return {};
    }

    auto object_get_class = reinterpret_cast<void* (*)(void*)>(
        GetProcAddress(gameAssembly, "il2cpp_object_get_class"));
    auto class_get_name = reinterpret_cast<const char* (*)(void*)>(
        GetProcAddress(gameAssembly, "il2cpp_class_get_name"));
    auto class_get_namespace = reinterpret_cast<const char* (*)(void*)>(
        GetProcAddress(gameAssembly, "il2cpp_class_get_namespace"));
    if (!object_get_class || !class_get_name || !class_get_namespace) {
        return {};
    }

    void* klass = object_get_class(object);
    if (!is_readable_address(klass, sizeof(void*))) {
        return {};
    }

    const char* ns = class_get_namespace(klass);
    const char* name = class_get_name(klass);
    if (!is_printable_ascii(name) || (ns && !is_printable_ascii(ns))) {
        return {};
    }

    std::string value = ns ? ns : "";
    if (!value.empty()) {
        value += ".";
    }
    value += name ? name : "<unknown>";
    return value;
}

void log_il2cpp_type(const char* label, void* object) {
    if (!object) {
        log_line(std::string(label) + " type: <null>");
        return;
    }

    if (!is_readable_address(object, sizeof(void*))) {
        log_line(std::string(label) + " type: <not-readable>");
        return;
    }

    const std::string typeName = il2cpp_type_name(object);
    if (typeName.empty()) {
        log_line(std::string(label) + " type: <not-il2cpp-object>");
        return;
    }

    char buffer[256]{};
    sprintf_s(
        buffer,
        "%s type: %s",
        label,
        typeName.c_str());
    log_line(buffer);
}

std::string trim(std::string value) {
    auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [=](char ch) {
        return !is_space(static_cast<unsigned char>(ch));
    }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [=](char ch) {
        return !is_space(static_cast<unsigned char>(ch));
    }).base(), value.end());
    return value;
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

struct ThrowPowerStaticState {
    bool available{};
    bool pressed{};
    float charge{};
};

namespace throw_power_offsets {

inline constexpr uintptr_t input_static_class_global_rva = 0x3EB76D0;
inline constexpr uintptr_t klass_static_fields = 184;
inline constexpr uintptr_t klass_initialized = 224;
inline constexpr uintptr_t pressed = 120;
inline constexpr uintptr_t charge = 124;
inline constexpr uintptr_t behavior_source = 136;
inline constexpr uintptr_t scene_pressed = 184;

} // namespace throw_power_offsets

namespace behavior_offsets {

inline constexpr uintptr_t value_type_global_rva = 0x3EB7538;
inline constexpr uintptr_t get_value = 0x7150;
inline constexpr uintptr_t set_value = 0x66C0;
inline constexpr uintptr_t has_value = 0x55E0;
inline constexpr unsigned short throw_power_id = 13;
inline constexpr unsigned short throw_charge_enabled_id = 33;

} // namespace behavior_offsets

void* throw_power_static_fields() {
    il2cpp_runtime::Module module;
    const auto slotAddress = module.checked_address(
        throw_power_offsets::input_static_class_global_rva,
        sizeof(uintptr_t));
    if (!slotAddress ||
        !is_readable_address(reinterpret_cast<void*>(*slotAddress), sizeof(uintptr_t))) {
        return nullptr;
    }

    auto classSlot = reinterpret_cast<uintptr_t*>(*slotAddress);
    void* klass = classSlot ? reinterpret_cast<void*>(*classSlot) : nullptr;

    void* staticFieldsSlotAddress =
        offset_address(klass, throw_power_offsets::klass_static_fields);
    void* initializedAddress =
        offset_address(klass, throw_power_offsets::klass_initialized);
    if (!is_readable_address(staticFieldsSlotAddress, sizeof(void*)) ||
        !is_readable_address(initializedAddress, sizeof(int))) {
        return nullptr;
    }

    auto classInit = reinterpret_cast<void (*)(void*)>(
        GetProcAddress(module.handle(), "il2cpp_runtime_class_init"));
    auto* initialized = reinterpret_cast<int*>(initializedAddress);
    if (classInit && !*initialized) {
        classInit(klass);
    }

    auto* staticFields = reinterpret_cast<void**>(staticFieldsSlotAddress);
    if (!is_readable_address(staticFields, sizeof(void*))) {
        return nullptr;
    }
    return *staticFields;
}

bool read_throw_power_state(ThrowPowerStaticState& state) {
    void* staticFields = throw_power_static_fields();
    if (!staticFields) {
        state = {};
        return false;
    }

    auto* pressed = reinterpret_cast<bool*>(
        offset_address(staticFields, throw_power_offsets::pressed));
    auto* charge = reinterpret_cast<float*>(
        offset_address(staticFields, throw_power_offsets::charge));
    if (!is_readable_address(pressed, sizeof(bool)) ||
        !is_readable_address(charge, sizeof(float))) {
        state = {};
        return false;
    }

    state.available = true;
    state.pressed = *pressed;
    state.charge = *charge;
    return true;
}

void log_throw_power_state(const char* label) {
    ThrowPowerStaticState state{};
    if (!read_throw_power_state(state)) {
        log_line(std::string(label) + ": throw power static fields unavailable");
        return;
    }

    char buffer[192]{};
    sprintf_s(
        buffer,
        "%s: pressed=%s charge=%.6f",
        label,
        state.pressed ? "true" : "false",
        state.charge);
    log_line(buffer);
}

bool set_throw_power_state(float power) {
    void* staticFields = throw_power_static_fields();
    if (!staticFields) {
        log_line("throw power direct write: static fields unavailable");
        return false;
    }

    bool* pressed = reinterpret_cast<bool*>(
        offset_address(staticFields, throw_power_offsets::pressed));
    float* charge = reinterpret_cast<float*>(
        offset_address(staticFields, throw_power_offsets::charge));
    if (!is_writable_address(pressed, sizeof(bool)) ||
        !is_writable_address(charge, sizeof(float))) {
        log_line("throw power direct write: static field span is not writable");
        return false;
    }

    const bool oldPressed = *pressed;
    const float oldCharge = *charge;
    *pressed = true;
    *charge = power;

    char buffer[224]{};
    sprintf_s(
        buffer,
        "throw power direct write: static=0x%p pressed %s->true charge %.3f->%.3f",
        staticFields,
        oldPressed ? "true" : "false",
        oldCharge,
        *charge);
    log_line(buffer);
    return true;
}

bool set_scene_input_pressed(bool pressedValue) {
    void* staticFields = throw_power_static_fields();
    if (!staticFields) {
        log_line("scene input direct write: static fields unavailable");
        return false;
    }

    bool* pressed = reinterpret_cast<bool*>(
        offset_address(staticFields, throw_power_offsets::scene_pressed));
    if (!is_writable_address(pressed, sizeof(bool))) {
        log_line("scene input direct write: static pressed field is not writable");
        return false;
    }

    const bool oldPressed = *pressed;
    *pressed = pressedValue;

    char buffer[192]{};
    sprintf_s(
        buffer,
        "scene input direct write: static=0x%p pressed %s->%s",
        staticFields,
        oldPressed ? "true" : "false",
        pressedValue ? "true" : "false");
    log_line(buffer);
    return true;
}

void* behavior_source_from_static() {
    void* staticFields = throw_power_static_fields();
    if (!staticFields) {
        return nullptr;
    }

    void** source = reinterpret_cast<void**>(
        offset_address(staticFields, throw_power_offsets::behavior_source));
    if (!is_readable_address(source, sizeof(void*))) {
        return nullptr;
    }
    return *source;
}

uintptr_t behavior_value_type() {
    il2cpp_runtime::Module module;
    const auto slotAddress = module.checked_address(
        behavior_offsets::value_type_global_rva,
        sizeof(uintptr_t));
    if (!slotAddress ||
        !is_readable_address(reinterpret_cast<void*>(*slotAddress), sizeof(uintptr_t))) {
        return 0;
    }

    auto* slot = reinterpret_cast<uintptr_t*>(*slotAddress);
    return is_readable_address(slot, sizeof(uintptr_t)) ? *slot : 0;
}

bool try_behavior_set_raw(
    void* fnPtr,
    unsigned short id,
    uintptr_t valueType,
    void* source,
    unsigned int rawValue) {
    using SetFn = uintptr_t (*)(unsigned short, uintptr_t, void*, unsigned int);
    __try {
        reinterpret_cast<SetFn>(fnPtr)(id, valueType, source, rawValue);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool try_behavior_get_raw(
    void* fnPtr,
    unsigned short id,
    uintptr_t valueType,
    void* source,
    unsigned long long* rawValue) {
    using GetFn = unsigned long long (*)(unsigned short, uintptr_t, void*);
    __try {
        *rawValue = reinterpret_cast<GetFn>(fnPtr)(id, valueType, source);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *rawValue = 0;
        return false;
    }
}

bool try_behavior_has_raw(
    void* fnPtr,
    unsigned short id,
    uintptr_t valueType,
    void* source,
    unsigned long long* rawValue) {
    using HasFn = unsigned long long (*)(unsigned short, uintptr_t, void*);
    __try {
        *rawValue = reinterpret_cast<HasFn>(fnPtr)(id, valueType, source);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *rawValue = 0;
        return false;
    }
}

bool set_behavior_raw(unsigned short id, unsigned int rawValue, const char* label) {
    void* source = behavior_source_from_static();
    const uintptr_t valueType = behavior_value_type();
    il2cpp_runtime::Module module;
    const auto address = module.executable_address(behavior_offsets::set_value);
    if (!source || !valueType || !address) {
        char buffer[192]{};
        sprintf_s(
            buffer,
            "%s behavior write skipped: source=0x%p valueType=0x%p fn=0x%p",
            label,
            source,
            reinterpret_cast<void*>(valueType),
            address ? reinterpret_cast<void*>(*address) : nullptr);
        log_line(buffer);
        return false;
    }

    const bool ok = try_behavior_set_raw(
        reinterpret_cast<void*>(*address),
        id,
        valueType,
        source,
        rawValue);
    char buffer[192]{};
    sprintf_s(
        buffer,
        "%s behavior write: id=%hu source=0x%p raw=0x%08X %s",
        label,
        id,
        source,
        rawValue,
        ok ? "ok" : "failed");
    log_line(buffer);
    return ok;
}

bool set_behavior_float(unsigned short id, float value, const char* label) {
    unsigned int raw = 0;
    std::memcpy(&raw, &value, sizeof(raw));
    return set_behavior_raw(id, raw, label);
}

bool read_behavior_float(unsigned short id, float& value) {
    value = 0.0f;
    void* source = behavior_source_from_static();
    const uintptr_t valueType = behavior_value_type();
    il2cpp_runtime::Module module;
    const auto address = module.executable_address(behavior_offsets::get_value);
    if (!source || !valueType || !address) {
        return false;
    }

    unsigned long long raw = 0;
    if (!try_behavior_get_raw(
            reinterpret_cast<void*>(*address),
            id,
            valueType,
            source,
            &raw)) {
        return false;
    }

    const unsigned int low = static_cast<unsigned int>(raw & 0xFFFFFFFFu);
    std::memcpy(&value, &low, sizeof(value));
    return std::isfinite(value);
}

bool read_behavior_bool(unsigned short id, bool& value) {
    value = false;
    void* source = behavior_source_from_static();
    const uintptr_t valueType = behavior_value_type();
    il2cpp_runtime::Module module;
    const auto address = module.executable_address(behavior_offsets::has_value);
    if (!source || !valueType || !address) {
        return false;
    }

    unsigned long long raw = 0;
    if (!try_behavior_has_raw(
            reinterpret_cast<void*>(*address),
            id,
            valueType,
            source,
            &raw)) {
        return false;
    }

    value = (raw & 0xFFu) != 0;
    return true;
}

void log_scene_input_state(const char* label) {
    void* staticFields = throw_power_static_fields();
    if (!staticFields) {
        log_line(std::string(label) + ": scene static fields unavailable");
        return;
    }

    bool scenePressed = false;
    bool throwPressed = false;
    float throwCharge = 0.0f;
    void* source = nullptr;

    bool* scenePressedPtr = reinterpret_cast<bool*>(
        offset_address(staticFields, throw_power_offsets::scene_pressed));
    bool* throwPressedPtr = reinterpret_cast<bool*>(
        offset_address(staticFields, throw_power_offsets::pressed));
    float* throwChargePtr = reinterpret_cast<float*>(
        offset_address(staticFields, throw_power_offsets::charge));
    void** sourcePtr = reinterpret_cast<void**>(
        offset_address(staticFields, throw_power_offsets::behavior_source));

    if (is_readable_address(scenePressedPtr, sizeof(bool))) {
        scenePressed = *scenePressedPtr;
    }
    if (is_readable_address(throwPressedPtr, sizeof(bool))) {
        throwPressed = *throwPressedPtr;
    }
    if (is_readable_address(throwChargePtr, sizeof(float))) {
        throwCharge = *throwChargePtr;
    }
    if (is_readable_address(sourcePtr, sizeof(void*))) {
        source = *sourcePtr;
    }

    float behaviorPower = 0.0f;
    bool hasPower = read_behavior_float(behavior_offsets::throw_power_id, behaviorPower);
    bool chargeEnabled = false;
    bool hasChargeEnabled = read_behavior_bool(
        behavior_offsets::throw_charge_enabled_id,
        chargeEnabled);

    char buffer[320]{};
    sprintf_s(
        buffer,
        "%s: static=0x%p scenePressed=%s source=0x%p prop13=%s%.3f prop33=%s%s throwPressed=%s throwCharge=%.3f",
        label,
        staticFields,
        scenePressed ? "true" : "false",
        source,
        hasPower ? "" : "<na>/",
        hasPower ? behaviorPower : 0.0f,
        hasChargeEnabled ? "" : "<na>/",
        chargeEnabled ? "true" : "false",
        throwPressed ? "true" : "false",
        throwCharge);
    log_line(buffer);
}

std::filesystem::path command_path() {
    return std::filesystem::path(process_directory()) / L"il2cpp_action_commands_v2.txt";
}

void clear_command_file() {
    std::error_code ec;
    std::filesystem::remove(command_path(), ec);
}

std::vector<std::string> take_commands() {
    const auto path = command_path();
    std::ifstream in(path);
    if (!in) {
        return {};
    }

    std::vector<std::string> commands;
    std::string line;
    while (std::getline(in, line)) {
        line = lower(trim(line));
        if (!line.empty() && line[0] != '#') {
            commands.push_back(line);
        }
    }
    in.close();

    std::error_code ec;
    std::filesystem::remove(path, ec);
    return commands;
}

std::filesystem::path log_path() {
    return std::filesystem::path(process_directory()) / L"il2cpp_action_module.log";
}

void set_menu_message(const std::wstring& message) {
    if (g_messageLabel) {
        SetWindowTextW(g_messageLabel, message.c_str());
    }
}

bool write_menu_command(const std::string& command) {
    const auto path = command_path();
    std::filesystem::path temp = path;
    temp += L".tmp.";
    temp += std::to_wstring(GetCurrentProcessId());
    temp += L".";
    temp += std::to_wstring(GetTickCount64());

    std::ofstream out(temp, std::ios::out | std::ios::trunc);
    if (!out) {
        set_menu_message(L"Cannot write command file.");
        return false;
    }

    out << command;
    if (command.empty() || command.back() != '\n') {
        out << '\n';
    }
    out.close();

    if (!out ||
        !MoveFileExW(
            temp.c_str(),
            path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::error_code ec;
        std::filesystem::remove(temp, ec);
        set_menu_message(L"Cannot publish command file.");
        return false;
    }

    set_menu_message(L"Command sent.");
    return true;
}

HFONT create_ui_font(HWND hwnd, int pointSize, int weight = FW_NORMAL) {
    HDC dc = GetDC(hwnd);
    const int height = -MulDiv(pointSize, GetDeviceCaps(dc, LOGPIXELSY), 72);
    ReleaseDC(hwnd, dc);

    return CreateFontW(
        height,
        0,
        0,
        0,
        weight,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI");
}

void ensure_menu_fonts(HWND hwnd) {
    if (!g_uiFont) {
        g_uiFont = create_ui_font(hwnd, 9);
    }
    if (!g_headingFont) {
        g_headingFont = create_ui_font(hwnd, 9, FW_SEMIBOLD);
    }
}

void set_control_font(HWND hwnd, HFONT font = nullptr) {
    SendMessageW(
        hwnd,
        WM_SETFONT,
        reinterpret_cast<WPARAM>(font ? font : g_uiFont),
        TRUE);
}

HWND add_button(HWND parent, int id, const wchar_t* text, int x, int y, int w, int h) {
    HWND button = CreateWindowExW(
        0,
        L"BUTTON",
        text,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        x,
        y,
        w,
        h,
        parent,
        reinterpret_cast<HMENU>(static_cast<intptr_t>(id)),
        g_module,
        nullptr);
    set_control_font(button);
    return button;
}

HWND add_static(HWND parent, const wchar_t* text, int x, int y, int w, int h, DWORD style = 0) {
    HWND label = CreateWindowExW(
        0,
        L"STATIC",
        text,
        WS_CHILD | WS_VISIBLE | style,
        x,
        y,
        w,
        h,
        parent,
        nullptr,
        g_module,
        nullptr);
    set_control_font(label);
    return label;
}

HWND add_group(HWND parent, const wchar_t* text, int x, int y, int w, int h) {
    HWND group = CreateWindowExW(
        0,
        L"BUTTON",
        text,
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        x,
        y,
        w,
        h,
        parent,
        nullptr,
        g_module,
        nullptr);
    set_control_font(group, g_headingFont);
    return group;
}

void create_embedded_menu(HWND hwnd) {
    ensure_menu_fonts(hwnd);

    add_group(hwnd, L"Cast", 16, 16, 368, 176);
    add_button(hwnd, kForceReadyCastButton, L"Scene cast", 32, 44, 144, 34);
    add_button(hwnd, kQuickPullButton, L"Quick pull", 208, 44, 144, 34);
    add_button(hwnd, kCastPower1kButton, L"Cast 1k", 32, 84, 144, 34);
    add_button(hwnd, kCastPower10kButton, L"Cast 10k", 208, 84, 144, 34);
    add_button(hwnd, kForceReadyButton, L"Force ready", 32, 124, 144, 34);
    add_button(hwnd, kDirectCastButton, L"Direct cast", 208, 124, 144, 34);

    add_group(hwnd, L"Input actions", 400, 16, 368, 176);
    add_button(hwnd, kHitchButton, L"Hitch", 416, 44, 144, 34);
    add_button(hwnd, kStartHookingButton, L"Start hooking", 592, 44, 144, 34);
    add_button(hwnd, kToggleReelButton, L"Toggle reel", 416, 84, 144, 34);
    add_button(hwnd, kSwitchThrowModeButton, L"Switch mode", 592, 84, 144, 34);
    add_button(hwnd, kChangeThrowDistanceButton, L"Change distance", 416, 124, 144, 34);
    add_button(hwnd, kReturnIdleButton, L"Force idle", 592, 124, 144, 34);

    add_group(hwnd, L"Automation", 16, 208, 368, 176);
    add_button(hwnd, kAutoCastOnButton, L"Bot ON", 32, 236, 144, 34);
    add_button(hwnd, kAutoCycleButton, L"Cycle ON", 208, 236, 144, 34);
    add_button(hwnd, kAutoCastTickButton, L"Bot tick", 32, 276, 144, 34);
    add_button(hwnd, kAutoCastOffButton, L"Bot OFF", 208, 276, 144, 34);
    add_button(hwnd, kSdkAutoCastButton, L"SDK auto cast", 32, 316, 144, 34);
    add_button(hwnd, kAutoCastButton, L"Fish WH", 208, 316, 144, 34);

    add_group(hwnd, L"Diagnostics", 400, 208, 368, 176);
    add_button(hwnd, kStatusButton, L"Status", 416, 236, 144, 34);
    add_button(hwnd, kRigInfoButton, L"Rig info", 592, 236, 144, 34);
    add_button(hwnd, kDumpRefsButton, L"Dump refs", 416, 276, 144, 34);
    add_button(hwnd, kCastInfoButton, L"Cast info", 592, 276, 144, 34);
    add_button(hwnd, kSdkCastButton, L"SDK cast", 416, 316, 144, 34);
    add_button(hwnd, kDispatcherPulseButton, L"Dispatcher", 592, 316, 144, 34);

    add_group(hwnd, L"Status", 16, 400, 752, 72);
    g_messageLabel = CreateWindowExW(
        WS_EX_STATICEDGE,
        L"STATIC",
        L"DLL loaded. Commands are executed by the in-process action module.",
        WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP | SS_CENTERIMAGE,
        32,
        426,
        496,
        28,
        hwnd,
        reinterpret_cast<HMENU>(static_cast<intptr_t>(kMessageLabel)),
        g_module,
        nullptr);
    set_control_font(g_messageLabel);

    add_button(hwnd, kOpenLogButton, L"Open log", 544, 424, 96, 32);
    add_button(hwnd, kQuitButton, L"Unload DLL", 652, 424, 96, 32);

    wchar_t folder[MAX_PATH]{};
    GetModuleFileNameW(nullptr, folder, MAX_PATH);
    std::wstring caption = L"Game process: ";
    caption += folder;
    add_static(hwnd, caption.c_str(), 24, 488, 744, 22, SS_LEFTNOWORDWRAP);
}

void handle_menu_command(HWND hwnd, int id) {
    switch (id) {
    case kStatusButton:
        write_menu_command("status");
        break;
    case kHitchButton:
        write_menu_command("hitch");
        break;
    case kStartHookingButton:
        write_menu_command("start_hooking");
        break;
    case kToggleReelButton:
        write_menu_command("toggle_reel");
        break;
    case kSwitchThrowModeButton:
        write_menu_command("switch_throw_mode");
        break;
    case kChangeThrowDistanceButton:
        write_menu_command("change_throw_distance");
        break;
    case kReturnIdleButton:
        write_menu_command("force_idle");
        break;
    case kQuickPullButton:
        write_menu_command("quick_pull");
        break;
    case kAutoCastButton:
        write_menu_command("fish_wh");
        break;
    case kDirectProbeButton:
        write_menu_command("direct_probe");
        break;
    case kDirectCastButton:
        write_menu_command("direct_cast");
        break;
    case kSdkCastButton:
        write_menu_command("sdk_cast");
        break;
    case kSdkAutoCastButton:
        write_menu_command("sdk_auto_cast");
        break;
    case kDispatcherPulseButton:
        write_menu_command("dispatcher_pulse");
        break;
    case kForceReadyButton:
        write_menu_command("force_ready");
        break;
    case kForceReadyCastButton:
        write_menu_command("max_cast");
        break;
    case kAutoCastOnButton:
        write_menu_command("bot_on");
        break;
    case kAutoCastOffButton:
        write_menu_command("bot_off");
        break;
    case kAutoCastTickButton:
        write_menu_command("bot_tick");
        break;
    case kDumpRefsButton:
        write_menu_command("dump_refs");
        break;
    case kAutoCycleButton:
        write_menu_command("auto_cycle_on");
        break;
    case kRigInfoButton:
        write_menu_command("rig_info");
        break;
    case kCastInfoButton:
        write_menu_command("cast_info");
        break;
    case kCastPower1kButton:
        write_menu_command("force_ready_cast_1000");
        break;
    case kCastPower10kButton:
        write_menu_command("force_ready_cast_10000");
        break;
    case kOpenLogButton:
        ShellExecuteW(hwnd, L"open", log_path().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        break;
    case kQuitButton:
        write_menu_command("unload");
        DestroyWindow(hwnd);
        break;
    default:
        break;
    }
}

LRESULT CALLBACK menu_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE:
        create_embedded_menu(hwnd);
        return 0;
    case WM_COMMAND:
        handle_menu_command(hwnd, LOWORD(wparam));
        return 0;
    case kCloseMenuForUnloadMessage:
        DestroyWindow(hwnd);
        return 0;
    case WM_CLOSE:
        if (InterlockedCompareExchange(&g_unloading, 0, 0) == 0) {
            write_menu_command("unload");
        }
        DestroyWindow(hwnd);
        return 0;
    case WM_CTLCOLORDLG:
        return reinterpret_cast<LRESULT>(g_backgroundBrush);
    case WM_CTLCOLORSTATIC:
        SetBkMode(reinterpret_cast<HDC>(wparam), TRANSPARENT);
        return reinterpret_cast<LRESULT>(g_backgroundBrush);
    case WM_DESTROY:
        g_menuWindow = nullptr;
        g_messageLabel = nullptr;
        if (g_uiFont) {
            DeleteObject(g_uiFont);
            g_uiFont = nullptr;
        }
        if (g_headingFont) {
            DeleteObject(g_headingFont);
            g_headingFont = nullptr;
        }
        if (g_backgroundBrush) {
            DeleteObject(g_backgroundBrush);
            g_backgroundBrush = nullptr;
        }
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

DWORD WINAPI menu_thread(void*) {
    INITCOMMONCONTROLSEX commonControls{
        sizeof(commonControls),
        ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES
    };
    InitCommonControlsEx(&commonControls);

    g_backgroundBrush = CreateSolidBrush(kBackgroundColor);

    WNDCLASSW wc{};
    wc.lpfnWndProc = menu_window_proc;
    wc.hInstance = g_module;
    wc.lpszClassName = L"Il2CppActionModuleEmbeddedMenu";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = g_backgroundBrush
        ? g_backgroundBrush
        : reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    RegisterClassW(&wc);

    g_menuWindow = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"RF4 Action Menu",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        kWindowWidth,
        kWindowHeight,
        nullptr,
        nullptr,
        g_module,
        nullptr);

    if (!g_menuWindow) {
        log_line("embedded menu: CreateWindowExW failed");
        return 0;
    }

    ShowWindow(g_menuWindow, SW_SHOWNORMAL);
    UpdateWindow(g_menuWindow);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}

void log_action_status(const char* name, void* action) {
    char buffer[256]{};
    sprintf_s(
        buffer,
        "%s status: action=0x%p map=0x%p state=0x%p triggerState=0x%p enabled=%s phase=%d pressed=%s triggered=%s",
        name,
        action,
        game_actions::input_action_map(action),
        game_actions::input_action_state(action),
        game_actions::input_action_trigger_state(action),
        game_actions::input_action_enabled(action) ? "true" : "false",
        game_actions::input_action_phase(action),
        game_actions::input_action_is_pressed(action) ? "true" : "false",
        game_actions::input_action_triggered(action) ? "true" : "false");
    log_line(buffer);
}

int list_count(void* list) {
    void* countField = offset_address(list, 0x18);
    if (!is_readable_address(countField, sizeof(int))) {
        return -1;
    }
    return *reinterpret_cast<int*>(countField);
}

void* object_field(void* instance, uintptr_t offset) {
    void* field = offset_address(instance, offset);
    if (!is_readable_address(field, sizeof(void*))) {
        return nullptr;
    }
    return *reinterpret_cast<void**>(field);
}

void log_callback_lists(void* inputActions) {
    void* fishingCallbacks = object_field(
        inputActions,
        action_offsets::UnityInput_Generated_InputSystemActions_m_FishingActionsCallbackInterfaces_field);
    void* fishingSetCallbacks = object_field(
        inputActions,
        action_offsets::UnityInput_Generated_InputSystemActions_m_FishingSetActionsCallbackInterfaces_field);
    void* handItemCallbacks = object_field(
        inputActions,
        action_offsets::UnityInput_Generated_InputSystemActions_m_HandItemActionsCallbackInterfaces_field);

    char buffer[256]{};
    sprintf_s(
        buffer,
        "callback lists: Fishing=%d FishingSet=%d HandItem=%d",
        list_count(fishingCallbacks),
        list_count(fishingSetCallbacks),
        list_count(handItemCallbacks));
    log_line(buffer);
}

struct ActionSet {
    void* fishingSet{};
    void* rod{};
    void* rigFromSet{};
    void* rigConnector{};
    void* interactiveRod{};
    void* toggleReel{};
    void* startHooking{};
    void* switchThrowMode{};
    void* hitch{};
    void* returnToIdle{};
    void* changeThrowDistance{};
    void* fishingSetInput{};
    void* handItemInput{};
    void* reelInput{};
    void* reel{};
    void* fish{};
    void* rig{};
    void* inputController{};
    void* fisher{};
};

bool try_call_full_power_vtable(
    void* fishingSet,
    void* fnPtr,
    void* methodInfo,
    double* rawPower) {
    using FullPowerFn = double (*)(void*, void*);
    __try {
        *rawPower = reinterpret_cast<FullPowerFn>(fnPtr)(fishingSet, methodInfo);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

namespace fishing_set_vtable {

inline constexpr uintptr_t full_power_fn_slot = 0xA68;
inline constexpr uintptr_t full_power_method_info_slot = 0xA70;

} // namespace fishing_set_vtable

namespace raw_action_offsets {

inline constexpr uintptr_t fishing_set_input_dispatch_pressed = 0xE266D0;
inline constexpr uintptr_t fishing_set_try_hitch_or_cast_pressed = 0x8ABF40;
inline constexpr uintptr_t fishing_set_throw_power_pressed = 0x8AC1F0;
inline constexpr uintptr_t fishing_set_do_cast_or_hitch_with_power = 0xC90D60;
inline constexpr uintptr_t fishing_set_update = 0xC90F70;
inline constexpr uintptr_t fishing_set_set_state = 0xC93B60;
inline constexpr uintptr_t fishing_set_enter_ready_vector = 0xC94B10;
inline constexpr uintptr_t fisher_prepare_current = 0xB870F0;
inline constexpr uintptr_t rod_reel_prepare_transform = 0x73F680;
inline constexpr uintptr_t cast_action_context_global = 0x3EB76A0;
inline constexpr uintptr_t rod_cast_target_interface = 0xDD40;
inline constexpr uintptr_t cast_target_get_active_object = 0xE70F90;
inline constexpr uintptr_t cast_target_has_cast_pose = 0x6E41F0;
inline constexpr uintptr_t cast_target_get_cast_pose = 0x6E3900;
inline constexpr uintptr_t cast_target_get_cast_component = 0x6E3E80;
inline constexpr uintptr_t transform_find_child_by_name = 0x907600;
inline constexpr uintptr_t fisher_prepare_child_name_a_global = 0x3E87BB8;
inline constexpr uintptr_t fisher_prepare_child_name_b_global = 0x3E87BA8;
inline constexpr uintptr_t resources_find_objects_of_type_all = 0x2EBB830;

} // namespace raw_action_offsets

namespace fish_offsets {

inline constexpr uintptr_t transform = 0x28;
inline constexpr uintptr_t active_flag = 0x58;
inline constexpr uintptr_t float_a = 0x78;
inline constexpr uintptr_t float_b = 0x7C;
inline constexpr uintptr_t float_c = 0x80;
inline constexpr uintptr_t float_d = 0x84;
inline constexpr uintptr_t float_e = 0x88;
inline constexpr uintptr_t rigidbody = 0x98;
inline constexpr uintptr_t vector_a = 0xCC;
inline constexpr uintptr_t vector_b = 0xD8;

} // namespace fish_offsets

namespace rig_offsets {

inline constexpr uintptr_t transform = 0x58;
inline constexpr uintptr_t transform_tip = 0x60;
inline constexpr uintptr_t state_bool = 0xA8;

} // namespace rig_offsets

namespace lure_complex_offsets {

inline constexpr uintptr_t transform = 0x20;
inline constexpr uintptr_t rigidbody = 0x28;
inline constexpr uintptr_t simple_lure = 0x38;
inline constexpr uintptr_t hook = 0x88;

} // namespace lure_complex_offsets

namespace lure_simple_offsets {

inline constexpr uintptr_t collider = 0xE0;
inline constexpr uintptr_t vector_a = 0xE8;
inline constexpr uintptr_t vector_b = 0xF4;
inline constexpr uintptr_t skeletal_root = 0x100;
inline constexpr uintptr_t float_a = 0x110;
inline constexpr uintptr_t float_b = 0x114;

} // namespace lure_simple_offsets

namespace rig_connector_methods {

inline constexpr uintptr_t line_distance = 0x1021220;   // public double baifinehodi()
inline constexpr uintptr_t behavior_9 = 0x10214A0;      // public float mlfgmdimjph()
inline constexpr uintptr_t total_connector_span = 0x10219C0; // public float kbcpdlnbhoh()
inline constexpr uintptr_t behavior_16 = 0x1021F60;     // public float eamnpeodgkb()
inline constexpr uintptr_t flag_72 = 0x1023730;         // public bool cejmbjpoeei()
inline constexpr uintptr_t behavior_6 = 0x1023740;      // public float needfmceefl()
inline constexpr uintptr_t timed_ok = 0x1023C10;        // public bool amlndoodhmo()
inline constexpr uintptr_t surface_value = 0x1023C40;   // public float cjpjhgkcboi()
inline constexpr uintptr_t behavior_27 = 0x1023C60;     // public double gbghmaefglc()
inline constexpr uintptr_t behavior_0 = 0x1025AC0;      // public float mpnbpelefhg()

} // namespace rig_connector_methods

namespace surface_connector_offsets {

inline constexpr uintptr_t break_force = 0x28;
inline constexpr uintptr_t joint_spring = 0x2C;
inline constexpr uintptr_t snag_time = 0x30;
inline constexpr uintptr_t smooth_break_time = 0x34;
inline constexpr uintptr_t nature_a = 0x38;
inline constexpr uintptr_t nature_b = 0x40;
inline constexpr uintptr_t active_flag = 0x48;

} // namespace surface_connector_offsets

namespace water2_offsets {

inline constexpr uintptr_t transform = 0x70;
inline constexpr uintptr_t sea_level = 0xD4;
inline constexpr uintptr_t wave_amp = 0xCC;

} // namespace water2_offsets

namespace ultimate_water_methods {

inline constexpr uintptr_t get_max_vertical_displacement = 0x6022A0;
inline constexpr uintptr_t get_height_at_xz = 0x13C0FB0; // public float GetHeightAt(float, float)

} // namespace ultimate_water_methods

namespace reel_input_methods {

inline constexpr uintptr_t reset_input = 0x650600;
inline constexpr uintptr_t set_crank_flag = 0x650770;
inline constexpr uintptr_t crank_tick = 0x650780;
inline constexpr uintptr_t primary_pressed = 0x650260;
inline constexpr uintptr_t alt_pressed = 0x650810;
inline constexpr uintptr_t bind_input = 0x650980;
inline constexpr uintptr_t release_pressed = 0x650FA0;
inline constexpr uintptr_t button_event = 0x6501C0;
inline constexpr uintptr_t crank_flag_field = 0x48;

} // namespace reel_input_methods

struct Vector3f {
    float x{};
    float y{};
    float z{};
};

struct Quaternionf {
    float x{};
    float y{};
    float z{};
    float w{1.0f};
};

bool sane_vector(const Vector3f& value) {
    constexpr float kLimit = 100000.0f;
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) &&
           std::fabs(value.x) < kLimit && std::fabs(value.y) < kLimit &&
           std::fabs(value.z) < kLimit;
}

bool sane_quaternion(const Quaternionf& value) {
    constexpr float kLimit = 10.0f;
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z) && std::isfinite(value.w) &&
           std::fabs(value.x) < kLimit && std::fabs(value.y) < kLimit &&
           std::fabs(value.z) < kLimit && std::fabs(value.w) < kLimit;
}

float distance_between(const Vector3f& a, const Vector3f& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
}

float vector_length(const Vector3f& value) {
    return std::sqrt((value.x * value.x) + (value.y * value.y) + (value.z * value.z));
}

Vector3f vector_add(const Vector3f& a, const Vector3f& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3f vector_subtract(const Vector3f& a, const Vector3f& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vector3f vector_scale(const Vector3f& value, float scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

Vector3f normalize_or_zero(Vector3f value) {
    const float length = vector_length(value);
    if (!std::isfinite(length) || length < 0.0001f) {
        return {};
    }
    return vector_scale(value, 1.0f / length);
}

Vector3f horizontal_normalize_or_zero(Vector3f value) {
    value.y = 0.0f;
    return normalize_or_zero(value);
}

Vector3f quaternion_forward(const Quaternionf& q) {
    return {
        2.0f * ((q.x * q.z) + (q.w * q.y)),
        2.0f * ((q.y * q.z) - (q.w * q.x)),
        1.0f - (2.0f * ((q.x * q.x) + (q.y * q.y)))
    };
}

std::string vector_to_string(const Vector3f& value) {
    char buffer[96]{};
    sprintf_s(buffer, "(%.2f, %.2f, %.2f)", value.x, value.y, value.z);
    return buffer;
}

bool read_vector3_field(void* instance, uintptr_t offset, Vector3f& value) {
    void* field = offset_address(instance, offset);
    if (!is_readable_address(field, sizeof(Vector3f))) {
        return false;
    }
    value = *reinterpret_cast<Vector3f*>(field);
    return sane_vector(value);
}

bool call_float0_safe(void* instance, uintptr_t rva, float& value) {
    value = 0.0f;
    if (!instance) {
        return false;
    }
    using Fn = float (*)(void*);
    il2cpp_runtime::Module module;
    const uintptr_t address = module.address(rva);
    auto fn = address && is_executable_address(reinterpret_cast<void*>(address))
        ? reinterpret_cast<Fn>(address)
        : nullptr;
    if (!fn) {
        return false;
    }
    __try {
        value = fn(instance);
        return std::isfinite(value);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        value = 0.0f;
        return false;
    }
}

bool call_float2_safe(void* instance, uintptr_t rva, float first, float second, float& value) {
    value = 0.0f;
    if (!instance) {
        return false;
    }
    using Fn = float (*)(void*, float, float);
    il2cpp_runtime::Module module;
    const uintptr_t address = module.address(rva);
    auto fn = address && is_executable_address(reinterpret_cast<void*>(address))
        ? reinterpret_cast<Fn>(address)
        : nullptr;
    if (!fn) {
        return false;
    }
    __try {
        value = fn(instance, first, second);
        return std::isfinite(value);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        value = 0.0f;
        return false;
    }
}

bool call_double0_safe(void* instance, uintptr_t rva, double& value) {
    value = 0.0;
    if (!instance) {
        return false;
    }
    using Fn = double (*)(void*);
    il2cpp_runtime::Module module;
    const uintptr_t address = module.address(rva);
    auto fn = address && is_executable_address(reinterpret_cast<void*>(address))
        ? reinterpret_cast<Fn>(address)
        : nullptr;
    if (!fn) {
        return false;
    }
    __try {
        value = fn(instance);
        return std::isfinite(value);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        value = 0.0;
        return false;
    }
}

bool call_bool0_safe(void* instance, uintptr_t rva, bool& value) {
    value = false;
    if (!instance) {
        return false;
    }
    using Fn = unsigned char (*)(void*);
    il2cpp_runtime::Module module;
    const uintptr_t address = module.address(rva);
    auto fn = address && is_executable_address(reinterpret_cast<void*>(address))
        ? reinterpret_cast<Fn>(address)
        : nullptr;
    if (!fn) {
        return false;
    }
    __try {
        value = fn(instance) != 0;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        value = false;
        return false;
    }
}

bool call_uintptr0_rva_safe(void* instance, uintptr_t rva, uintptr_t& value) {
    value = 0;
    if (!instance) {
        return false;
    }
    using Fn = uintptr_t (*)(void*);
    il2cpp_runtime::Module module;
    const uintptr_t address = module.address(rva);
    auto fn = address && is_executable_address(reinterpret_cast<void*>(address))
        ? reinterpret_cast<Fn>(address)
        : nullptr;
    if (!fn) {
        return false;
    }
    __try {
        value = fn(instance);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        value = 0;
        return false;
    }
}

bool call_ptr0_rva_safe(void* instance, uintptr_t rva, void*& value) {
    value = nullptr;
    if (!instance) {
        return false;
    }
    using Fn = void* (*)(void*);
    il2cpp_runtime::Module module;
    const uintptr_t address = module.address(rva);
    auto fn = address && is_executable_address(reinterpret_cast<void*>(address))
        ? reinterpret_cast<Fn>(address)
        : nullptr;
    if (!fn) {
        return false;
    }
    __try {
        value = fn(instance);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        value = nullptr;
        return false;
    }
}

bool call_bool_ptr_rva_safe(void* instance, uintptr_t rva, void* argument, bool& value) {
    value = false;
    if (!instance || !argument) {
        return false;
    }
    using Fn = unsigned char (*)(void*, void*);
    il2cpp_runtime::Module module;
    const uintptr_t address = module.address(rva);
    auto fn = address && is_executable_address(reinterpret_cast<void*>(address))
        ? reinterpret_cast<Fn>(address)
        : nullptr;
    if (!fn) {
        return false;
    }
    __try {
        value = fn(instance, argument) != 0;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        value = false;
        return false;
    }
}

bool call_bool_vec3_rva_safe(void* instance, uintptr_t rva, const Vector3f& argument, bool& value) {
    value = false;
    if (!instance) {
        return false;
    }
    using Fn = unsigned char (*)(void*, const Vector3f*);
    il2cpp_runtime::Module module;
    const uintptr_t address = module.address(rva);
    auto fn = address && is_executable_address(reinterpret_cast<void*>(address))
        ? reinterpret_cast<Fn>(address)
        : nullptr;
    if (!fn) {
        return false;
    }
    __try {
        value = fn(instance, &argument) != 0;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        value = false;
        return false;
    }
}

bool call_bool_int_rva_safe(void* instance, uintptr_t rva, int argument, bool& value) {
    value = false;
    if (!instance) {
        return false;
    }
    using Fn = unsigned char (*)(void*, int);
    il2cpp_runtime::Module module;
    const uintptr_t address = module.address(rva);
    auto fn = address && is_executable_address(reinterpret_cast<void*>(address))
        ? reinterpret_cast<Fn>(address)
        : nullptr;
    if (!fn) {
        return false;
    }
    __try {
        value = fn(instance, argument) != 0;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        value = false;
        return false;
    }
}

bool call_bool_bool_rva_safe(void* instance, uintptr_t rva, bool argument, bool& value) {
    value = false;
    if (!instance) {
        return false;
    }
    using Fn = unsigned char (*)(void*, unsigned char);
    il2cpp_runtime::Module module;
    const uintptr_t address = module.address(rva);
    auto fn = address && is_executable_address(reinterpret_cast<void*>(address))
        ? reinterpret_cast<Fn>(address)
        : nullptr;
    if (!fn) {
        return false;
    }
    __try {
        value = fn(instance, argument ? 1 : 0) != 0;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        value = false;
        return false;
    }
}

bool call_bool_bool_int_rva_safe(
    void* instance,
    uintptr_t rva,
    bool boolArgument,
    int intArgument,
    bool& value) {
    value = false;
    if (!instance) {
        return false;
    }
    using Fn = unsigned char (*)(void*, unsigned char, int);
    il2cpp_runtime::Module module;
    const uintptr_t address = module.address(rva);
    auto fn = address && is_executable_address(reinterpret_cast<void*>(address))
        ? reinterpret_cast<Fn>(address)
        : nullptr;
    if (!fn) {
        return false;
    }
    __try {
        value = fn(instance, boolArgument ? 1 : 0, intArgument) != 0;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        value = false;
        return false;
    }
}

bool call_vtable_bool0_safe(
    void* instance,
    uintptr_t functionSlotOffset,
    uintptr_t methodInfoSlotOffset,
    bool& value) {
    value = false;
    if (!instance || !is_readable_address(instance, sizeof(void*))) {
        return false;
    }

    void* vtable = *reinterpret_cast<void**>(instance);
    void* fnSlot = offset_address(vtable, functionSlotOffset);
    void* methodSlot = offset_address(vtable, methodInfoSlotOffset);
    if (!is_readable_address(fnSlot, sizeof(void*)) ||
        !is_readable_address(methodSlot, sizeof(void*))) {
        return false;
    }

    void* fnPtr = *reinterpret_cast<void**>(fnSlot);
    void* methodInfo = *reinterpret_cast<void**>(methodSlot);
    if (!fnPtr || !is_executable_address(fnPtr, 1)) {
        return false;
    }

    using Fn = unsigned char (*)(void*, void*);
    __try {
        value = reinterpret_cast<Fn>(fnPtr)(instance, methodInfo) != 0;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        value = false;
        return false;
    }
}

bool write_pointer_field(void* instance, uintptr_t offset, void* value) {
    void* field = offset_address(instance, offset);
    if (!is_writable_address(field, sizeof(void*))) {
        return false;
    }
    *reinterpret_cast<void**>(field) = value;
    return true;
}

bool write_u8_field(void* instance, uintptr_t offset, unsigned char value) {
    void* field = offset_address(instance, offset);
    if (!is_writable_address(field, sizeof(unsigned char))) {
        return false;
    }
    *reinterpret_cast<unsigned char*>(field) = value;
    return true;
}

bool write_i32_field(void* instance, uintptr_t offset, int value) {
    void* field = offset_address(instance, offset);
    if (!is_writable_address(field, sizeof(int))) {
        return false;
    }
    *reinterpret_cast<int*>(field) = value;
    return true;
}

bool write_u32_field(void* instance, uintptr_t offset, unsigned int value) {
    void* field = offset_address(instance, offset);
    if (!is_writable_address(field, sizeof(unsigned int))) {
        return false;
    }
    *reinterpret_cast<unsigned int*>(field) = value;
    return true;
}

bool write_f32_field(void* instance, uintptr_t offset, float value) {
    void* field = offset_address(instance, offset);
    if (!is_writable_address(field, sizeof(float))) {
        return false;
    }
    *reinterpret_cast<float*>(field) = value;
    return true;
}

template <typename T>
T read_field(void* instance, uintptr_t offset, T fallback = {});

void* read_global_pointer_rva(uintptr_t rva) {
    il2cpp_runtime::Module module;
    const auto address = module.checked_address(rva, sizeof(void*));
    if (!address) {
        return nullptr;
    }
    auto* slot = reinterpret_cast<void**>(address.value());
    if (!is_readable_address(slot, sizeof(void*))) {
        return nullptr;
    }
    return *slot;
}

void* read_static_fields_from_class_global(uintptr_t rva) {
    il2cpp_runtime::Module module;
    const auto slotAddress = module.checked_address(rva, sizeof(uintptr_t));
    if (!slotAddress ||
        !is_readable_address(reinterpret_cast<void*>(*slotAddress), sizeof(uintptr_t))) {
        return nullptr;
    }

    auto classSlot = reinterpret_cast<uintptr_t*>(*slotAddress);
    void* klass = classSlot ? reinterpret_cast<void*>(*classSlot) : nullptr;
    void* staticFieldsSlotAddress = offset_address(klass, throw_power_offsets::klass_static_fields);
    void* initializedAddress = offset_address(klass, throw_power_offsets::klass_initialized);
    if (!is_readable_address(staticFieldsSlotAddress, sizeof(void*)) ||
        !is_readable_address(initializedAddress, sizeof(int))) {
        return nullptr;
    }

    auto classInit = reinterpret_cast<void (*)(void*)>(
        GetProcAddress(module.handle(), "il2cpp_runtime_class_init"));
    auto* initialized = reinterpret_cast<int*>(initializedAddress);
    if (classInit && !*initialized) {
        classInit(klass);
    }

    auto* staticFields = reinterpret_cast<void**>(staticFieldsSlotAddress);
    if (!is_readable_address(staticFields, sizeof(void*))) {
        return nullptr;
    }
    return *staticFields;
}

bool call_ptr_ptr_ptr_rva_safe(
    void* first,
    uintptr_t rva,
    void* second,
    void* third,
    void*& value) {
    value = nullptr;
    if (!first || !second) {
        return false;
    }
    using Fn = void* (*)(void*, void*, void*);
    il2cpp_runtime::Module module;
    const uintptr_t address = module.address(rva);
    auto fn = address && is_executable_address(reinterpret_cast<void*>(address))
        ? reinterpret_cast<Fn>(address)
        : nullptr;
    if (!fn) {
        return false;
    }
    __try {
        value = fn(first, second, third);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        value = nullptr;
        return false;
    }
}

bool call_void_int_rva_safe(void* instance, uintptr_t rva, int argument) {
    if (!instance) {
        return false;
    }
    using Fn = void (*)(void*, int);
    il2cpp_runtime::Module module;
    const uintptr_t address = module.address(rva);
    auto fn = address && is_executable_address(reinterpret_cast<void*>(address))
        ? reinterpret_cast<Fn>(address)
        : nullptr;
    if (!fn) {
        return false;
    }
    __try {
        fn(instance, argument);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool call_void_bool_rva_safe(void* instance, uintptr_t rva, bool argument) {
    if (!instance) {
        return false;
    }
    using Fn = void (*)(void*, unsigned char);
    il2cpp_runtime::Module module;
    const uintptr_t address = module.address(rva);
    auto fn = address && is_executable_address(reinterpret_cast<void*>(address))
        ? reinterpret_cast<Fn>(address)
        : nullptr;
    if (!fn) {
        return false;
    }
    __try {
        fn(instance, argument ? 1 : 0);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool call_void0_rva_safe(void* instance, uintptr_t rva) {
    if (!instance) {
        return false;
    }
    using Fn = void (*)(void*);
    il2cpp_runtime::Module module;
    const uintptr_t address = module.address(rva);
    auto fn = address && is_executable_address(reinterpret_cast<void*>(address))
        ? reinterpret_cast<Fn>(address)
        : nullptr;
    if (!fn) {
        return false;
    }
    __try {
        fn(instance);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void* resolve_unity_icall(const char* name) {
    HMODULE gameAssembly = GetModuleHandleW(L"GameAssembly.dll");
    if (!gameAssembly) {
        return nullptr;
    }
    auto resolve = reinterpret_cast<void* (*)(const char*)>(
        GetProcAddress(gameAssembly, "il2cpp_resolve_icall"));
    return resolve ? resolve(name) : nullptr;
}

void* g_componentGetTransformIcall{};
void* g_transformGetPositionIcall{};
void* g_transformGetRotationIcall{};
void* g_transformSetPositionIcall{};
void* g_cameraMainIcall{};
void* g_rigidbodySetPositionIcall{};
void* g_rigidbodySetVelocityIcall{};
void* g_rigidbodyAddForceIcall{};

void* cached_unity_icall(void*& cache, const char* name) {
    if (!cache) {
        cache = resolve_unity_icall(name);
    }
    return cache;
}

void* component_transform(void* component) {
    if (!component || !is_readable_address(component, sizeof(void*))) {
        return nullptr;
    }
    using GetTransformFn = void* (*)(void*);
    auto fn = reinterpret_cast<GetTransformFn>(
        cached_unity_icall(
            g_componentGetTransformIcall,
            "UnityEngine.Component::get_transform()"));
    if (!fn) {
        return nullptr;
    }
    __try {
        return fn(component);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

bool transform_position(void* transform, Vector3f& value) {
    value = {};
    if (!transform || !is_readable_address(transform, sizeof(void*))) {
        return false;
    }
    using GetPositionInjectedFn = void (*)(void*, Vector3f*);
    auto fn = reinterpret_cast<GetPositionInjectedFn>(
        cached_unity_icall(
            g_transformGetPositionIcall,
            "UnityEngine.Transform::get_position_Injected(UnityEngine.Vector3&)"));
    if (!fn) {
        return false;
    }
    __try {
        fn(transform, &value);
        return sane_vector(value);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        value = {};
        return false;
    }
}

bool transform_rotation(void* transform, Quaternionf& value) {
    value = {};
    if (!transform || !is_readable_address(transform, sizeof(void*))) {
        return false;
    }
    using GetRotationInjectedFn = void (*)(void*, Quaternionf*);
    auto fn = reinterpret_cast<GetRotationInjectedFn>(
        cached_unity_icall(
            g_transformGetRotationIcall,
            "UnityEngine.Transform::get_rotation_Injected(UnityEngine.Quaternion&)"));
    if (!fn) {
        return false;
    }
    __try {
        fn(transform, &value);
        return sane_quaternion(value);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        value = {};
        return false;
    }
}

bool transform_set_position(void* transform, const Vector3f& value) {
    if (!transform || !sane_vector(value) || !is_readable_address(transform, sizeof(void*))) {
        return false;
    }
    using SetPositionInjectedFn = void (*)(void*, const Vector3f*);
    auto fn = reinterpret_cast<SetPositionInjectedFn>(
        cached_unity_icall(
            g_transformSetPositionIcall,
            "UnityEngine.Transform::set_position_Injected(UnityEngine.Vector3&)"));
    if (!fn) {
        return false;
    }
    __try {
        fn(transform, &value);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool component_position(void* component, Vector3f& value) {
    void* transform = component_transform(component);
    return transform_position(transform, value);
}

bool object_position(void* object, uintptr_t transformOffset, Vector3f& value) {
    if (!object) {
        return false;
    }
    if (transformOffset) {
        void* transform = object_field(object, transformOffset);
        if (transform_position(transform, value)) {
            return true;
        }
    }
    return component_position(object, value);
}

bool component_field_position(void* object, uintptr_t componentOffset, Vector3f& value) {
    void* component = object_field(object, componentOffset);
    return component_position(component, value);
}

void* camera_main() {
    using CameraMainFn = void* (*)();
    auto fn = reinterpret_cast<CameraMainFn>(
        cached_unity_icall(g_cameraMainIcall, "UnityEngine.Camera::get_main()"));
    if (!fn) {
        return nullptr;
    }
    __try {
        return fn();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

bool rigidbody_set_position(void* rigidbody, const Vector3f& value) {
    if (!rigidbody || !sane_vector(value) || !is_readable_address(rigidbody, sizeof(void*))) {
        return false;
    }
    using SetPositionInjectedFn = void (*)(void*, const Vector3f*);
    auto fn = reinterpret_cast<SetPositionInjectedFn>(
        cached_unity_icall(
            g_rigidbodySetPositionIcall,
            "UnityEngine.Rigidbody::set_position_Injected(UnityEngine.Vector3&)"));
    if (!fn) {
        return false;
    }
    __try {
        fn(rigidbody, &value);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool rigidbody_set_velocity(void* rigidbody, const Vector3f& value) {
    if (!rigidbody || !sane_vector(value) || !is_readable_address(rigidbody, sizeof(void*))) {
        return false;
    }
    using SetVelocityInjectedFn = void (*)(void*, const Vector3f*);
    auto fn = reinterpret_cast<SetVelocityInjectedFn>(
        cached_unity_icall(
            g_rigidbodySetVelocityIcall,
            "UnityEngine.Rigidbody::set_velocity_Injected(UnityEngine.Vector3&)"));
    if (!fn) {
        return false;
    }
    __try {
        fn(rigidbody, &value);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool rigidbody_add_force(void* rigidbody, const Vector3f& value, int forceMode) {
    if (!rigidbody || !sane_vector(value) || !is_readable_address(rigidbody, sizeof(void*))) {
        return false;
    }
    using AddForceInjectedFn = void (*)(void*, const Vector3f*, int);
    auto fn = reinterpret_cast<AddForceInjectedFn>(
        cached_unity_icall(
            g_rigidbodyAddForceIcall,
            "UnityEngine.Rigidbody::AddForce_Injected(UnityEngine.Vector3&,UnityEngine.ForceMode)"));
    if (!fn) {
        return false;
    }
    __try {
        fn(rigidbody, &value, forceMode);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

std::vector<void*> find_all_objects(
    const char* assemblyName,
    const char* namespaze,
    const char* className) {
    il2cpp_runtime::UnityObjectFinder finder(
        action_offsets::UnityEngine_Object_FindObjectsOfType_1_method);
    return finder.find_all(assemblyName, namespaze, className);
}

std::vector<void*> find_all_scene_objects(const char* className) {
    return find_all_objects("Assembly-CSharp.dll", "RF4.Client.FishingScene", className);
}

std::vector<void*> find_all_scene_objects_including_inactive(const char* className) {
    std::vector<void*> result = find_all_scene_objects(className);
    il2cpp_runtime::UnityObjectFinder finder(raw_action_offsets::resources_find_objects_of_type_all);
    const auto inactiveOrAssets = finder.find_all(
        "Assembly-CSharp.dll",
        "RF4.Client.FishingScene",
        className);
    for (void* object : inactiveOrAssets) {
        if (object && std::find(result.begin(), result.end(), object) == result.end()) {
            result.push_back(object);
        }
    }
    return result;
}

struct WaterSample {
    bool ok{};
    void* object{};
    const char* source{"<none>"};
    float height{};
    float maxVertical{};
    size_t ultimateWaterCount{};
    size_t water2Count{};
};

bool plausible_water_height(float value) {
    return std::isfinite(value) && std::fabs(value) < 10000.0f;
}

WaterSample sample_water_at(const Vector3f& position) {
    WaterSample sample{};
    const auto ultimateWaters = find_all_objects("Assembly-CSharp.dll", "UltimateWater", "Water");
    sample.ultimateWaterCount = ultimateWaters.size();

    float bestScore = std::numeric_limits<float>::max();
    for (void* water : ultimateWaters) {
        float height = 0.0f;
        if (!call_float2_safe(
                water,
                ultimate_water_methods::get_height_at_xz,
                position.x,
                position.z,
                height) ||
            !plausible_water_height(height)) {
            continue;
        }

        const float score = std::fabs(position.y - height);
        if (!sample.ok || score < bestScore) {
            sample.ok = true;
            sample.object = water;
            sample.source = "UltimateWater.GetHeightAt";
            sample.height = height;
            bestScore = score;

            float maxVertical = 0.0f;
            if (call_float0_safe(
                    water,
                    ultimate_water_methods::get_max_vertical_displacement,
                    maxVertical) &&
                std::isfinite(maxVertical)) {
                sample.maxVertical = maxVertical;
            }
        }
    }

    if (sample.ok) {
        return sample;
    }

    const auto water2Objects = find_all_objects("Assembly-CSharp.dll", "RF4.Client.Water", "Water2");
    sample.water2Count = water2Objects.size();
    for (void* water : water2Objects) {
        const float seaLevel = read_field<float>(water, water2_offsets::sea_level, 0.0f);
        if (!plausible_water_height(seaLevel)) {
            continue;
        }
        sample.ok = true;
        sample.object = water;
        sample.source = "Water2._seaLevel";
        sample.height = seaLevel;
        sample.maxVertical = read_field<float>(water, water2_offsets::wave_amp, 0.0f);
        break;
    }
    return sample;
}

bool clamp_lure_to_water(Vector3f& position, float preferredDepth, float maxDepth) {
    WaterSample water = sample_water_at(position);
    if (!water.ok) {
        return false;
    }

    preferredDepth = std::clamp(preferredDepth, 0.12f, 2.0f);
    maxDepth = std::clamp(maxDepth, preferredDepth + 0.05f, 3.0f);

    const float targetY = water.height - preferredDepth;
    const float minY = water.height - maxDepth;
    const float maxY = water.height - 0.08f;
    if (!std::isfinite(position.y) || position.y < minY || position.y > water.height + 0.75f) {
        position.y = targetY;
    } else {
        position.y = std::clamp(position.y, minY, maxY);
        position.y += std::clamp((targetY - position.y) * 0.45f, -0.12f, 0.22f);
    }
    return true;
}

void log_position_probe(const char* label, void* object, uintptr_t transformOffset) {
    Vector3f position{};
    const bool ok = object_position(object, transformOffset, position);
    char buffer[256]{};
    sprintf_s(
        buffer,
        "%s position: ok=%s obj=0x%p pos=%s",
        label,
        ok ? "true" : "false",
        object,
        ok ? vector_to_string(position).c_str() : "<none>");
    log_line(buffer);
}

bool compute_full_cast_power(const ActionSet& actions, double& rawPower, int& castPower) {
    rawPower = 0.0;
    castPower = 0;
    if (!actions.fishingSet || !is_readable_address(actions.fishingSet, sizeof(void*))) {
        log_line("computed cast power: missing FishingSet");
        return false;
    }

    void* vtable = *reinterpret_cast<void**>(actions.fishingSet);
    void* fnSlot = offset_address(vtable, fishing_set_vtable::full_power_fn_slot);
    void* methodInfoSlot = offset_address(
        vtable,
        fishing_set_vtable::full_power_method_info_slot);
    if (!is_readable_address(fnSlot, sizeof(void*)) ||
        !is_readable_address(methodInfoSlot, sizeof(void*))) {
        log_line("computed cast power: FishingSet vtable unreadable");
        return false;
    }

    void* fnPtr = *reinterpret_cast<void**>(fnSlot);
    void* methodInfo = *reinterpret_cast<void**>(methodInfoSlot);
    il2cpp_runtime::Module module;
    if (!fnPtr ||
        !module.contains(reinterpret_cast<uintptr_t>(fnPtr)) ||
        !is_executable_address(fnPtr, 1)) {
        log_line("computed cast power: vtable slot function is not executable in GameAssembly");
        return false;
    }

    if (!try_call_full_power_vtable(actions.fishingSet, fnPtr, methodInfo, &rawPower)) {
        log_line("computed cast power: vtable call raised exception");
        return false;
    }

    if (!std::isfinite(rawPower)) {
        log_line("computed cast power: raw value is not finite");
        return false;
    }

    const double floored = std::floor(static_cast<double>(static_cast<float>(rawPower)));
    if (floored < 1.0) {
        castPower = 1;
    } else if (floored > static_cast<double>(std::numeric_limits<int>::max())) {
        castPower = std::numeric_limits<int>::max();
    } else {
        castPower = static_cast<int>(floored);
    }

    char buffer[224]{};
    sprintf_s(
        buffer,
        "computed cast power: vtable=0x%p fn=0x%p method=0x%p raw=%.6f castPower=%d",
        vtable,
        fnPtr,
        methodInfo,
        rawPower,
        castPower);
    log_line(buffer);
    return true;
}

ActionSet discover_instances(bool verbose);
bool can_cast_or_hitch(const ActionSet& actions);
bool ensure_cast_ready(const ActionSet& actions, DWORD waitMs);

template <typename T>
T read_field(void* instance, uintptr_t offset, T fallback) {
    void* field = offset_address(instance, offset);
    if (!is_readable_address(field, sizeof(T))) {
        return fallback;
    }
    return *reinterpret_cast<T*>(field);
}

const char* fishing_set_state_name(int state) {
    switch (state) {
    case 0:
        return "0/enum0";
    case 1:
        return "1/enum1";
    case 2:
        return "2/castable";
    case 3:
        return "3/castable";
    default:
        return "unknown";
    }
}

int dispatcher_handler_count(void* dispatcher) {
    void* handlers = object_field(dispatcher, 0x18);
    return read_field<int>(handlers, 0x18, -1);
}

void log_dispatcher_state(const char* label, void* dispatcher) {
    void* handlers = object_field(dispatcher, 0x18);
    char buffer[256]{};
    sprintf_s(
        buffer,
        "%s: dispatcher=0x%p handlers=0x%p count=%d stopOnHandled=%u dispatching=%u",
        label,
        dispatcher,
        handlers,
        dispatcher_handler_count(dispatcher),
        static_cast<unsigned int>(read_field<unsigned char>(dispatcher, 0x10)),
        static_cast<unsigned int>(read_field<unsigned char>(dispatcher, 0x21)));
    log_line(buffer);
}

void enable_actions(const ActionSet& actions) {
    game_actions::enable_input_action(actions.toggleReel);
    game_actions::enable_input_action(actions.startHooking);
    game_actions::enable_input_action(actions.switchThrowMode);
    game_actions::enable_input_action(actions.hitch);
    game_actions::enable_input_action(actions.returnToIdle);
    game_actions::enable_input_action(actions.changeThrowDistance);
}

void log_actions(const ActionSet& actions) {
    log_ptr("FishingSet", actions.fishingSet);
    log_il2cpp_type("FishingSet", actions.fishingSet);
    if (actions.fishingSet) {
        char buffer[320]{};
        sprintf_s(
            buffer,
            "FishingSet fields: activeItem(+0x10)=0x%p transform(+0x30)=0x%p setData(+0x38)=0x%p marker(+0xA4)=0x%llX marker2(+0xAC)=0x%X state(+0x128)=%d/%s canCastOrHitch=%s",
            read_field<void*>(actions.fishingSet, 0x10),
            read_field<void*>(actions.fishingSet, 0x30),
            read_field<void*>(actions.fishingSet, 0x38),
            static_cast<unsigned long long>(read_field<unsigned long long>(actions.fishingSet, 0xA4)),
            read_field<unsigned int>(actions.fishingSet, 0xAC),
            read_field<int>(actions.fishingSet, 0x128),
            fishing_set_state_name(read_field<int>(actions.fishingSet, 0x128)),
            game_actions::call_bool0(actions.fishingSet, 0xC98820) ? "true" : "false");
        log_line(buffer);

        sprintf_s(
            buffer,
            "FishingSet refs: +0x40=0x%p +0x48=0x%p +0x58=0x%p +0x68=0x%p +0x1B0=0x%p +0x2C8=0x%p +0x2F0=0x%p",
            read_field<void*>(actions.fishingSet, 0x40),
            read_field<void*>(actions.fishingSet, 0x48),
            read_field<void*>(actions.fishingSet, 0x58),
            read_field<void*>(actions.fishingSet, 0x68),
            read_field<void*>(actions.fishingSet, 0x1B0),
            read_field<void*>(actions.fishingSet, 0x2C8),
            read_field<void*>(actions.fishingSet, 0x2F0));
        log_line(buffer);
    }

    log_ptr("Rod(+0x38)", actions.rod);
    log_il2cpp_type("Rod(+0x38)", actions.rod);
    log_ptr("RigFromSet(+0x40)", actions.rigFromSet);
    log_il2cpp_type("RigFromSet(+0x40)", actions.rigFromSet);
    log_ptr("RigConnector(+0x90)", actions.rigConnector);
    log_il2cpp_type("RigConnector(+0x90)", actions.rigConnector);
    log_ptr("InteractiveRod(+0x100)", actions.interactiveRod);
    log_il2cpp_type("InteractiveRod(+0x100)", actions.interactiveRod);
    log_ptr("Fisher", actions.fisher);
    log_il2cpp_type("Fisher", actions.fisher);
    if (actions.fisher) {
        char buffer[320]{};
        sprintf_s(
            buffer,
            "Fisher refs: transform(+0x38)=0x%p transform(+0x88)=0x%p current(+0xD0)=0x%p alt(+0xE0)=0x%p busy(+0x1A0)=0x%p",
            read_field<void*>(actions.fisher, action_offsets::RF4_Client_FishingScene_Fisher_transform_0_field),
            read_field<void*>(actions.fisher, action_offsets::RF4_Client_FishingScene_Fisher_transform_0_field_2),
            read_field<void*>(actions.fisher, 0xD0),
            read_field<void*>(actions.fisher, 0xE0),
            read_field<void*>(actions.fisher, 0x1A0));
        log_line(buffer);
    }

    log_action_status("Fishing.ToggleReel", actions.toggleReel);
    log_action_status("Fishing.StartHooking", actions.startHooking);
    log_action_status("FishingSet.SwitchThrowMode", actions.switchThrowMode);
    log_action_status("FishingSet.Hitch", actions.hitch);
    log_action_status("FishingSet.ReturnToIdle", actions.returnToIdle);
    log_action_status("HandItem.ChangeThrowDistance", actions.changeThrowDistance);
    log_ptr("FishingSceneInputController", actions.inputController);
    if (actions.inputController) {
        log_ptr(
            "FishingSceneInputController.dispatcher(+0x20)",
            object_field(actions.inputController, 0x20));
        log_ptr(
            "FishingSceneInputController.modeSource(+0x28)",
            object_field(actions.inputController, 0x28));
        log_dispatcher_state(
            "FishingSceneInputController.dispatcher(+0x20) state",
            object_field(actions.inputController, 0x20));
    }
    log_ptr("FishingSetUserInputController", actions.fishingSetInput);
    if (actions.fishingSetInput) {
        log_ptr(
            "FishingSetUserInputController.currentSet(+0x40)",
            object_field(actions.fishingSetInput, 0x40));
        log_ptr(
            "FishingSetUserInputController.dispatcher(+0x30)",
            object_field(actions.fishingSetInput, 0x30));
        log_dispatcher_state(
            "FishingSetUserInputController.dispatcher(+0x30) state",
            object_field(actions.fishingSetInput, 0x30));
    }
    log_throw_power_state("ThrowPower.static");
    log_ptr("HandItemInputController", actions.handItemInput);
    log_ptr("ReelUserInputController", actions.reelInput);
    log_ptr("Reel", actions.reel);
    log_il2cpp_type("Reel", actions.reel);
    if (actions.reel) {
        char buffer[320]{};
        sprintf_s(
            buffer,
            "Reel fields: +0xF0=0x%X +0xF4=%.3f +0xF8=%.3f +0xFC=%.3f +0x100=%.3f +0x104=%.3f +0x108=%.3f +0x10C=%.3f",
            read_field<unsigned int>(actions.reel, 0xF0),
            read_field<float>(actions.reel, 0xF4),
            read_field<float>(actions.reel, 0xF8),
            read_field<float>(actions.reel, 0xFC),
            read_field<float>(actions.reel, 0x100),
            read_field<float>(actions.reel, 0x104),
            read_field<float>(actions.reel, 0x108),
            read_field<float>(actions.reel, 0x10C));
        log_line(buffer);
    }
    log_ptr("Fish", actions.fish);
    log_il2cpp_type("Fish", actions.fish);
    if (actions.fish) {
        char buffer[320]{};
        sprintf_s(
            buffer,
            "Fish fields: +0xE8=0x%p +0xF0=0x%p +0xF8=%.3f +0xFC=%.3f +0x100=%.3f +0x104=%.3f",
            read_field<void*>(actions.fish, 0xE8),
            read_field<void*>(actions.fish, 0xF0),
            read_field<float>(actions.fish, 0xF8),
            read_field<float>(actions.fish, 0xFC),
            read_field<float>(actions.fish, 0x100),
            read_field<float>(actions.fish, 0x104));
        log_line(buffer);
    }
    log_ptr("Rig", actions.rig);
    log_il2cpp_type("Rig", actions.rig);
}

void log_object_ref(const char* name, void* object) {
    log_ptr(name, object);
    log_il2cpp_type(name, object);
}

void dump_fishing_set_refs(const ActionSet& actions) {
    log_line("deep refs dump started");
    log_actions(actions);
    if (!actions.fishingSet) {
        log_line("deep refs dump stopped: missing FishingSet");
        return;
    }

    constexpr uintptr_t refs[] = {
        0x10, 0x30, 0x38, 0x40, 0x48, 0x50, 0x58, 0x60, 0x68, 0x70,
        0x78, 0x80, 0x88, 0x90, 0x98, 0xB0, 0xB8, 0xC0, 0xC8, 0xD0,
        0xD8, 0xE0, 0xE8, 0xF0, 0xF8, 0x100, 0x108, 0x110, 0x118,
        0x120, 0x130, 0x138, 0x140, 0x148, 0x150, 0x158, 0x160,
        0x168, 0x170, 0x178, 0x180, 0x188, 0x190, 0x198, 0x1A0,
        0x1A8, 0x1B0, 0x1B8, 0x1C0, 0x1C8, 0x1D0, 0x1D8, 0x1E0,
        0x1E8, 0x1F0, 0x1F8, 0x200, 0x208, 0x210, 0x218, 0x220,
        0x228, 0x230, 0x238, 0x240, 0x248, 0x250, 0x258, 0x260,
        0x268, 0x270, 0x278, 0x280, 0x288, 0x290, 0x298, 0x2A0,
        0x2A8, 0x2B0, 0x2B8, 0x2C0, 0x2C8, 0x2D0, 0x2D8, 0x2E0,
        0x2E8, 0x2F0, 0x2F8, 0x300
    };

    for (uintptr_t offset : refs) {
        void* value = read_field<void*>(actions.fishingSet, offset);
        if (!is_readable_address(value, sizeof(void*))) {
            continue;
        }
        char label[64]{};
        sprintf_s(label, "FishingSet.ref(+0x%zX)", offset);
        log_object_ref(label, value);
    }
    log_line("deep refs dump finished");
}

void log_scalar_window(const char* label, void* object, const uintptr_t* offsets, size_t count) {
    log_ptr(label, object);
    log_il2cpp_type(label, object);
    if (!object) {
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        const uintptr_t offset = offsets[i];
        void* ptr = read_field<void*>(object, offset);
        const unsigned int u32 = read_field<unsigned int>(object, offset);
        const int i32 = read_field<int>(object, offset);
        const float f32 = read_field<float>(object, offset);
        char buffer[256]{};
        sprintf_s(
            buffer,
            "%s +0x%zX: ptr=0x%p u32=0x%08X i32=%d f32=%.6f",
            label,
            offset,
            ptr,
            u32,
            i32,
            f32);
        log_line(buffer);
    }
}

struct WorldTelemetry {
    size_t fishCount{};
    size_t lureComplexCount{};
    size_t lureSimpleCount{};
    size_t hookCount{};
    size_t reelCount{};
    bool referenceOk{};
    Vector3f referencePosition{};
    std::string referenceLabel;
    bool nearestFishOk{};
    void* nearestFish{};
    Vector3f nearestFishPosition{};
    float nearestFishDistance{std::numeric_limits<float>::max()};
    bool lineDistanceOk{};
    double lineDistance{};
    bool connectorSpanOk{};
    float connectorSpan{};
    bool connectorFlag72{};
    bool connectorTimedOk{};
    bool connectorTimedKnown{};
    void* surfaceConnector{};
    bool surfaceConnectorActive{};
    float surfaceBreakForce{};
    float surfaceJointSpring{};
    float surfaceSnagTime{};
};

struct RetrieveResult {
    bool actionOk{};
    bool moved{};
    bool lineBeforeOk{};
    bool lineAfterOk{};
    double lineBefore{};
    double lineAfter{};
    float lureMove{};
};

bool first_reference_position(const ActionSet& actions, Vector3f& position, std::string& label) {
    const auto lureComplex = find_all_scene_objects("LureComplex");
    for (void* lure : lureComplex) {
        if (object_position(lure, lure_complex_offsets::transform, position)) {
            label = "LureComplex";
            return true;
        }
    }

    const auto lureSimple = find_all_scene_objects("LureSimple");
    for (void* lure : lureSimple) {
        if (component_field_position(lure, lure_simple_offsets::collider, position)) {
            label = "LureSimple.collider";
            return true;
        }
    }

    if (object_position(actions.rigConnector, 0, position)) {
        label = "RigConnector";
        return true;
    }
    if (object_position(actions.rigFromSet ? actions.rigFromSet : actions.rig, rig_offsets::transform, position)) {
        label = "Rig";
        return true;
    }
    if (object_position(actions.fishingSet, 0x30, position)) {
        label = "FishingSet.transform";
        return true;
    }
    return false;
}

void log_rig_connector_metrics(void* connector, WorldTelemetry& telemetry) {
    log_ptr("RigConnector.telemetry", connector);
    log_il2cpp_type("RigConnector.telemetry", connector);
    if (!connector) {
        return;
    }

    double lineDistance = 0.0;
    telemetry.lineDistanceOk = call_double0_safe(
        connector,
        rig_connector_methods::line_distance,
        lineDistance);
    telemetry.lineDistance = lineDistance;

    float span = 0.0f;
    telemetry.connectorSpanOk = call_float0_safe(
        connector,
        rig_connector_methods::total_connector_span,
        span);
    telemetry.connectorSpan = span;

    float behavior0 = 0.0f;
    float behavior6 = 0.0f;
    float behavior9 = 0.0f;
    float behavior16 = 0.0f;
    float surfaceValue = 0.0f;
    double behavior27 = 0.0;
    const bool behavior0Ok = call_float0_safe(
        connector,
        rig_connector_methods::behavior_0,
        behavior0);
    const bool behavior6Ok = call_float0_safe(
        connector,
        rig_connector_methods::behavior_6,
        behavior6);
    const bool behavior9Ok = call_float0_safe(
        connector,
        rig_connector_methods::behavior_9,
        behavior9);
    const bool behavior16Ok = call_float0_safe(
        connector,
        rig_connector_methods::behavior_16,
        behavior16);
    const bool surfaceOk = call_float0_safe(
        connector,
        rig_connector_methods::surface_value,
        surfaceValue);
    const bool behavior27Ok = call_double0_safe(
        connector,
        rig_connector_methods::behavior_27,
        behavior27);

    bool flag72 = false;
    call_bool0_safe(connector, rig_connector_methods::flag_72, flag72);
    telemetry.connectorFlag72 = flag72;
    bool timedOk = false;
    telemetry.connectorTimedKnown = call_bool0_safe(
        connector,
        rig_connector_methods::timed_ok,
        timedOk);
    telemetry.connectorTimedOk = timedOk;
    telemetry.surfaceConnector = read_field<void*>(connector, 0x38);
    telemetry.surfaceConnectorActive =
        telemetry.surfaceConnector &&
        is_readable_address(telemetry.surfaceConnector, surface_connector_offsets::active_flag + 1);
    if (telemetry.surfaceConnectorActive) {
        telemetry.surfaceBreakForce = read_field<float>(
            telemetry.surfaceConnector,
            surface_connector_offsets::break_force,
            0.0f);
        telemetry.surfaceJointSpring = read_field<float>(
            telemetry.surfaceConnector,
            surface_connector_offsets::joint_spring,
            0.0f);
        telemetry.surfaceSnagTime = read_field<float>(
            telemetry.surfaceConnector,
            surface_connector_offsets::snag_time,
            0.0f);
    }

    char buffer[1200]{};
    sprintf_s(
        buffer,
        "RigConnector metrics: lineDistance=%s%.3f span=%s%.3f b0=%s%.3f b6=%s%.3f b9=%s%.3f b16=%s%.3f b27=%s%.3f surface=%s%.3f flags(+0x70..72)=%u/%u/%u timedOk=%s%s debugForce=%.3f debugMult=%.3f surfaceConnector=0x%p active=%s break=%.3f spring=%.3f snagTime=%.3f",
        telemetry.lineDistanceOk ? "" : "!",
        telemetry.lineDistance,
        telemetry.connectorSpanOk ? "" : "!",
        telemetry.connectorSpan,
        behavior0Ok ? "" : "!",
        behavior0,
        behavior6Ok ? "" : "!",
        behavior6,
        behavior9Ok ? "" : "!",
        behavior9,
        behavior16Ok ? "" : "!",
        behavior16,
        behavior27Ok ? "" : "!",
        behavior27,
        surfaceOk ? "" : "!",
        surfaceValue,
        static_cast<unsigned int>(read_field<unsigned char>(connector, 0x70)),
        static_cast<unsigned int>(read_field<unsigned char>(connector, 0x71)),
        static_cast<unsigned int>(read_field<unsigned char>(connector, 0x72)),
        telemetry.connectorTimedKnown ? "" : "!",
        telemetry.connectorTimedOk ? "true" : "false",
        read_field<float>(connector, 0x98),
        read_field<float>(connector, 0x9C),
        telemetry.surfaceConnector,
        telemetry.surfaceConnectorActive ? "true" : "false",
        telemetry.surfaceBreakForce,
        telemetry.surfaceJointSpring,
        telemetry.surfaceSnagTime);
    log_line(buffer);
}

WorldTelemetry log_world_telemetry(const ActionSet& actions, bool verbose) {
    WorldTelemetry telemetry{};
    log_line(verbose ? "world telemetry started (verbose)" : "world telemetry started");

    const auto fishes = find_all_scene_objects_including_inactive("Fish");
    const auto lureComplex = find_all_scene_objects("LureComplex");
    const auto lureSimple = find_all_scene_objects("LureSimple");
    const auto hooks = find_all_scene_objects("Hook");
    const auto reels = find_all_scene_objects("Reel");
    telemetry.fishCount = fishes.size();
    telemetry.lureComplexCount = lureComplex.size();
    telemetry.lureSimpleCount = lureSimple.size();
    telemetry.hookCount = hooks.size();
    telemetry.reelCount = reels.size();

    telemetry.referenceOk = first_reference_position(
        actions,
        telemetry.referencePosition,
        telemetry.referenceLabel);

    char buffer[640]{};
    sprintf_s(
        buffer,
        "world counts: fish=%zu lureComplex=%zu lureSimple=%zu hook=%zu reel=%zu ref=%s %s setState=%d canCast=%s",
        telemetry.fishCount,
        telemetry.lureComplexCount,
        telemetry.lureSimpleCount,
        telemetry.hookCount,
        telemetry.reelCount,
        telemetry.referenceOk ? telemetry.referenceLabel.c_str() : "<none>",
        telemetry.referenceOk ? vector_to_string(telemetry.referencePosition).c_str() : "",
        read_field<int>(actions.fishingSet, 0x128),
        can_cast_or_hitch(actions) ? "true" : "false");
    log_line(buffer);

    log_position_probe("FishingSet", actions.fishingSet, 0x30);
    log_position_probe("Rig.active", actions.rigFromSet ? actions.rigFromSet : actions.rig, rig_offsets::transform);
    log_position_probe("RigConnector", actions.rigConnector, 0);
    log_position_probe("Reel.active", actions.reel, action_offsets::RF4_Client_FishingScene_Reel_transform_0_field);
    log_rig_connector_metrics(actions.rigConnector, telemetry);
    if (telemetry.referenceOk) {
        const WaterSample water = sample_water_at(telemetry.referencePosition);
        char waterBuffer[320]{};
        sprintf_s(
            waterBuffer,
            "Water.sample(ref=%s): ok=%s source=%s obj=0x%p height=%.3f lureY=%.3f depth=%.3f maxV=%.3f counts ultimate=%zu water2=%zu",
            telemetry.referenceLabel.c_str(),
            water.ok ? "true" : "false",
            water.source ? water.source : "<none>",
            water.object,
            water.ok ? water.height : 0.0f,
            telemetry.referencePosition.y,
            water.ok ? (water.height - telemetry.referencePosition.y) : 0.0f,
            water.maxVertical,
            water.ultimateWaterCount,
            water.water2Count);
        log_line(waterBuffer);
    }

    const size_t fishLimit = std::min<size_t>(fishes.size(), verbose ? 32 : 8);
    for (size_t i = 0; i < fishLimit; ++i) {
        void* fish = fishes[i];
        Vector3f position{};
        const bool positionOk = object_position(fish, fish_offsets::transform, position);
        Vector3f vectorA{};
        Vector3f vectorB{};
        const bool vectorAOk = read_vector3_field(fish, fish_offsets::vector_a, vectorA);
        const bool vectorBOk = read_vector3_field(fish, fish_offsets::vector_b, vectorB);
        float distance = -1.0f;
        if (positionOk && telemetry.referenceOk) {
            distance = distance_between(position, telemetry.referencePosition);
            const bool plausibleSceneFish =
                std::isfinite(distance) &&
                distance < 180.0f &&
                std::fabs(position.x) > 0.1f &&
                std::fabs(position.z) > 0.1f;
            if (plausibleSceneFish && distance < telemetry.nearestFishDistance) {
                telemetry.nearestFishDistance = distance;
                telemetry.nearestFish = fish;
                telemetry.nearestFishPosition = position;
                telemetry.nearestFishOk = true;
            }
        }

        sprintf_s(
            buffer,
            "Fish[%zu]: obj=0x%p pos=%s dist=%.2f active(+0x58)=%u rb=0x%p f78/f7C/f80/f84/f88=%.3f/%.3f/%.3f/%.3f/%.3f vecCC=%s%s vecD8=%s%s",
            i,
            fish,
            positionOk ? vector_to_string(position).c_str() : "<none>",
            distance,
            static_cast<unsigned int>(read_field<unsigned char>(fish, fish_offsets::active_flag)),
            read_field<void*>(fish, fish_offsets::rigidbody),
            read_field<float>(fish, fish_offsets::float_a),
            read_field<float>(fish, fish_offsets::float_b),
            read_field<float>(fish, fish_offsets::float_c),
            read_field<float>(fish, fish_offsets::float_d),
            read_field<float>(fish, fish_offsets::float_e),
            vectorAOk ? "" : "!",
            vectorAOk ? vector_to_string(vectorA).c_str() : "<none>",
            vectorBOk ? "" : "!",
            vectorBOk ? vector_to_string(vectorB).c_str() : "<none>");
        log_line(buffer);
    }

    const size_t complexLimit = std::min<size_t>(lureComplex.size(), verbose ? 8 : 3);
    for (size_t i = 0; i < complexLimit; ++i) {
        void* lure = lureComplex[i];
        Vector3f position{};
        const bool positionOk = object_position(lure, lure_complex_offsets::transform, position);
        sprintf_s(
            buffer,
            "LureComplex[%zu]: obj=0x%p pos=%s rb=0x%p simple=0x%p hook=0x%p",
            i,
            lure,
            positionOk ? vector_to_string(position).c_str() : "<none>",
            read_field<void*>(lure, lure_complex_offsets::rigidbody),
            read_field<void*>(lure, lure_complex_offsets::simple_lure),
            read_field<void*>(lure, lure_complex_offsets::hook));
        log_line(buffer);
    }

    const size_t simpleLimit = std::min<size_t>(lureSimple.size(), verbose ? 8 : 3);
    for (size_t i = 0; i < simpleLimit; ++i) {
        void* lure = lureSimple[i];
        Vector3f position{};
        const bool positionOk = component_field_position(lure, lure_simple_offsets::collider, position);
        Vector3f vectorA{};
        Vector3f vectorB{};
        const bool vectorAOk = read_vector3_field(lure, lure_simple_offsets::vector_a, vectorA);
        const bool vectorBOk = read_vector3_field(lure, lure_simple_offsets::vector_b, vectorB);
        sprintf_s(
            buffer,
            "LureSimple[%zu]: obj=0x%p pos=%s collider=0x%p root=0x%p f110/f114=%.3f/%.3f vecE8=%s%s vecF4=%s%s",
            i,
            lure,
            positionOk ? vector_to_string(position).c_str() : "<none>",
            read_field<void*>(lure, lure_simple_offsets::collider),
            read_field<void*>(lure, lure_simple_offsets::skeletal_root),
            read_field<float>(lure, lure_simple_offsets::float_a),
            read_field<float>(lure, lure_simple_offsets::float_b),
            vectorAOk ? "" : "!",
            vectorAOk ? vector_to_string(vectorA).c_str() : "<none>",
            vectorBOk ? "" : "!",
            vectorBOk ? vector_to_string(vectorB).c_str() : "<none>");
        log_line(buffer);
    }

    if (telemetry.nearestFishOk) {
        sprintf_s(
            buffer,
            "nearest fish: obj=0x%p dist=%.2f pos=%s ref=%s %s",
            telemetry.nearestFish,
            telemetry.nearestFishDistance,
            vector_to_string(telemetry.nearestFishPosition).c_str(),
            telemetry.referenceLabel.c_str(),
            vector_to_string(telemetry.referencePosition).c_str());
        log_line(buffer);
    } else {
        log_line("nearest fish: <none>");
    }

    log_line("world telemetry finished");
    return telemetry;
}

void fish_wh_probe(const ActionSet& actions) {
    il2cpp_runtime::ThreadAttach attach;
    if (!attach.attached()) {
        log_line("fish WH skipped: il2cpp_thread_attach failed");
        return;
    }
    log_world_telemetry(actions, true);
}

void water_info_probe(const ActionSet& actions) {
    il2cpp_runtime::ThreadAttach attach;
    if (!attach.attached()) {
        log_line("water info skipped: il2cpp_thread_attach failed");
        return;
    }

    log_line("water info started");
    const auto ultimateWaters = find_all_objects("Assembly-CSharp.dll", "UltimateWater", "Water");
    const auto water2Objects = find_all_objects("Assembly-CSharp.dll", "RF4.Client.Water", "Water2");
    char buffer[512]{};
    sprintf_s(
        buffer,
        "water counts: UltimateWater.Water=%zu RF4.Client.Water.Water2=%zu",
        ultimateWaters.size(),
        water2Objects.size());
    log_line(buffer);

    const size_t ultimateLimit = std::min<size_t>(ultimateWaters.size(), 8);
    for (size_t i = 0; i < ultimateLimit; ++i) {
        void* water = ultimateWaters[i];
        Vector3f position{};
        const bool positionOk = component_position(water, position);
        float maxVertical = 0.0f;
        const bool maxVerticalOk = call_float0_safe(
            water,
            ultimate_water_methods::get_max_vertical_displacement,
            maxVertical);
        sprintf_s(
            buffer,
            "UltimateWater[%zu]: obj=0x%p pos=%s maxVertical=%s%.3f",
            i,
            water,
            positionOk ? vector_to_string(position).c_str() : "<none>",
            maxVerticalOk ? "" : "!",
            maxVerticalOk ? maxVertical : 0.0f);
        log_line(buffer);
    }

    const size_t water2Limit = std::min<size_t>(water2Objects.size(), 8);
    for (size_t i = 0; i < water2Limit; ++i) {
        void* water = water2Objects[i];
        Vector3f position{};
        const bool positionOk = object_position(water, water2_offsets::transform, position);
        sprintf_s(
            buffer,
            "Water2[%zu]: obj=0x%p pos=%s seaLevel=%.3f waveAmp=%.3f",
            i,
            water,
            positionOk ? vector_to_string(position).c_str() : "<none>",
            read_field<float>(water, water2_offsets::sea_level, 0.0f),
            read_field<float>(water, water2_offsets::wave_amp, 0.0f));
        log_line(buffer);
    }

    Vector3f reference{};
    std::string label;
    if (first_reference_position(actions, reference, label)) {
        const WaterSample sample = sample_water_at(reference);
        sprintf_s(
            buffer,
            "water sample current %s %s: ok=%s source=%s obj=0x%p height=%.3f depth=%.3f",
            label.c_str(),
            vector_to_string(reference).c_str(),
            sample.ok ? "true" : "false",
            sample.source ? sample.source : "<none>",
            sample.object,
            sample.ok ? sample.height : 0.0f,
            sample.ok ? (sample.height - reference.y) : 0.0f);
        log_line(buffer);
    }

    log_line("water info finished");
}

namespace rig_lure_complex_offsets {

inline constexpr uintptr_t internal_component = 0xD8;
inline constexpr uintptr_t internal_rigidbody = 0x28;
inline constexpr uintptr_t direct_transform = 0x20;
inline constexpr uintptr_t direct_rigidbody = 0x28;

} // namespace rig_lure_complex_offsets

struct RigPhysicsRefs {
    void* rig{};
    void* component{};
    void* transform{};
    void* rigidbody{};
    const char* source{};
};

bool fill_rig_physics_refs_from_object(
    void* rig,
    const char* source,
    RigPhysicsRefs& refs) {
    if (!rig) {
        return false;
    }

    void* internalComponent = object_field(
        rig,
        rig_lure_complex_offsets::internal_component);
    void* internalRigidbody = object_field(
        internalComponent,
        rig_lure_complex_offsets::internal_rigidbody);
    void* internalTransform = component_transform(internalComponent);
    Vector3f internalPosition{};
    const bool internalPositionOk = transform_position(internalTransform, internalPosition);
    if (internalRigidbody || internalPositionOk) {
        refs = {rig, internalComponent, internalTransform, internalRigidbody, source};
        return true;
    }

    void* directRigidbody = object_field(
        rig,
        rig_lure_complex_offsets::direct_rigidbody);
    void* directTransform = object_field(
        rig,
        rig_lure_complex_offsets::direct_transform);
    Vector3f directPosition{};
    bool directTransformOk = transform_position(directTransform, directPosition);
    if (!directTransformOk) {
        directTransformOk = component_position(rig, directPosition);
    }
    if (directRigidbody || directTransformOk) {
        refs = {rig, rig, directTransform, directRigidbody, source};
        return true;
    }

    return false;
}

bool resolve_rig_physics_refs(const ActionSet& actions, RigPhysicsRefs& refs) {
    const struct Candidate {
        const char* source;
        void* object;
    } candidates[] = {
        {"FishingSet(+0x40)", actions.rigFromSet},
        {"Rig.active", actions.rig},
        {"game_actions::find_any_rig", game_actions::find_any_rig()},
    };

    for (const Candidate& candidate : candidates) {
        if (fill_rig_physics_refs_from_object(candidate.object, candidate.source, refs)) {
            return true;
        }
    }

    const auto rigLureObjects = find_all_scene_objects("RigLureComplex");
    for (void* rig : rigLureObjects) {
        if (fill_rig_physics_refs_from_object(rig, "FindObjects RigLureComplex", refs)) {
            return true;
        }
    }

    const auto lureObjects = find_all_scene_objects("LureComplex");
    for (void* lure : lureObjects) {
        RigPhysicsRefs direct{};
        direct.rig = lure;
        direct.component = lure;
        direct.transform = object_field(lure, lure_complex_offsets::transform);
        direct.rigidbody = object_field(lure, lure_complex_offsets::rigidbody);
        direct.source = "FindObjects LureComplex";
        Vector3f position{};
        if (direct.rigidbody || transform_position(direct.transform, position) ||
            component_position(lure, position)) {
            refs = direct;
            return true;
        }
    }

    return false;
}

bool rig_physics_position(const RigPhysicsRefs& refs, Vector3f& position) {
    if (transform_position(refs.transform, position)) {
        return true;
    }
    if (component_position(refs.component, position)) {
        return true;
    }
    if (component_position(refs.rig, position)) {
        return true;
    }
    return false;
}

bool choose_cast_origin(const ActionSet& actions, const RigPhysicsRefs& refs, Vector3f& origin, std::string& source) {
    if (object_position(actions.fishingSet, 0x30, origin)) {
        source = "FishingSet.transform";
        return true;
    }
    if (object_position(actions.rod, action_offsets::RF4_Client_FishingScene_Rod_transform_0_field, origin)) {
        source = "Rod.transform";
        return true;
    }
    if (object_position(actions.fisher, action_offsets::RF4_Client_FishingScene_Fisher_transform_0_field_2, origin)) {
        source = "Fisher.transform_0";
        return true;
    }
    if (rig_physics_position(refs, origin)) {
        source = "RigPhysics";
        return true;
    }
    return false;
}

bool choose_cast_direction(
    const ActionSet& actions,
    const WorldTelemetry& telemetry,
    const Vector3f& origin,
    Vector3f& direction,
    std::string& source) {
    if (telemetry.nearestFishOk) {
        Vector3f toFish = horizontal_normalize_or_zero(
            vector_subtract(telemetry.nearestFishPosition, origin));
        if (vector_length(toFish) > 0.1f) {
            direction = toFish;
            source = "nearest fish";
            return true;
        }
    }

    void* camera = camera_main();
    void* cameraTransform = component_transform(camera);
    Quaternionf cameraRotation{};
    if (transform_rotation(cameraTransform, cameraRotation)) {
        Vector3f cameraForward = horizontal_normalize_or_zero(quaternion_forward(cameraRotation));
        if (vector_length(cameraForward) > 0.1f) {
            direction = cameraForward;
            source = "main camera";
            return true;
        }
    }

    Vector3f fisherPosition{};
    Vector3f setPosition{};
    if (object_position(actions.fisher, action_offsets::RF4_Client_FishingScene_Fisher_transform_0_field_2, fisherPosition) &&
        object_position(actions.fishingSet, 0x30, setPosition)) {
        Vector3f fromFisher = horizontal_normalize_or_zero(vector_subtract(setPosition, fisherPosition));
        if (vector_length(fromFisher) > 0.1f) {
            direction = fromFisher;
            source = "fisher->set";
            return true;
        }
    }

    direction = {0.0f, 0.0f, 1.0f};
    source = "world +Z fallback";
    return true;
}

float choose_cast_distance(
    const WorldTelemetry& telemetry,
    const Vector3f& origin,
    float requestedDistance) {
    if (requestedDistance > 0.0f) {
        return std::clamp(requestedDistance, 10.0f, 68.0f);
    }
    if (telemetry.nearestFishOk) {
        const float fishFromRod = distance_between(origin, telemetry.nearestFishPosition);
        if (std::isfinite(fishFromRod) && fishFromRod > 2.0f) {
            return std::clamp(fishFromRod + 4.0f, 16.0f, 26.0f);
        }
    }
    return 22.0f;
}

bool physics_spin_cast_probe(
    const ActionSet& actions,
    float requestedDistance = 0.0f,
    bool startReel = false) {
    log_line("physics spin cast started");
    if (!actions.fishingSet) {
        log_line("physics spin cast stopped: missing FishingSet");
        return false;
    }

    WorldTelemetry telemetry = log_world_telemetry(actions, false);
    if (!ensure_cast_ready(actions, 2600)) {
        log_line("physics spin cast: cast-ready path failed; trying physics fallback anyway");
    }

    RigPhysicsRefs refs{};
    if (!resolve_rig_physics_refs(actions, refs)) {
        log_line("physics spin cast stopped: no lure Rigidbody/Transform refs");
        return false;
    }

    Vector3f origin{};
    std::string originSource;
    if (!choose_cast_origin(actions, refs, origin, originSource)) {
        log_line("physics spin cast stopped: no sane cast origin");
        return false;
    }

    Vector3f direction{};
    std::string directionSource;
    choose_cast_direction(actions, telemetry, origin, direction, directionSource);
    direction = horizontal_normalize_or_zero(direction);
    if (vector_length(direction) < 0.1f) {
        log_line("physics spin cast stopped: no sane cast direction");
        return false;
    }

    Vector3f beforePosition{};
    const bool beforePositionOk = rig_physics_position(refs, beforePosition);
    const float distance = choose_cast_distance(telemetry, origin, requestedDistance);
    Vector3f landingPosition = vector_add(origin, vector_scale(direction, distance));
    const bool landingWaterOk = clamp_lure_to_water(landingPosition, 0.42f, 1.15f);
    const float velocityMagnitude = std::clamp(distance * 0.68f, 9.0f, 30.0f);
    Vector3f startPosition = vector_add(
        origin,
        vector_add(vector_scale(direction, 1.65f), {0.0f, 1.05f, 0.0f}));
    if (landingWaterOk) {
        startPosition.y = std::max(startPosition.y, landingPosition.y + 1.15f);
    }
    Vector3f velocity = vector_scale(direction, velocityMagnitude);
    velocity.y = std::clamp(4.3f + (distance * 0.045f), 4.6f, 7.2f);
    Vector3f force = vector_scale(direction, velocityMagnitude * 0.34f);
    force.y = 1.45f;

    const bool transformMoved = transform_set_position(refs.transform, startPosition);
    const bool bodyMoved = rigidbody_set_position(refs.rigidbody, startPosition);
    const bool velocitySet = rigidbody_set_velocity(refs.rigidbody, velocity);
    const bool impulseOk = rigidbody_add_force(refs.rigidbody, force, 2);
    const bool state3Called = call_void_int_rva_safe(
        actions.fishingSet,
        raw_action_offsets::fishing_set_set_state,
        3);

    for (int i = 0; i < 10; ++i) {
        call_void0_rva_safe(actions.fishingSet, raw_action_offsets::fishing_set_update);
        call_void0_rva_safe(
            actions.inputController,
            action_offsets::RF4_Client_FishingScene_FishingSceneInputController_Update_0_method);
        if (i >= 4) {
            Vector3f current{};
            if (rig_physics_position(refs, current) && clamp_lure_to_water(current, 0.48f, 1.20f)) {
                rigidbody_set_position(refs.rigidbody, current);
                transform_set_position(refs.transform, current);
            }
        }
        Sleep(16);
    }

    bool landingMove = false;
    bool landingVelocity = false;
    if (landingWaterOk) {
        Vector3f settleVelocity = vector_scale(direction, 0.75f);
        settleVelocity.y = 0.04f;
        landingMove = rigidbody_set_position(refs.rigidbody, landingPosition) || landingMove;
        landingMove = transform_set_position(refs.transform, landingPosition) || landingMove;
        landingVelocity = rigidbody_set_velocity(refs.rigidbody, settleVelocity);
    }

    Vector3f afterPosition{};
    const bool afterPositionOk = rig_physics_position(refs, afterPosition);
    double lineDistance = 0.0;
    const bool lineOk = call_double0_safe(
        actions.rigConnector,
        rig_connector_methods::line_distance,
        lineDistance);

    char buffer[1200]{};
    sprintf_s(
        buffer,
        "physics spin cast result: rigSource=%s rig=0x%p comp=0x%p rb=0x%p transform=0x%p origin=%s %s dir=%s %s requested=%.1f distance=%.1f start=%s landing=%s waterLanding=%s landingMove=%s landingVelocity=%s velocity=%s force=%s before=%s%s after=%s%s transformMove=%s rbMove=%s velocitySet=%s force=%s setState3=%s state=%d marker=0x%llX canCast=%s line=%s%.2f startReel=%s",
        refs.source ? refs.source : "<none>",
        refs.rig,
        refs.component,
        refs.rigidbody,
        refs.transform,
        originSource.c_str(),
        vector_to_string(origin).c_str(),
        directionSource.c_str(),
        vector_to_string(direction).c_str(),
        requestedDistance,
        distance,
        vector_to_string(startPosition).c_str(),
        vector_to_string(landingPosition).c_str(),
        landingWaterOk ? "true" : "false",
        landingMove ? "true" : "false",
        landingVelocity ? "true" : "false",
        vector_to_string(velocity).c_str(),
        vector_to_string(force).c_str(),
        beforePositionOk ? "" : "!",
        beforePositionOk ? vector_to_string(beforePosition).c_str() : "<none>",
        afterPositionOk ? "" : "!",
        afterPositionOk ? vector_to_string(afterPosition).c_str() : "<none>",
        transformMoved ? "true" : "false",
        bodyMoved ? "true" : "false",
        velocitySet ? "true" : "false",
        impulseOk ? "true" : "false",
        state3Called ? "true" : "false",
        read_field<int>(actions.fishingSet, 0x128),
        static_cast<unsigned long long>(read_field<unsigned long long>(actions.fishingSet, 0xA4)),
        can_cast_or_hitch(actions) ? "true" : "false",
        lineOk ? "" : "!",
        lineOk ? lineDistance : -1.0,
        startReel ? "true" : "false");
    log_line(buffer);

    if (startReel) {
        Sleep(450);
        const bool reelOk = game_actions::pulse_input_action(actions.toggleReel);
        log_line(std::string("physics spin cast reel after cast: ") +
                 (reelOk ? "performed" : "failed"));
    }

    log_line("physics spin cast finished");
    return bodyMoved || velocitySet || impulseOk || transformMoved;
}

bool snap_lure_to_rod(const ActionSet& actions, const char* reason) {
    RigPhysicsRefs refs{};
    if (!resolve_rig_physics_refs(actions, refs)) {
        log_line("snap lure skipped: no lure Rigidbody/Transform refs");
        return false;
    }

    Vector3f origin{};
    std::string originSource;
    if (!choose_cast_origin(actions, refs, origin, originSource)) {
        log_line("snap lure skipped: no sane rod origin");
        return false;
    }

    Vector3f direction{};
    std::string directionSource;
    WorldTelemetry telemetry{};
    choose_cast_direction(actions, telemetry, origin, direction, directionSource);
    direction = horizontal_normalize_or_zero(direction);
    if (vector_length(direction) < 0.1f) {
        direction = {0.0f, 0.0f, 1.0f};
    }

    const Vector3f target = vector_add(
        origin,
        vector_add(vector_scale(direction, 0.55f), {0.0f, 0.12f, 0.0f}));
    const Vector3f zero{};
    Vector3f before{};
    const bool beforeOk = rig_physics_position(refs, before);
    bool velocityZeroed = false;
    bool bodyMoved = false;
    bool transformMoved = false;

    for (int i = 0; i < 12; ++i) {
        velocityZeroed = rigidbody_set_velocity(refs.rigidbody, zero) || velocityZeroed;
        bodyMoved = rigidbody_set_position(refs.rigidbody, target) || bodyMoved;
        transformMoved = transform_set_position(refs.transform, target) || transformMoved;
        call_void0_rva_safe(actions.fishingSet, raw_action_offsets::fishing_set_update);
        call_void0_rva_safe(
            actions.inputController,
            action_offsets::RF4_Client_FishingScene_FishingSceneInputController_Update_0_method);
        Sleep(16);
    }
    velocityZeroed = rigidbody_set_velocity(refs.rigidbody, zero) || velocityZeroed;
    bodyMoved = rigidbody_set_position(refs.rigidbody, target) || bodyMoved;
    transformMoved = transform_set_position(refs.transform, target) || transformMoved;

    Vector3f after{};
    const bool afterOk = rig_physics_position(refs, after);
    double lineDistance = 0.0;
    const bool lineOk = call_double0_safe(
        actions.rigConnector,
        rig_connector_methods::line_distance,
        lineDistance);

    char buffer[640]{};
    sprintf_s(
        buffer,
        "snap lure result: reason=%s source=%s rig=0x%p rb=0x%p transform=0x%p origin=%s %s target=%s before=%s%s after=%s%s velocityZero=%s rbMove=%s transformMove=%s state=%d marker=0x%llX line=%s%.2f",
        reason ? reason : "<none>",
        refs.source ? refs.source : "<none>",
        refs.rig,
        refs.rigidbody,
        refs.transform,
        originSource.c_str(),
        vector_to_string(origin).c_str(),
        vector_to_string(target).c_str(),
        beforeOk ? "" : "!",
        beforeOk ? vector_to_string(before).c_str() : "<none>",
        afterOk ? "" : "!",
        afterOk ? vector_to_string(after).c_str() : "<none>",
        velocityZeroed ? "true" : "false",
        bodyMoved ? "true" : "false",
        transformMoved ? "true" : "false",
        read_field<int>(actions.fishingSet, 0x128),
        static_cast<unsigned long long>(read_field<unsigned long long>(actions.fishingSet, 0xA4)),
        lineOk ? "" : "!",
        lineOk ? lineDistance : -1.0);
    log_line(buffer);
    return velocityZeroed || bodyMoved || transformMoved;
}

bool reel_crank_probe(const ActionSet& actions, int ticks = 45) {
    log_line("reel crank started");
    if (!actions.reelInput) {
        log_line("reel crank stopped: missing ReelUserInputController");
        return false;
    }

    ticks = std::clamp(ticks, 1, 240);
    double beforeLine = 0.0;
    const bool beforeLineOk = call_double0_safe(
        actions.rigConnector,
        rig_connector_methods::line_distance,
        beforeLine);

    const bool rebound = call_void0_rva_safe(
        actions.reelInput,
        reel_input_methods::bind_input);
    const bool flagWritten = write_u8_field(
        actions.reelInput,
        reel_input_methods::crank_flag_field,
        1);
    RigPhysicsRefs refs{};
    const bool directPullReady = resolve_rig_physics_refs(actions, refs);
    Vector3f pullOrigin{};
    std::string pullOriginSource;
    const bool pullOriginOk = directPullReady &&
        choose_cast_origin(actions, refs, pullOrigin, pullOriginSource);

    bool setFlagCall = false;
    bool primaryCall = false;
    bool altCall = false;
    bool crankCall = false;
    bool buttonCall = false;
    bool releaseCall = false;
    bool directPullMoved = false;
    bool directPullVelocity = false;
    bool ignored = false;

    setFlagCall = call_bool_bool_rva_safe(
        actions.reelInput,
        reel_input_methods::set_crank_flag,
        true,
        ignored) || setFlagCall;
    primaryCall = call_bool_bool_rva_safe(
        actions.reelInput,
        reel_input_methods::primary_pressed,
        true,
        ignored) || primaryCall;
    altCall = call_bool_bool_rva_safe(
        actions.reelInput,
        reel_input_methods::alt_pressed,
        true,
        ignored) || altCall;
    buttonCall = call_bool_bool_int_rva_safe(
        actions.reelInput,
        reel_input_methods::button_event,
        true,
        0x65,
        ignored) || buttonCall;

    for (int i = 0; i < ticks; ++i) {
        write_u8_field(actions.reelInput, reel_input_methods::crank_flag_field, 1);
        call_bool_bool_rva_safe(
            actions.reelInput,
            reel_input_methods::set_crank_flag,
            true,
            ignored);
        crankCall = call_bool_bool_rva_safe(
            actions.reelInput,
            reel_input_methods::crank_tick,
            true,
            ignored) || crankCall;
        if ((i % 10) == 0) {
            primaryCall = call_bool_bool_rva_safe(
                actions.reelInput,
                reel_input_methods::primary_pressed,
                true,
                ignored) || primaryCall;
            altCall = call_bool_bool_rva_safe(
                actions.reelInput,
                reel_input_methods::alt_pressed,
                true,
                ignored) || altCall;
            releaseCall = call_bool_bool_rva_safe(
                actions.reelInput,
                reel_input_methods::release_pressed,
                true,
                ignored) || releaseCall;
        }
        if (pullOriginOk) {
            Vector3f lurePosition{};
            if (rig_physics_position(refs, lurePosition)) {
                Vector3f toOrigin = vector_subtract(pullOrigin, lurePosition);
                const float distance = vector_length(toOrigin);
                if (distance > 0.45f) {
                    const Vector3f pullDirection = normalize_or_zero(toOrigin);
                    const float step = std::clamp(distance * 0.10f, 0.45f, 3.4f);
                    const float speed = std::clamp(distance * 1.35f, 9.0f, 42.0f);
                    const Vector3f nextPosition = vector_add(
                        lurePosition,
                        vector_scale(pullDirection, step));
                    const Vector3f pullVelocity = vector_scale(pullDirection, speed);
                    directPullVelocity = rigidbody_set_velocity(refs.rigidbody, pullVelocity) ||
                        directPullVelocity;
                    directPullMoved = rigidbody_set_position(refs.rigidbody, nextPosition) ||
                        directPullMoved;
                    directPullMoved = transform_set_position(refs.transform, nextPosition) ||
                        directPullMoved;
                }
            }
        }
        call_void0_rva_safe(actions.fishingSet, raw_action_offsets::fishing_set_update);
        call_void0_rva_safe(
            actions.inputController,
            action_offsets::RF4_Client_FishingScene_FishingSceneInputController_Update_0_method);
        Sleep(16);
    }

    double afterLine = 0.0;
    const bool afterLineOk = call_double0_safe(
        actions.rigConnector,
        rig_connector_methods::line_distance,
        afterLine);

    char buffer[512]{};
    sprintf_s(
        buffer,
        "reel crank result: ticks=%d rebound=%s flagWrite=%s flag(+0x48)=%u calls=set:%s button:%s primary:%s alt:%s crank:%s release:%s directPull=%s/%s origin=%s %s line=%s%.2f->%s%.2f state=%d reelF0=0x%X",
        ticks,
        rebound ? "true" : "false",
        flagWritten ? "true" : "false",
        static_cast<unsigned int>(read_field<unsigned char>(
            actions.reelInput,
            reel_input_methods::crank_flag_field,
            0)),
        setFlagCall ? "true" : "false",
        buttonCall ? "true" : "false",
        primaryCall ? "true" : "false",
        altCall ? "true" : "false",
        crankCall ? "true" : "false",
        releaseCall ? "true" : "false",
        directPullReady ? "ready" : "missing",
        directPullMoved || directPullVelocity ? "moved" : "idle",
        pullOriginOk ? pullOriginSource.c_str() : "<none>",
        pullOriginOk ? vector_to_string(pullOrigin).c_str() : "<none>",
        beforeLineOk ? "" : "!",
        beforeLineOk ? beforeLine : -1.0,
        afterLineOk ? "" : "!",
        afterLineOk ? afterLine : -1.0,
        read_field<int>(actions.fishingSet, 0x128),
        read_field<unsigned int>(actions.reel, 0xF0, 0));
    log_line(buffer);
    log_line("reel crank finished");
    return crankCall || setFlagCall || primaryCall || altCall || buttonCall ||
        directPullMoved || directPullVelocity;
}

RetrieveResult spinning_retrieve_probe(const ActionSet& actions, int ticks = 16) {
    log_line("spinning retrieve started");
    RetrieveResult result{};
    if (!actions.reelInput) {
        log_line("spinning retrieve stopped: missing ReelUserInputController");
        return result;
    }

    ticks = std::clamp(ticks, 1, 80);
    result.lineBeforeOk = call_double0_safe(
        actions.rigConnector,
        rig_connector_methods::line_distance,
        result.lineBefore);

    const bool rebound = call_void0_rva_safe(
        actions.reelInput,
        reel_input_methods::bind_input);
    const bool flagWritten = write_u8_field(
        actions.reelInput,
        reel_input_methods::crank_flag_field,
        1);

    RigPhysicsRefs refs{};
    const bool directPullReady = resolve_rig_physics_refs(actions, refs);
    Vector3f pullOrigin{};
    std::string pullOriginSource;
    const bool pullOriginOk = directPullReady &&
        choose_cast_origin(actions, refs, pullOrigin, pullOriginSource);
    Vector3f firstPosition{};
    const bool firstPositionOk = directPullReady && rig_physics_position(refs, firstPosition);

    bool setFlagCall = false;
    bool crankCall = false;
    bool primaryCall = false;
    bool releaseCall = false;
    bool directMoved = false;
    bool directVelocity = false;
    int waterAdjustments = 0;
    bool ignored = false;

    for (int i = 0; i < ticks; ++i) {
        write_u8_field(actions.reelInput, reel_input_methods::crank_flag_field, 1);
        setFlagCall = call_bool_bool_rva_safe(
            actions.reelInput,
            reel_input_methods::set_crank_flag,
            true,
            ignored) || setFlagCall;
        crankCall = call_bool_bool_rva_safe(
            actions.reelInput,
            reel_input_methods::crank_tick,
            true,
            ignored) || crankCall;
        if ((i % 7) == 0) {
            primaryCall = call_bool_bool_rva_safe(
                actions.reelInput,
                reel_input_methods::primary_pressed,
                true,
                ignored) || primaryCall;
        }
        if ((i % 11) == 0) {
            releaseCall = call_bool_bool_rva_safe(
                actions.reelInput,
                reel_input_methods::release_pressed,
                true,
                ignored) || releaseCall;
        }

        if (pullOriginOk) {
            Vector3f lurePosition{};
            if (rig_physics_position(refs, lurePosition)) {
                const Vector3f toOrigin = vector_subtract(pullOrigin, lurePosition);
                const float distance = vector_length(toOrigin);
                if (distance > 1.2f) {
                    const Vector3f pullDirection = normalize_or_zero(toOrigin);
                    const float step = std::clamp(distance * 0.007f, 0.025f, 0.085f);
                    const float speed = std::clamp(distance * 0.10f, 0.45f, 2.6f);
                    const Vector3f lift = {0.0f, (i % 9 == 0) ? 0.035f : 0.0f, 0.0f};
                    Vector3f nextPosition = vector_add(
                        vector_add(lurePosition, vector_scale(pullDirection, step)),
                        lift);
                    const bool waterAdjusted = clamp_lure_to_water(nextPosition, 0.55f, 1.15f);
                    if (waterAdjusted) {
                        ++waterAdjustments;
                    }
                    Vector3f pullVelocity = vector_add(
                        vector_scale(pullDirection, speed),
                        {0.0f, 0.08f, 0.0f});
                    if (waterAdjusted) {
                        pullVelocity.y = std::clamp(
                            (nextPosition.y - lurePosition.y) * 4.0f,
                            -0.45f,
                            1.35f);
                    }
                    directVelocity = rigidbody_set_velocity(refs.rigidbody, pullVelocity) ||
                        directVelocity;
                    directMoved = rigidbody_set_position(refs.rigidbody, nextPosition) ||
                        directMoved;
                    directMoved = transform_set_position(refs.transform, nextPosition) ||
                        directMoved;
                }
            }
        }

        call_void0_rva_safe(actions.fishingSet, raw_action_offsets::fishing_set_update);
        call_void0_rva_safe(
            actions.inputController,
            action_offsets::RF4_Client_FishingScene_FishingSceneInputController_Update_0_method);
        Sleep(16);
    }

    Vector3f lastPosition{};
    const bool lastPositionOk = directPullReady && rig_physics_position(refs, lastPosition);
    if (firstPositionOk && lastPositionOk) {
        result.lureMove = distance_between(firstPosition, lastPosition);
    }
    result.lineAfterOk = call_double0_safe(
        actions.rigConnector,
        rig_connector_methods::line_distance,
        result.lineAfter);
    result.actionOk = rebound || flagWritten || setFlagCall || crankCall || primaryCall ||
        releaseCall || directMoved || directVelocity;
    result.moved =
        (result.lineBeforeOk && result.lineAfterOk &&
         std::abs(result.lineBefore - result.lineAfter) > 0.18) ||
        result.lureMove > 0.18f ||
        directMoved ||
        directVelocity;

    char buffer[640]{};
    sprintf_s(
        buffer,
        "spinning retrieve result: ticks=%d rebound=%s flagWrite=%s calls=set:%s crank:%s primary:%s release:%s direct=%s/%s waterAdjust=%d origin=%s %s line=%s%.2f->%s%.2f lureMove=%.2f state=%d reelF0=0x%X",
        ticks,
        rebound ? "true" : "false",
        flagWritten ? "true" : "false",
        setFlagCall ? "true" : "false",
        crankCall ? "true" : "false",
        primaryCall ? "true" : "false",
        releaseCall ? "true" : "false",
        directPullReady ? "ready" : "missing",
        directMoved || directVelocity ? "moved" : "idle",
        waterAdjustments,
        pullOriginOk ? pullOriginSource.c_str() : "<none>",
        pullOriginOk ? vector_to_string(pullOrigin).c_str() : "<none>",
        result.lineBeforeOk ? "" : "!",
        result.lineBeforeOk ? result.lineBefore : -1.0,
        result.lineAfterOk ? "" : "!",
        result.lineAfterOk ? result.lineAfter : -1.0,
        result.lureMove,
        read_field<int>(actions.fishingSet, 0x128),
        read_field<unsigned int>(actions.reel, 0xF0, 0));
    log_line(buffer);
    log_line("spinning retrieve finished");
    return result;
}

bool neutralize_surface_connector(const ActionSet& actions, const char* reason) {
    void* surface = read_field<void*>(actions.rigConnector, 0x38);
    if (!surface ||
        !is_readable_address(surface, surface_connector_offsets::active_flag + 1)) {
        return false;
    }

    const float beforeBreak = read_field<float>(
        surface,
        surface_connector_offsets::break_force,
        0.0f);
    const float beforeSpring = read_field<float>(
        surface,
        surface_connector_offsets::joint_spring,
        0.0f);
    const float beforeSnag = read_field<float>(
        surface,
        surface_connector_offsets::snag_time,
        0.0f);

    const bool breakWritten = write_f32_field(
        surface,
        surface_connector_offsets::break_force,
        0.01f);
    const bool springWritten = write_f32_field(
        surface,
        surface_connector_offsets::joint_spring,
        0.0f);
    const bool snagWritten = write_f32_field(
        surface,
        surface_connector_offsets::snag_time,
        0.0f);
    const bool smoothWritten = write_f32_field(
        surface,
        surface_connector_offsets::smooth_break_time,
        0.0f);
    const bool flagWritten = write_u8_field(
        surface,
        surface_connector_offsets::active_flag,
        0);

    char buffer[512]{};
    sprintf_s(
        buffer,
        "surface connector neutralize: reason=%s connector=0x%p break %.3f->%.3f spring %.3f->%.3f snag %.3f->%.3f writes break=%s spring=%s snag=%s smooth=%s flag=%s natureA=0x%p natureB=0x%p",
        reason ? reason : "<none>",
        surface,
        beforeBreak,
        read_field<float>(surface, surface_connector_offsets::break_force, 0.0f),
        beforeSpring,
        read_field<float>(surface, surface_connector_offsets::joint_spring, 0.0f),
        beforeSnag,
        read_field<float>(surface, surface_connector_offsets::snag_time, 0.0f),
        breakWritten ? "true" : "false",
        springWritten ? "true" : "false",
        snagWritten ? "true" : "false",
        smoothWritten ? "true" : "false",
        flagWritten ? "true" : "false",
        read_field<void*>(surface, surface_connector_offsets::nature_a),
        read_field<void*>(surface, surface_connector_offsets::nature_b));
    log_line(buffer);
    return breakWritten || springWritten || snagWritten || smoothWritten || flagWritten;
}

bool anti_snag_probe(const ActionSet& actions, const char* reason = "manual") {
    log_line("anti snag started");
    neutralize_surface_connector(actions, reason);
    RigPhysicsRefs refs{};
    if (!resolve_rig_physics_refs(actions, refs)) {
        log_line("anti snag stopped: no lure Rigidbody/Transform refs");
        return false;
    }

    Vector3f origin{};
    std::string originSource;
    if (!choose_cast_origin(actions, refs, origin, originSource)) {
        log_line("anti snag stopped: no sane rod origin");
        return false;
    }

    Vector3f before{};
    if (!rig_physics_position(refs, before)) {
        log_line("anti snag stopped: no lure position");
        return false;
    }

    Vector3f toOrigin = horizontal_normalize_or_zero(vector_subtract(origin, before));
    if (vector_length(toOrigin) < 0.1f) {
        toOrigin = {0.0f, 0.0f, 1.0f};
    }
    Vector3f target = vector_add(
        before,
        vector_add(vector_scale(toOrigin, 1.15f), {0.0f, 0.95f, 0.0f}));
    const bool waterAdjusted = clamp_lure_to_water(target, 0.22f, 0.85f);
    const Vector3f velocity = vector_add(vector_scale(toOrigin, 4.0f), {0.0f, 3.2f, 0.0f});

    bool velocitySet = false;
    bool bodyMoved = false;
    bool transformMoved = false;
    bool forceOk = false;
    for (int i = 0; i < 8; ++i) {
        velocitySet = rigidbody_set_velocity(refs.rigidbody, velocity) || velocitySet;
        bodyMoved = rigidbody_set_position(refs.rigidbody, target) || bodyMoved;
        transformMoved = transform_set_position(refs.transform, target) || transformMoved;
        forceOk = rigidbody_add_force(refs.rigidbody, velocity, 2) || forceOk;
        call_void0_rva_safe(actions.fishingSet, raw_action_offsets::fishing_set_update);
        call_void0_rva_safe(
            actions.inputController,
            action_offsets::RF4_Client_FishingScene_FishingSceneInputController_Update_0_method);
        Sleep(16);
    }

    Vector3f after{};
    const bool afterOk = rig_physics_position(refs, after);
    double lineDistance = 0.0;
    const bool lineOk = call_double0_safe(
        actions.rigConnector,
        rig_connector_methods::line_distance,
        lineDistance);

    char buffer[640]{};
    sprintf_s(
        buffer,
        "anti snag result: reason=%s source=%s origin=%s %s before=%s target=%s waterAdjusted=%s after=%s%s velocity=%s velocitySet=%s rbMove=%s transformMove=%s force=%s line=%s%.2f state=%d",
        reason ? reason : "<none>",
        refs.source ? refs.source : "<none>",
        originSource.c_str(),
        vector_to_string(origin).c_str(),
        vector_to_string(before).c_str(),
        vector_to_string(target).c_str(),
        waterAdjusted ? "true" : "false",
        afterOk ? "" : "!",
        afterOk ? vector_to_string(after).c_str() : "<none>",
        vector_to_string(velocity).c_str(),
        velocitySet ? "true" : "false",
        bodyMoved ? "true" : "false",
        transformMoved ? "true" : "false",
        forceOk ? "true" : "false",
        lineOk ? "" : "!",
        lineOk ? lineDistance : -1.0,
        read_field<int>(actions.fishingSet, 0x128));
    log_line(buffer);
    log_line("anti snag finished");
    return velocitySet || bodyMoved || transformMoved || forceOk;
}

bool log_direct_bool_key(
    const char* name,
    void* instance,
    uintptr_t rva,
    bool pressed,
    int keyCode) {
    const bool result = game_actions::call_bool_key(instance, rva, pressed, keyCode);
    char buffer[192]{};
    sprintf_s(
        buffer,
        "%s(%s, KeyCode=%d): %s",
        name,
        pressed ? "true" : "false",
        keyCode,
        result ? "true" : "false");
    log_line(buffer);
    return result;
}

bool log_direct_bool1(const char* name, void* instance, uintptr_t rva, bool value) {
    const bool result = game_actions::call_bool1(instance, rva, value);
    char buffer[160]{};
    sprintf_s(
        buffer,
        "%s(%s): %s",
        name,
        value ? "true" : "false",
        result ? "true" : "false");
    log_line(buffer);
    return result;
}

void log_direct_void_bool(const char* name, void* instance, uintptr_t rva, bool value) {
    const bool result = game_actions::call_void_bool(instance, rva, value);
    char buffer[160]{};
    sprintf_s(
        buffer,
        "%s(%s): %s",
        name,
        value ? "true" : "false",
        result ? "called" : "failed");
    log_line(buffer);
}

void log_direct_void0(const char* name, void* instance, uintptr_t rva) {
    const bool result = game_actions::call_void0(instance, rva);
    log_line(std::string(name) + (result ? ": called" : ": failed"));
}

void log_direct_void_int(const char* name, void* instance, uintptr_t rva, int value) {
    const bool result = game_actions::call_void_int(instance, rva, value);
    char buffer[128]{};
    sprintf_s(buffer, "%s(%d): %s", name, value, result ? "called" : "failed");
    log_line(buffer);
}

void log_direct_bool_int(const char* name, void* instance, uintptr_t rva, int value) {
    const bool result = game_actions::call_bool_int(instance, rva, value);
    char buffer[160]{};
    sprintf_s(buffer, "%s(%d): %s", name, value, result ? "true" : "false");
    log_line(buffer);
}

bool dispatch_pressed(void* dispatcher, bool pressed, float deltaTime) {
    return game_actions::call_bool_bool_float(
        dispatcher,
        raw_action_offsets::fishing_set_input_dispatch_pressed,
        pressed,
        deltaTime);
}

void log_dispatcher_pressed(const ActionSet& actions, bool pressed, float deltaTime) {
    void* dispatcher = object_field(actions.fishingSetInput, 0x30);
    const bool result = dispatch_pressed(dispatcher, pressed, deltaTime);
    char buffer[192]{};
    sprintf_s(
        buffer,
        "FishingSetInput.DispatchPressed(%s, %.3f): dispatcher=0x%p result=%s",
        pressed ? "true" : "false",
        deltaTime,
        dispatcher,
        result ? "true" : "false");
    log_line(buffer);
}

void sdk_cast_probe(const ActionSet& actions, int power) {
    log_line("sdk cast probe started");
    log_actions(actions);
    log_direct_void_int(
        "FishingSet.DoCastOrHitchWithPower",
        actions.fishingSet,
        raw_action_offsets::fishing_set_do_cast_or_hitch_with_power,
        power);
    log_actions(actions);
    log_line("sdk cast probe finished");
}

void log_float_getter(const char* name, void* instance, uintptr_t rva) {
    const float value = game_actions::call_float0_value(instance, rva, -99999.0f);
    char buffer[160]{};
    sprintf_s(buffer, "%s: %.6f", name, value);
    log_line(buffer);
}

void log_double_getter(const char* name, void* instance, uintptr_t rva) {
    const double value = game_actions::call_double0_value(instance, rva, -99999.0);
    char buffer[160]{};
    sprintf_s(buffer, "%s: %.6f", name, value);
    log_line(buffer);
}

void rig_info_probe(const ActionSet& actions) {
    void* rig = actions.rigFromSet ? actions.rigFromSet : actions.rig;
    log_line("rig info probe started");
    log_ptr("Rig.active", rig);
    log_il2cpp_type("Rig.active", rig);
    if (!rig) {
        log_line("rig info probe stopped: missing RigBobberClassic");
        return;
    }

    char buffer[320]{};
    sprintf_s(
        buffer,
        "RigBobberClassic fields: opeg(+0x118)=%.3f sinkerA(+0x120)=0x%p sinkerB(+0x128)=0x%p jointA(+0x150)=0x%p jointB(+0x158)=0x%p jointC(+0x160)=0x%p joints(+0x168)=0x%p multi(+0x178)=0x%p",
        read_field<float>(rig, 0x118),
        read_field<void*>(rig, 0x120),
        read_field<void*>(rig, 0x128),
        read_field<void*>(rig, 0x150),
        read_field<void*>(rig, 0x158),
        read_field<void*>(rig, 0x160),
        read_field<void*>(rig, 0x168),
        read_field<void*>(rig, 0x178));
    log_line(buffer);
    log_object_ref("Rig.sinkerA(+0x120)", read_field<void*>(rig, 0x120));
    log_object_ref("Rig.sinkerB(+0x128)", read_field<void*>(rig, 0x128));
    log_object_ref("Rig.jointA(+0x150)", read_field<void*>(rig, 0x150));
    log_object_ref("Rig.jointB(+0x158)", read_field<void*>(rig, 0x158));
    log_object_ref("Rig.jointC(+0x160)", read_field<void*>(rig, 0x160));
    log_object_ref("Rig.multiJoint(+0x178)", read_field<void*>(rig, 0x178));

    log_float_getter("Rig.hblocphhibp()", rig, 0x114B5D0);
    log_float_getter("Rig.npgbjopcolg()", rig, 0x114D5F0);
    log_float_getter("Rig.kaloaamecia()", rig, 0x114D730);
    log_double_getter("Rig.bjdedejmfgd()", rig, 0xA36430);
    log_double_getter("Rig.offoolgcepa()", rig, 0xA385F0);
    log_line("rig info probe finished");
}

void power_info_probe(const ActionSet& actions) {
    log_line("power info probe started");
    log_actions(actions);

    double rawFullPower = 0.0;
    int computedPower = 0;
    compute_full_cast_power(actions, rawFullPower, computedPower);

    constexpr uintptr_t fishingSetOffsets[] = {
        0xA0, 0xA4, 0xAC, 0xB0, 0xB8, 0xC0, 0xC8, 0xD0,
        0xF0, 0xF8, 0x100, 0x108, 0x110, 0x118, 0x120, 0x128,
        0x130, 0x138, 0x140, 0x148, 0x150, 0x158, 0x160
    };
    constexpr uintptr_t connectorOffsets[] = {
        0x10, 0x18, 0x20, 0x28, 0x30, 0x38, 0x40, 0x48,
        0x50, 0x58, 0x60, 0x68, 0x70, 0x78, 0x80, 0x88,
        0x90, 0x98, 0xA0, 0xA8, 0xB0
    };
    constexpr uintptr_t rigOffsets[] = {
        0x100, 0x108, 0x110, 0x118, 0x120, 0x128, 0x130, 0x138,
        0x140, 0x148, 0x150, 0x158, 0x160, 0x168, 0x170, 0x178,
        0x180, 0x188, 0x190
    };
    constexpr uintptr_t rodOffsets[] = {
        0x10, 0x18, 0x20, 0x28, 0x30, 0x38, 0x40, 0x48,
        0x50, 0x58, 0x60, 0x68, 0x70, 0x78, 0x80, 0x88,
        0x90, 0x98, 0xA0, 0xA8, 0xB0
    };

    log_scalar_window("FishingSet.scalar", actions.fishingSet, fishingSetOffsets, std::size(fishingSetOffsets));
    log_scalar_window("RigConnector.scalar", actions.rigConnector, connectorOffsets, std::size(connectorOffsets));
    log_scalar_window("Rig.scalar", actions.rigFromSet ? actions.rigFromSet : actions.rig, rigOffsets, std::size(rigOffsets));
    log_scalar_window("Rod.scalar", actions.rod, rodOffsets, std::size(rodOffsets));

    char buffer[160]{};
    sprintf_s(
        buffer,
        "power info summary: rawFull=%.6f computedPower=%d canCastOrHitch=%s",
        rawFullPower,
        computedPower,
        can_cast_or_hitch(actions) ? "true" : "false");
    log_line(buffer);
    log_line("power info probe finished");
}

void sdk_auto_cast_probe(const ActionSet& actions) {
    log_line("sdk auto cast probe started");
    log_actions(actions);
    log_direct_void0("FishingSet.DoCastOrHitchAutoPower", actions.fishingSet, 0xC984C0);
    log_actions(actions);
    log_line("sdk auto cast probe finished");
}

void rebind_throw_input_probe(const ActionSet& actions) {
    log_line("throw input rebind started");
    log_dispatcher_state(
        "FishingSetUserInputController.dispatcher before rebind",
        object_field(actions.fishingSetInput, 0x30));
    log_direct_void0("FishingSetInput.BindThrowInput", actions.fishingSetInput, 0x8AC430);
    log_dispatcher_state(
        "FishingSetUserInputController.dispatcher after rebind",
        object_field(actions.fishingSetInput, 0x30));
    log_throw_power_state("throw power after rebind");
    log_line("throw input rebind finished");
}

void force_ready_probe(const ActionSet& actions) {
    log_line("force ready probe started (safe prepare path)");
    log_actions(actions);
    ensure_cast_ready(actions, 2400);
    log_actions(actions);
    log_line("force ready probe finished");
}

void force_idle_probe(const ActionSet& actions) {
    log_line("force idle probe started");
    bool ignored = false;
    const bool reelInputReset = call_void0_rva_safe(
        actions.reelInput,
        reel_input_methods::reset_input);
    const bool reelReleased = call_bool_bool_rva_safe(
        actions.reelInput,
        reel_input_methods::release_pressed,
        true,
        ignored);
    const bool reelFlagCleared = write_u8_field(
        actions.reelInput,
        reel_input_methods::crank_flag_field,
        0);
    const bool reelF0Cleared = write_u32_field(actions.reel, 0xF0, 0);
    const bool stateCalled = call_void_int_rva_safe(
        actions.fishingSet,
        raw_action_offsets::fishing_set_set_state,
        0);
    int stateAfterCall = read_field<int>(actions.fishingSet, 0x128);
    const bool directStateCleared =
        stateAfterCall == 0 ? false : write_i32_field(actions.fishingSet, 0x128, 0);
    const bool resetCalled = call_void0_rva_safe(actions.fishingSetInput, 0x8ABEA0);
    bool rebound = false;
    if (resetCalled) {
        rebound = call_void0_rva_safe(actions.fishingSetInput, 0x8AC430);
    }
    for (int i = 0; i < 5; ++i) {
        call_void0_rva_safe(actions.fishingSet, raw_action_offsets::fishing_set_update);
        call_void0_rva_safe(
            actions.inputController,
            action_offsets::RF4_Client_FishingScene_FishingSceneInputController_Update_0_method);
        Sleep(16);
    }
    const int finalState = read_field<int>(actions.fishingSet, 0x128);
    char buffer[320]{};
    sprintf_s(
        buffer,
        "force idle calls: ReelReset=%s ReelRelease=%s reelFlag0=%s reelF0=0:%s SetState(0)=%s directState0=%s state=%d->%d ResetThrowInput=%s BindThrowInput=%s",
        reelInputReset ? "true" : "false",
        reelReleased ? "true" : "false",
        reelFlagCleared ? "true" : "false",
        reelF0Cleared ? "true" : "false",
        stateCalled ? "true" : "false",
        directStateCleared ? "true" : "false",
        stateAfterCall,
        finalState,
        resetCalled ? "true" : "false",
        rebound ? "true" : "false");
    log_line(buffer);
    log_actions(actions);
    log_line("force idle probe finished");
}

void quick_pull_probe(const ActionSet& actions) {
    log_line("quick pull started");
    snap_lure_to_rod(actions, "quick_pull pre-idle");
    force_idle_probe(actions);
    snap_lure_to_rod(actions, "quick_pull post-idle");
    log_line("quick pull finished");
}

bool can_cast_or_hitch(const ActionSet& actions) {
    return game_actions::call_bool0(actions.fishingSet, 0xC98820);
}

void log_transform_candidate(const char* label, void* transform) {
    Vector3f position{};
    const bool positionOk = transform_position(transform, position);
    char buffer[256]{};
    sprintf_s(
        buffer,
        "%s: transform=0x%p pos=%s%s",
        label,
        transform,
        positionOk ? "" : "!",
        positionOk ? vector_to_string(position).c_str() : "<none>");
    log_line(buffer);
}

void* fisher_prepare_child_transform(const ActionSet& actions, uintptr_t stringGlobalRva, const char* label) {
    void* root = object_field(
        actions.fisher,
        action_offsets::RF4_Client_FishingScene_Fisher_transform_0_field_2);
    void* name = read_global_pointer_rva(stringGlobalRva);
    void* child = nullptr;
    const bool called = call_ptr_ptr_ptr_rva_safe(
        root,
        raw_action_offsets::transform_find_child_by_name,
        name,
        nullptr,
        child);
    char buffer[224]{};
    sprintf_s(
        buffer,
        "prepare child lookup %s: root=0x%p name=0x%p called=%s child=0x%p",
        label,
        root,
        name,
        called ? "true" : "false",
        child);
    log_line(buffer);
    if (child) {
        log_transform_candidate(label, child);
    }
    return child;
}

bool choose_ready_position(const ActionSet& actions, Vector3f& position, std::string& source) {
    if (object_position(actions.rigFromSet, lure_complex_offsets::transform, position)) {
        source = "RigFromSet.transform";
        return true;
    }
    if (object_position(actions.fishingSet, 0x30, position)) {
        source = "FishingSet.transform";
        return true;
    }
    if (object_position(actions.fisher, action_offsets::RF4_Client_FishingScene_Fisher_transform_0_field_2, position)) {
        source = "Fisher.transform_0(+0x88)";
        return true;
    }
    if (object_position(actions.rod, action_offsets::RF4_Client_FishingScene_Rod_transform_0_field, position)) {
        source = "Rod.transform_0";
        return true;
    }
    return false;
}

bool prepare_via_fisher(const ActionSet& actions) {
    if (!actions.fisher) {
        log_line("prepare cast: missing Fisher");
        return false;
    }

    const int beforeState = read_field<int>(actions.fishingSet, 0x128);
    uintptr_t rawResult = 0;
    const bool called = call_uintptr0_rva_safe(
        actions.fisher,
        raw_action_offsets::fisher_prepare_current,
        rawResult);
    Sleep(50);
    const int afterState = read_field<int>(actions.fishingSet, 0x128);
    const bool castable = can_cast_or_hitch(actions);
    const bool prepared =
        called &&
        (rawResult != 0 ||
         afterState != beforeState ||
         afterState == 1 ||
         castable ||
         read_field<void*>(actions.fishingSet, 0x2F0) != nullptr);

    char buffer[320]{};
    sprintf_s(
        buffer,
        "prepare cast via Fisher.japjacieogd: called=%s raw=0x%llX state %d->%d canCast=%s transform_0(+0x88)=0x%p current(+0xD0)=0x%p alt(+0xE0)=0x%p busy(+0x1A0)=0x%p",
        called ? "true" : "false",
        static_cast<unsigned long long>(rawResult),
        beforeState,
        afterState,
        castable ? "true" : "false",
        object_field(actions.fisher, action_offsets::RF4_Client_FishingScene_Fisher_transform_0_field_2),
        object_field(actions.fisher, 0xD0),
        object_field(actions.fisher, 0xE0),
        object_field(actions.fisher, 0x1A0));
    log_line(buffer);
    return prepared;
}

bool prepare_with_transform_candidate(
    const ActionSet& actions,
    const char* label,
    void* transform) {
    if (!actions.fishingSet || !transform) {
        return false;
    }

    log_transform_candidate(label, transform);
    const int beforeState = read_field<int>(actions.fishingSet, 0x128);
    bool result = false;
    const bool called = call_bool_ptr_rva_safe(
        actions.fishingSet,
        raw_action_offsets::rod_reel_prepare_transform,
        transform,
        result);
    Sleep(50);
    const int afterState = read_field<int>(actions.fishingSet, 0x128);
    const bool castable = can_cast_or_hitch(actions);
    const bool stored = read_field<void*>(actions.fishingSet, 0x2F0) == transform;
    const bool prepared =
        stored ||
        (called &&
         (result ||
          afterState != beforeState ||
          afterState == 1 ||
          castable));

    char buffer[256]{};
    sprintf_s(
        buffer,
        "prepare cast via RodReelRigFishingSet(%s): called=%s result=%s state %d->%d canCast=%s storedTransform(+0x2F0)=0x%p",
        label,
        called ? "true" : "false",
        result ? "true" : "false",
        beforeState,
        afterState,
        castable ? "true" : "false",
        read_field<void*>(actions.fishingSet, 0x2F0));
    log_line(buffer);
    return prepared;
}

bool string_contains_case_insensitive(const std::string& haystack, const char* needle) {
    if (!needle) {
        return false;
    }
    return lower(haystack).find(lower(needle)) != std::string::npos;
}

bool fishing_set_is_rod_reel(const ActionSet& actions) {
    const std::string typeName = il2cpp_type_name(actions.fishingSet);
    return string_contains_case_insensitive(typeName, "RodReelRigFishingSet");
}

bool pointer_is_unity_transform(void* value) {
    return string_contains_case_insensitive(il2cpp_type_name(value), "UnityEngine.Transform");
}

bool cast_marker_busy(const ActionSet& actions) {
    const unsigned long long marker =
        read_field<unsigned long long>(actions.fishingSet, 0xA4);
    const unsigned int low = static_cast<unsigned int>(marker & 0xFFFFFFFFull);
    const unsigned int high = static_cast<unsigned int>((marker >> 32) & 0xFFFFFFFFull);
    return low != high;
}

struct CastTargetInfo {
    bool targetCallOk{};
    void* target{};
    bool activeCallOk{};
    void* active{};
    bool activeReadyCallOk{};
    bool activeReady{};
};

CastTargetInfo inspect_cast_target(
    const ActionSet& actions,
    const char* label,
    bool hydrateActiveObject) {
    CastTargetInfo info{};
    if (!actions.rod) {
        log_line(std::string(label ? label : "cast target") + ": missing Rod(+0x38)");
        return info;
    }

    info.targetCallOk = call_ptr_ptr_ptr_rva_safe(
        actions.fishingSet ? actions.fishingSet : actions.rod,
        raw_action_offsets::rod_cast_target_interface,
        actions.fishingSet ? actions.fishingSet : actions.rod,
        actions.rod,
        info.target);

    const unsigned char targetReady =
        read_field<unsigned char>(info.target, 0x28, 0);
    const unsigned char targetBlocked =
        read_field<unsigned char>(info.target, 0x29, 0);
    void* cachedActive = read_field<void*>(info.target, 0x50, nullptr);
    if (hydrateActiveObject && info.target) {
        info.activeCallOk = call_ptr0_rva_safe(
            info.target,
            raw_action_offsets::cast_target_get_active_object,
            info.active);
    } else {
        info.active = cachedActive;
    }
    if (!info.active) {
        info.active = cachedActive;
    }

    const unsigned char activeInit =
        read_field<unsigned char>(info.active, 0x48, 0);
    const unsigned char activeBlocked =
        read_field<unsigned char>(info.active, 0x49, 0);
    if (info.active) {
        info.activeReadyCallOk = call_vtable_bool0_safe(
            info.active,
            504,
            512,
            info.activeReady);
    }

    bool poseReady = false;
    const bool poseReadyCallOk = call_bool0_safe(
        info.target,
        raw_action_offsets::cast_target_has_cast_pose,
        poseReady);
    void* castPose = read_field<void*>(info.target, 0xA0, nullptr);
    void* castPoseAfterCall = nullptr;
    const bool castPoseCallOk = call_ptr0_rva_safe(
        info.target,
        raw_action_offsets::cast_target_get_cast_pose,
        castPoseAfterCall);
    if (castPoseAfterCall) {
        castPose = castPoseAfterCall;
    }
    void* castComponent = read_field<void*>(info.target, 0x98, nullptr);
    void* castComponentAfterCall = nullptr;
    const bool castComponentCallOk = call_ptr0_rva_safe(
        info.target,
        raw_action_offsets::cast_target_get_cast_component,
        castComponentAfterCall);
    if (castComponentAfterCall) {
        castComponent = castComponentAfterCall;
    }

    char buffer[700]{};
    sprintf_s(
        buffer,
        "%s: setType=%s state=%d canCast=%s markerBusy=%s rod=0x%p targetCall=%s target=0x%p targetFlags(+28/+29)=%u/%u cachedActive(+50)=0x%p activeCall=%s active=0x%p activeFlags(+48/+49)=%u/%u activeReady=%s/%s poseReady=%s/%s pose(+A0)=0x%p poseCall=%s component(+98)=0x%p componentCall=%s storedTransform(+2F0)=0x%p",
        label ? label : "cast target",
        il2cpp_type_name(actions.fishingSet).c_str(),
        read_field<int>(actions.fishingSet, 0x128),
        can_cast_or_hitch(actions) ? "true" : "false",
        cast_marker_busy(actions) ? "true" : "false",
        actions.rod,
        info.targetCallOk ? "true" : "false",
        info.target,
        static_cast<unsigned int>(targetReady),
        static_cast<unsigned int>(targetBlocked),
        cachedActive,
        info.activeCallOk ? "true" : "false",
        info.active,
        static_cast<unsigned int>(activeInit),
        static_cast<unsigned int>(activeBlocked),
        info.activeReadyCallOk ? "called" : "skipped",
        info.activeReady ? "true" : "false",
        poseReadyCallOk ? "called" : "skipped",
        poseReady ? "true" : "false",
        castPose,
        castPoseCallOk ? "true" : "false",
        castComponent,
        castComponentCallOk ? "true" : "false",
        read_field<void*>(actions.fishingSet, 0x2F0));
    log_line(buffer);
    log_il2cpp_type("CastTarget", info.target);
    log_il2cpp_type("CastTarget.active(+0x50)", info.active);
    return info;
}

bool repair_cast_target_for_cast(const ActionSet& actions, const char* reason) {
    CastTargetInfo before = inspect_cast_target(actions, "cast target repair before", false);
    if (!before.target) {
        log_line("cast target repair skipped: target is null");
        return false;
    }

    const unsigned char ready = read_field<unsigned char>(before.target, 0x28, 0);
    const unsigned char blocked = read_field<unsigned char>(before.target, 0x29, 0);
    bool wroteReady = true;
    bool wroteBlocked = true;
    if (ready != 1) {
        wroteReady = write_u8_field(before.target, 0x28, 1);
    }
    if (blocked != 0) {
        wroteBlocked = write_u8_field(before.target, 0x29, 0);
    }

    char buffer[256]{};
    sprintf_s(
        buffer,
        "cast target repair flags: reason=%s old(+28/+29)=%u/%u wroteReady=%s wroteBlocked=%s",
        reason ? reason : "<none>",
        static_cast<unsigned int>(ready),
        static_cast<unsigned int>(blocked),
        wroteReady ? "true" : "false",
        wroteBlocked ? "true" : "false");
    log_line(buffer);

    CastTargetInfo after = inspect_cast_target(actions, "cast target repair after hydrate", true);
    const unsigned char afterReady = read_field<unsigned char>(after.target, 0x28, 0);
    const unsigned char afterBlocked = read_field<unsigned char>(after.target, 0x29, 1);
    return after.target && afterReady == 1 && afterBlocked == 0;
}

void* cast_action_service_object() {
    void* staticFields = read_static_fields_from_class_global(
        raw_action_offsets::cast_action_context_global);
    if (!staticFields) {
        return nullptr;
    }

    void** service = reinterpret_cast<void**>(offset_address(staticFields, 784));
    if (!is_readable_address(service, sizeof(void*))) {
        return nullptr;
    }
    return *service;
}

void log_cast_service_state(const char* label) {
    void* staticFields = read_static_fields_from_class_global(
        raw_action_offsets::cast_action_context_global);
    void* service = nullptr;
    void* serviceDispatcher = nullptr;
    if (staticFields) {
        void** serviceField = reinterpret_cast<void**>(offset_address(staticFields, 784));
        if (is_readable_address(serviceField, sizeof(void*))) {
            service = *serviceField;
        }
    }
    if (service) {
        serviceDispatcher = read_field<void*>(service, 0x60, nullptr);
    }

    char buffer[240]{};
    sprintf_s(
        buffer,
        "%s: castAction static=0x%p service(+0x310)=0x%p service(+0x60)=0x%p",
        label ? label : "cast service",
        staticFields,
        service,
        serviceDispatcher);
    log_line(buffer);
    log_il2cpp_type("CastAction.service(+0x310)", service);
}

bool refresh_rod_reel_transform_binding(const ActionSet& actions, const char* reason) {
    if (!actions.fishingSet || !fishing_set_is_rod_reel(actions)) {
        log_line("rod reel binding refresh skipped: FishingSet is not RodReelRigFishingSet");
        return true;
    }

    void* stored = read_field<void*>(actions.fishingSet, 0x2F0, nullptr);
    if (pointer_is_unity_transform(stored)) {
        char keep[192]{};
        sprintf_s(
            keep,
            "rod reel binding refresh skipped: existing +0x2F0 transform=0x%p reason=%s",
            stored,
            reason ? reason : "<none>");
        log_line(keep);
        return true;
    }

    struct Candidate {
        const char* label;
        void* transform;
    };

    void* childA = fisher_prepare_child_transform(
        actions,
        raw_action_offsets::fisher_prepare_child_name_a_global,
        "Fisher.prepareChildA.refresh");
    void* childB = fisher_prepare_child_transform(
        actions,
        raw_action_offsets::fisher_prepare_child_name_b_global,
        "Fisher.prepareChildB.refresh");

    const Candidate candidates[] = {
        {"Fisher.prepareChildA.refresh", childA},
        {"Fisher.prepareChildB.refresh", childB},
        {"Fisher.transform_0(+0x88).refresh", object_field(actions.fisher, action_offsets::RF4_Client_FishingScene_Fisher_transform_0_field_2)},
        {"FishingSet.transform(+0x30).refresh", object_field(actions.fishingSet, 0x30)},
        {"Rod.transform_0(+0x30).refresh", object_field(actions.rod, action_offsets::RF4_Client_FishingScene_Rod_transform_0_field)},
    };

    const int state = read_field<int>(actions.fishingSet, 0x128);
    if (can_cast_or_hitch(actions) || state == 2 || state == 3) {
        const Candidate directCandidates[] = {
            {"FishingSet.transform(+0x30).direct", object_field(actions.fishingSet, 0x30)},
            {"Rod.transform_0(+0x30).direct", object_field(actions.rod, action_offsets::RF4_Client_FishingScene_Rod_transform_0_field)},
            {"Fisher.prepareChildA.direct", childA},
            {"Fisher.prepareChildB.direct", childB},
            {"Fisher.transform_0(+0x88).direct", object_field(actions.fisher, action_offsets::RF4_Client_FishingScene_Fisher_transform_0_field_2)},
        };
        for (const Candidate& candidate : directCandidates) {
            if (!candidate.transform) {
                continue;
            }
            const bool wrote = write_pointer_field(actions.fishingSet, 0x2F0, candidate.transform);
            char buffer[256]{};
            sprintf_s(
                buffer,
                "rod reel binding refresh direct write while castable: reason=%s candidate=%s transform=0x%p wrote=%s stored=0x%p",
                reason ? reason : "<none>",
                candidate.label,
                candidate.transform,
                wrote ? "true" : "false",
                read_field<void*>(actions.fishingSet, 0x2F0, nullptr));
            log_line(buffer);
            if (wrote) {
                return true;
            }
        }
    }

    for (const Candidate& candidate : candidates) {
        if (!candidate.transform) {
            continue;
        }
        if (prepare_with_transform_candidate(actions, candidate.label, candidate.transform) &&
            read_field<void*>(actions.fishingSet, 0x2F0, nullptr) == candidate.transform) {
            log_line("rod reel binding refresh: method rebound +0x2F0");
            return true;
        }
    }

    for (const Candidate& candidate : candidates) {
        if (!candidate.transform) {
            continue;
        }
        const bool wrote = write_pointer_field(actions.fishingSet, 0x2F0, candidate.transform);
        char buffer[256]{};
        sprintf_s(
            buffer,
            "rod reel binding refresh direct write: reason=%s candidate=%s transform=0x%p wrote=%s stored=0x%p",
            reason ? reason : "<none>",
            candidate.label,
            candidate.transform,
            wrote ? "true" : "false",
            read_field<void*>(actions.fishingSet, 0x2F0, nullptr));
        log_line(buffer);
        if (wrote) {
            return true;
        }
    }

    log_line("rod reel binding refresh failed: no usable transform candidate");
    return false;
}

void cast_info_probe(const ActionSet& actions) {
    log_line("cast info probe started");
    log_actions(actions);
    inspect_cast_target(actions, "cast info raw target", false);
    refresh_rod_reel_transform_binding(actions, "cast info");
    inspect_cast_target(actions, "cast info hydrated target", true);
    log_line("cast info probe finished");
}

bool prepare_cast_entry(const ActionSet& actions) {
    log_line("prepare cast entry started");
    if (can_cast_or_hitch(actions)) {
        log_line("prepare cast entry: already castable");
        return true;
    }

    struct Candidate {
        const char* label;
        void* transform;
    };

    const bool fisherPrepared = prepare_via_fisher(actions);
    if (read_field<int>(actions.fishingSet, 0x128) == 1 &&
        !read_field<void*>(actions.fishingSet, 0x2F0)) {
        log_line("prepare cast entry: Fisher reached state 1, completing RodReel transform binding");
    } else if (read_field<int>(actions.fishingSet, 0x128) != 0) {
        log_line("prepare cast entry: Fisher path changed set state");
        return true;
    } else if (fisherPrepared) {
        log_line("prepare cast entry: Fisher path reported success without visible state change");
        return true;
    }

    void* childA = fisher_prepare_child_transform(
        actions,
        raw_action_offsets::fisher_prepare_child_name_a_global,
        "Fisher.prepareChildA");
    void* childB = fisher_prepare_child_transform(
        actions,
        raw_action_offsets::fisher_prepare_child_name_b_global,
        "Fisher.prepareChildB");

    const Candidate candidates[] = {
        {"Fisher.prepareChildA", childA},
        {"Fisher.prepareChildB", childB},
        {"Fisher.transform_0(+0x88)", object_field(actions.fisher, action_offsets::RF4_Client_FishingScene_Fisher_transform_0_field_2)},
        {"FishingSet.transform(+0x30)", object_field(actions.fishingSet, 0x30)},
        {"Rod.transform_0(+0x30)", object_field(actions.rod, action_offsets::RF4_Client_FishingScene_Rod_transform_0_field)},
        {"RigFromSet.transform(+0x20)", object_field(actions.rigFromSet, lure_complex_offsets::transform)},
    };

    for (const Candidate& candidate : candidates) {
        if (can_cast_or_hitch(actions) ||
            (read_field<int>(actions.fishingSet, 0x128) == 1 &&
             read_field<void*>(actions.fishingSet, 0x2F0) != nullptr)) {
            break;
        }
        if (prepare_with_transform_candidate(actions, candidate.label, candidate.transform)) {
            log_line("prepare cast entry: transform fallback changed set state");
            return true;
        }
    }

    log_line("prepare cast entry finished without state change");
    return can_cast_or_hitch(actions) || read_field<int>(actions.fishingSet, 0x128) == 1;
}

bool enter_castable_state(const ActionSet& actions, const char* reason) {
    if (!actions.fishingSet) {
        log_line("enter castable stopped: missing FishingSet");
        return false;
    }
    if (can_cast_or_hitch(actions)) {
        log_line("enter castable skipped: already castable");
        return true;
    }

    const int beforeState = read_field<int>(actions.fishingSet, 0x128);
    if (beforeState != 1) {
        char skip[160]{};
        sprintf_s(skip, "enter castable skipped: state=%d is not prepared", beforeState);
        log_line(skip);
        return false;
    }

    Vector3f readyPosition{};
    std::string source;
    if (!choose_ready_position(actions, readyPosition, source)) {
        log_line("enter castable stopped: no sane ready Vector3 source");
        return false;
    }

    bool result = false;
    const bool called = call_bool_vec3_rva_safe(
        actions.fishingSet,
        raw_action_offsets::fishing_set_enter_ready_vector,
        readyPosition,
        result);
    Sleep(50);

    char buffer[320]{};
    sprintf_s(
        buffer,
        "enter castable via FishingSet.kogkplpalgb(Vector3): reason=%s source=%s point=%s called=%s result=%s state %d->%d marker=0x%llX canCast=%s",
        reason ? reason : "<none>",
        source.c_str(),
        vector_to_string(readyPosition).c_str(),
        called ? "true" : "false",
        result ? "true" : "false",
        beforeState,
        read_field<int>(actions.fishingSet, 0x128),
        static_cast<unsigned long long>(read_field<unsigned long long>(actions.fishingSet, 0xA4)),
        can_cast_or_hitch(actions) ? "true" : "false");
    log_line(buffer);
    if (called && (result || can_cast_or_hitch(actions))) {
        return true;
    }

    if (read_field<int>(actions.fishingSet, 0x128) == 1) {
        const bool stateCalled = call_void_int_rva_safe(
            actions.fishingSet,
            raw_action_offsets::fishing_set_set_state,
            2);
        Sleep(50);
        char fallback[256]{};
        sprintf_s(
            fallback,
            "enter castable fallback SetState(2): called=%s state=%d marker=0x%llX canCast=%s",
            stateCalled ? "true" : "false",
            read_field<int>(actions.fishingSet, 0x128),
            static_cast<unsigned long long>(read_field<unsigned long long>(actions.fishingSet, 0xA4)),
            can_cast_or_hitch(actions) ? "true" : "false");
        log_line(fallback);
        return stateCalled && can_cast_or_hitch(actions);
    }

    return false;
}

bool pump_until_castable(const ActionSet& actions, DWORD waitMs) {
    const ULONGLONG waitEnd = GetTickCount64() + waitMs;
    int waitTicks = 0;
    while (!can_cast_or_hitch(actions) && GetTickCount64() < waitEnd) {
        const bool setUpdate = call_void0_rva_safe(
            actions.fishingSet,
            raw_action_offsets::fishing_set_update);
        const bool sceneUpdate = call_void0_rva_safe(
            actions.inputController,
            action_offsets::RF4_Client_FishingScene_FishingSceneInputController_Update_0_method);
        ++waitTicks;
        if ((waitTicks % 30) == 0) {
            char tick[224]{};
            sprintf_s(
                tick,
                "wait castable tick=%d setUpdate=%s sceneUpdate=%s state=%d canCast=%s",
                waitTicks,
                setUpdate ? "true" : "false",
                sceneUpdate ? "true" : "false",
                read_field<int>(actions.fishingSet, 0x128),
                can_cast_or_hitch(actions) ? "true" : "false");
            log_line(tick);
        }
        Sleep(16);
    }

    char waitBuffer[192]{};
    sprintf_s(
        waitBuffer,
        "FishingSet wait ready: ticks=%d state=%d marker=0x%llX canCast=%s",
        waitTicks,
        read_field<int>(actions.fishingSet, 0x128),
        static_cast<unsigned long long>(read_field<unsigned long long>(actions.fishingSet, 0xA4)),
        can_cast_or_hitch(actions) ? "true" : "false");
    log_line(waitBuffer);
    return can_cast_or_hitch(actions);
}

bool wait_marker_idle_if_busy(const ActionSet& actions, DWORD waitMs) {
    if (!actions.fishingSet || can_cast_or_hitch(actions) || !cast_marker_busy(actions)) {
        return can_cast_or_hitch(actions);
    }

    const ULONGLONG waitEnd = GetTickCount64() + waitMs;
    int waitTicks = 0;
    while (!can_cast_or_hitch(actions) &&
           cast_marker_busy(actions) &&
           GetTickCount64() < waitEnd) {
        call_void0_rva_safe(actions.fishingSet, raw_action_offsets::fishing_set_update);
        call_void0_rva_safe(
            actions.inputController,
            action_offsets::RF4_Client_FishingScene_FishingSceneInputController_Update_0_method);
        ++waitTicks;
        Sleep(16);
    }

    char buffer[224]{};
    sprintf_s(
        buffer,
        "marker wait: ticks=%d state=%d marker=0x%llX busy=%s canCast=%s",
        waitTicks,
        read_field<int>(actions.fishingSet, 0x128),
        static_cast<unsigned long long>(read_field<unsigned long long>(actions.fishingSet, 0xA4)),
        cast_marker_busy(actions) ? "true" : "false",
        can_cast_or_hitch(actions) ? "true" : "false");
    log_line(buffer);
    return can_cast_or_hitch(actions);
}

bool ensure_cast_ready(const ActionSet& actions, DWORD waitMs = 3000) {
    if (!actions.fishingSet) {
        log_line("ensure cast ready stopped: missing FishingSet");
        return false;
    }

    const int initialState = read_field<int>(actions.fishingSet, 0x128);
    if (!can_cast_or_hitch(actions) && cast_marker_busy(actions)) {
        wait_marker_idle_if_busy(actions, waitMs);
    }

    if (!can_cast_or_hitch(actions)) {
        char prepareBuffer[192]{};
        sprintf_s(
            prepareBuffer,
            "ensure cast ready: initial state=%d/%s canCast=%s",
            initialState,
            fishing_set_state_name(initialState),
            can_cast_or_hitch(actions) ? "true" : "false");
        log_line(prepareBuffer);

        if (initialState == 0) {
            prepare_cast_entry(actions);
        }
        if (!can_cast_or_hitch(actions)) {
            pump_until_castable(actions, waitMs);
        }
        if (!can_cast_or_hitch(actions) && read_field<int>(actions.fishingSet, 0x128) == 1) {
            enter_castable_state(actions, "ensure post-pump fallback");
            if (!can_cast_or_hitch(actions)) {
                pump_until_castable(actions, 600);
            }
        }
    }

    if (can_cast_or_hitch(actions)) {
        refresh_rod_reel_transform_binding(actions, "ensure cast ready final");
        if (!can_cast_or_hitch(actions) && read_field<int>(actions.fishingSet, 0x128) == 1) {
            enter_castable_state(actions, "ensure after binding refresh");
        }
        inspect_cast_target(actions, "ensure cast ready target", true);
    }

    return can_cast_or_hitch(actions);
}

bool direct_powered_cast(const ActionSet& actions, int requestedPower, const char* label) {
    if (!actions.fishingSet) {
        log_line("direct powered cast stopped: missing FishingSet");
        return false;
    }
    if (!can_cast_or_hitch(actions)) {
        log_line("direct powered cast stopped: set is not castable");
        return false;
    }

    refresh_rod_reel_transform_binding(actions, "direct powered cast");
    CastTargetInfo targetInfo =
        inspect_cast_target(actions, "direct powered cast target", true);
    if (!targetInfo.target) {
        log_line("direct powered cast stopped: Rod cast target is null; avoiding marker jam");
        return false;
    }
    const unsigned char targetReady =
        read_field<unsigned char>(targetInfo.target, 0x28, 0);
    const unsigned char targetBlocked =
        read_field<unsigned char>(targetInfo.target, 0x29, 1);
    if (targetReady != 1 || targetBlocked != 0 || !targetInfo.active) {
        if (!repair_cast_target_for_cast(actions, "direct powered cast")) {
            log_line("direct powered cast stopped: cast target repair failed");
            return false;
        }
        targetInfo = inspect_cast_target(actions, "direct powered cast target after repair", true);
    }
    if (!targetInfo.activeReadyCallOk || !targetInfo.activeReady) {
        log_line("direct powered cast stopped: active cast object is not ready");
        return false;
    }

    log_throw_power_state("throw power before direct powered cast");

    double rawFullPower = 0.0;
    int computedPower = 0;
    const bool computed = compute_full_cast_power(actions, rawFullPower, computedPower);
    int castPower = requestedPower > 0
        ? requestedPower
        : (computed ? computedPower : 4);
    if (castPower < 1) {
        castPower = 1;
    }

    bool castResult = false;
    const bool called = call_bool_int_rva_safe(
        actions.fishingSet,
        raw_action_offsets::fishing_set_do_cast_or_hitch_with_power,
        castPower,
        castResult);

    char buffer[256]{};
    sprintf_s(
        buffer,
        "%s direct powered cast: requested=%d rawFull=%.6f computed=%d used=%d called=%s result=%s state=%d lineMarker=0x%llX",
        label ? label : "cast",
        requestedPower,
        rawFullPower,
        computedPower,
        castPower,
        called ? "true" : "false",
        castResult ? "true" : "false",
        read_field<int>(actions.fishingSet, 0x128),
        static_cast<unsigned long long>(read_field<unsigned long long>(actions.fishingSet, 0xA4)));
    log_line(buffer);
    return called && castResult;
}

void force_ready_cast_probe(const ActionSet& actions, int requestedPower = 0) {
    log_line("safe max cast probe started");
    log_actions(actions);

    if (!ensure_cast_ready(actions)) {
        log_line("safe max cast stopped: set is not castable, refusing to force SetState(2)");
        log_actions(actions);
        return;
    }

    direct_powered_cast(actions, requestedPower, "safe max cast");
    log_actions(actions);
    log_line("safe max cast probe finished");
}

void force_ready_cast_auto_probe(const ActionSet& actions) {
    log_line("auto power cast probe started");
    log_actions(actions);

    if (!ensure_cast_ready(actions)) {
        log_line("auto power cast stopped: set is not castable");
        log_actions(actions);
        return;
    }

    double rawFullPower = 0.0;
    int computedPower = 0;
    compute_full_cast_power(actions, rawFullPower, computedPower);
    log_throw_power_state("throw power before auto power cast");
    log_direct_void0("FishingSet.DoCastOrHitchAutoPower", actions.fishingSet, 0xC984C0);
    log_actions(actions);
    log_line("auto power cast probe finished");
}

bool fast_charged_cast_sequence(const ActionSet& actions, float charge, const char* label) {
    if (!actions.fishingSetInput) {
        log_line("fast charged cast stopped: missing FishingSetUserInputController");
        return false;
    }

    charge = std::clamp(charge, 0.05f, 0.98f);
    void* dispatcher = object_field(actions.fishingSetInput, 0x30);
    if (dispatcher && dispatcher_handler_count(dispatcher) < 2) {
        log_line("fast charged cast: dispatcher handlers low, rebinding throw input");
        rebind_throw_input_probe(actions);
    }

    log_cast_service_state(label ? label : "fast charged cast");
    log_throw_power_state("fast charged cast before");
    call_void_bool_rva_safe(
        actions.fishingSetInput,
        raw_action_offsets::fishing_set_throw_power_pressed,
        false);
    call_void_bool_rva_safe(
        actions.fishingSetInput,
        raw_action_offsets::fishing_set_throw_power_pressed,
        true);
    const bool wroteStatic = set_throw_power_state(charge);
    const bool wroteBehavior = set_behavior_float(
        behavior_offsets::throw_power_id,
        charge,
        "fast charged cast prop13");
    set_behavior_raw(
        behavior_offsets::throw_charge_enabled_id,
        1,
        "fast charged cast prop33");
    Sleep(24);

    double rawFullPower = 0.0;
    int computedPower = 0;
    compute_full_cast_power(actions, rawFullPower, computedPower);
    bool castPressed = false;
    const bool castCallOk = call_bool_bool_rva_safe(
        actions.fishingSetInput,
        raw_action_offsets::fishing_set_try_hitch_or_cast_pressed,
        true,
        castPressed);
    Sleep(50);
    bool releaseResult = false;
    const bool releaseCallOk = call_bool_bool_rva_safe(
        actions.fishingSetInput,
        raw_action_offsets::fishing_set_try_hitch_or_cast_pressed,
        false,
        releaseResult);
    call_void_bool_rva_safe(
        actions.fishingSetInput,
        raw_action_offsets::fishing_set_throw_power_pressed,
        false);
    set_behavior_raw(
        behavior_offsets::throw_charge_enabled_id,
        0,
        "fast charged cast prop33 release");

    char buffer[256]{};
    sprintf_s(
        buffer,
        "%s fast charged cast result: charge=%.3f staticWrite=%s behaviorWrite=%s rawFull=%.6f computed=%d castCall=%s castPressed=%s releaseCall=%s state=%d marker=0x%llX canCast=%s",
        label ? label : "cast",
        charge,
        wroteStatic ? "true" : "false",
        wroteBehavior ? "true" : "false",
        rawFullPower,
        computedPower,
        castCallOk ? "true" : "false",
        castPressed ? "true" : "false",
        releaseCallOk ? "true" : "false",
        read_field<int>(actions.fishingSet, 0x128),
        static_cast<unsigned long long>(read_field<unsigned long long>(actions.fishingSet, 0xA4)),
        can_cast_or_hitch(actions) ? "true" : "false");
    log_line(buffer);
    log_throw_power_state("fast charged cast after release");
    return castPressed;
}

void charged_cast_probe(const ActionSet& actions, int chargeTicks = 210) {
    log_line("charged cast probe started");
    log_actions(actions);

    if (!ensure_cast_ready(actions)) {
        log_line("charged cast stopped: set is not castable");
        log_actions(actions);
        return;
    }

    log_throw_power_state("charged cast before charge");
    call_void_bool_rva_safe(
        actions.fishingSetInput,
        raw_action_offsets::fishing_set_throw_power_pressed,
        false);
    call_void_bool_rva_safe(
        actions.fishingSetInput,
        raw_action_offsets::fishing_set_throw_power_pressed,
        true);

    for (int i = 0; i < chargeTicks; ++i) {
        call_void_bool_rva_safe(
            actions.fishingSetInput,
            raw_action_offsets::fishing_set_throw_power_pressed,
            true);
        if ((i + 1) % 30 == 0 || i + 1 == chargeTicks) {
            char label[64]{};
            sprintf_s(label, "charged cast tick %d/%d", i + 1, chargeTicks);
            log_throw_power_state(label);
        }
        Sleep(16);
    }

    double rawFullPower = 0.0;
    int computedPower = 0;
    compute_full_cast_power(actions, rawFullPower, computedPower);
    log_cast_service_state("charged cast");
    bool castPressed = false;
    const bool castCallOk = call_bool_bool_rva_safe(
        actions.fishingSetInput,
        raw_action_offsets::fishing_set_try_hitch_or_cast_pressed,
        true,
        castPressed);
    Sleep(50);
    bool releaseResult = false;
    const bool releaseCallOk = call_bool_bool_rva_safe(
        actions.fishingSetInput,
        raw_action_offsets::fishing_set_try_hitch_or_cast_pressed,
        false,
        releaseResult);
    call_void_bool_rva_safe(
        actions.fishingSetInput,
        raw_action_offsets::fishing_set_throw_power_pressed,
        false);

    char buffer[192]{};
    sprintf_s(
        buffer,
        "charged cast result: ticks=%d rawFull=%.6f computed=%d castCall=%s castPressed=%s releaseCall=%s",
        chargeTicks,
        rawFullPower,
        computedPower,
        castCallOk ? "true" : "false",
        castPressed ? "true" : "false",
        releaseCallOk ? "true" : "false");
    log_line(buffer);
    log_throw_power_state("charged cast after release");
    log_actions(actions);
    log_line("charged cast probe finished");
}

void dispatcher_cast_probe(const ActionSet& actions, int holdTicks = 230) {
    log_line("dispatcher cast probe started");
    log_actions(actions);

    if (!ensure_cast_ready(actions)) {
        log_line("dispatcher cast stopped: set is not castable");
        log_actions(actions);
        return;
    }

    rebind_throw_input_probe(actions);
    void* dispatcher = object_field(actions.fishingSetInput, 0x30);
    if (!dispatcher) {
        log_line("dispatcher cast stopped: missing FishingSetInput dispatcher");
        log_actions(actions);
        return;
    }

    log_dispatcher_state("dispatcher cast before hold", dispatcher);
    dispatch_pressed(dispatcher, false, 0.016f);
    log_throw_power_state("dispatcher cast after initial release");

    bool lastHandled = false;
    for (int i = 0; i < holdTicks; ++i) {
        lastHandled = dispatch_pressed(dispatcher, true, 0.016f);
        if ((i + 1) % 30 == 0 || i + 1 == holdTicks) {
            char label[96]{};
            sprintf_s(label, "dispatcher cast tick %d/%d", i + 1, holdTicks);
            log_throw_power_state(label);

            char buffer[192]{};
            sprintf_s(
                buffer,
                "dispatcher cast tick state: handled=%s state=%d canCast=%s handlers=%d",
                lastHandled ? "true" : "false",
                read_field<int>(actions.fishingSet, 0x128),
                can_cast_or_hitch(actions) ? "true" : "false",
                dispatcher_handler_count(dispatcher));
            log_line(buffer);
        }
        Sleep(16);
    }

    const bool releaseHandled = dispatch_pressed(dispatcher, false, 0.016f);
    Sleep(80);

    char buffer[192]{};
    sprintf_s(
        buffer,
        "dispatcher cast result: ticks=%d lastHoldHandled=%s releaseHandled=%s",
        holdTicks,
        lastHandled ? "true" : "false",
        releaseHandled ? "true" : "false");
    log_line(buffer);
    log_throw_power_state("dispatcher cast after release");
    log_dispatcher_state("dispatcher cast after release", dispatcher);
    log_actions(actions);
    log_line("dispatcher cast probe finished");
}

void scene_dispatcher_cast_probe(const ActionSet& actions, int holdTicks = 230) {
    log_line("scene dispatcher cast probe started");
    log_actions(actions);

    void* dispatcher = object_field(actions.inputController, 0x20);
    if (!dispatcher) {
        log_line("scene dispatcher cast stopped: missing FishingSceneInputController dispatcher");
        log_actions(actions);
        return;
    }

    if (!ensure_cast_ready(actions)) {
        log_line("scene dispatcher cast: direct prepare did not reach castable; trying dispatcher anyway");
    }

    log_dispatcher_state("scene dispatcher before hold", dispatcher);
    const bool initialRelease = dispatch_pressed(dispatcher, false, 0.016f);
    char buffer[224]{};
    sprintf_s(buffer, "scene dispatcher initial release handled=%s", initialRelease ? "true" : "false");
    log_line(buffer);

    bool lastHandled = false;
    for (int i = 0; i < holdTicks; ++i) {
        lastHandled = dispatch_pressed(dispatcher, true, 0.016f);
        if ((i + 1) % 30 == 0 || i + 1 == holdTicks) {
            char label[96]{};
            sprintf_s(label, "scene dispatcher cast tick %d/%d", i + 1, holdTicks);
            log_throw_power_state(label);

            sprintf_s(
                buffer,
                "scene dispatcher cast tick state: handled=%s state=%d canCast=%s handlers=%d marker=0x%llX",
                lastHandled ? "true" : "false",
                read_field<int>(actions.fishingSet, 0x128),
                can_cast_or_hitch(actions) ? "true" : "false",
                dispatcher_handler_count(dispatcher),
                static_cast<unsigned long long>(read_field<unsigned long long>(actions.fishingSet, 0xA4)));
            log_line(buffer);
        }
        Sleep(16);
    }

    const bool releaseHandled = dispatch_pressed(dispatcher, false, 0.016f);
    Sleep(80);
    sprintf_s(
        buffer,
        "scene dispatcher cast result: ticks=%d lastHoldHandled=%s releaseHandled=%s",
        holdTicks,
        lastHandled ? "true" : "false",
        releaseHandled ? "true" : "false");
    log_line(buffer);
    log_throw_power_state("scene dispatcher cast after release");
    log_dispatcher_state("scene dispatcher after release", dispatcher);
    log_actions(actions);
    log_line("scene dispatcher cast probe finished");
}

void max_cast_probe(const ActionSet& actions, int holdTicks = 180) {
    log_line("max cast probe started (physics spin path)");
    log_actions(actions);

    const float requestedDistance = holdTicks >= 170
        ? 0.0f
        : std::clamp(14.0f + (static_cast<float>(holdTicks) * 0.32f), 14.0f, 38.0f);
    physics_spin_cast_probe(actions, requestedDistance, false);
    log_actions(actions);
    log_line("max cast probe finished");
}

struct BotRuntime {
    bool reelActiveGuess{};
    ULONGLONG lastReelToggle{};
    ULONGLONG lastHookPulse{};
    ULONGLONG lastCastAttempt{};
    ULONGLONG lastSoftPrepare{};
    ULONGLONG lastQuickPull{};
    ULONGLONG lastCatchLand{};
    ULONGLONG lastAntiSnag{};
    ULONGLONG lastSlowRetrieve{};
    unsigned int snagStrikes{};
};

BotRuntime g_botRuntime{};

void reset_bot_runtime() {
    g_botRuntime = {};
}

bool bot_pulse_action(const char* label, void* action) {
    const bool ok = game_actions::pulse_input_action(action);
    log_line(std::string("bot action ") + label + (ok ? ": performed" : ": failed"));
    return ok;
}

void bot_ensure_reel_on(const ActionSet& actions, const char* reason, ULONGLONG now) {
    if (g_botRuntime.reelActiveGuess) {
        const int crankTicks = (now - g_botRuntime.lastReelToggle < 1200) ? 25 : 35;
        reel_crank_probe(actions, crankTicks);
        return;
    }
    char buffer[192]{};
    sprintf_s(buffer, "bot reel request: %s", reason);
    log_line(buffer);
    if (bot_pulse_action("toggle_reel", actions.toggleReel)) {
        g_botRuntime.reelActiveGuess = true;
        g_botRuntime.lastReelToggle = now;
    }
    reel_crank_probe(actions, 45);
}

bool telemetry_suggests_stuck(const WorldTelemetry& telemetry) {
    if (telemetry.lineDistanceOk && telemetry.lineDistance <= 2.5) {
        return false;
    }
    if (telemetry.surfaceConnectorActive &&
        (!telemetry.lineDistanceOk || telemetry.lineDistance > 3.0) &&
        (telemetry.surfaceSnagTime > 0.0f ||
         telemetry.surfaceBreakForce > 0.0f ||
         telemetry.surfaceJointSpring > 0.0f)) {
        return true;
    }
    if (telemetry.connectorFlag72 &&
        telemetry.lineDistanceOk &&
        telemetry.lineDistance > 115.0) {
        return true;
    }
    if (telemetry.connectorTimedKnown && !telemetry.connectorTimedOk &&
        telemetry.connectorSpanOk && telemetry.connectorSpan > 2.0f) {
        return true;
    }
    if (telemetry.lineDistanceOk && telemetry.lineDistance > 140.0) {
        return true;
    }
    return false;
}

void autonomous_bot_tick(const ActionSet& actions) {
    il2cpp_runtime::ThreadAttach attach;
    if (!attach.attached()) {
        log_line("bot tick skipped: il2cpp_thread_attach failed");
        return;
    }

    const ULONGLONG now = GetTickCount64();
    log_line("bot tick started");
    WorldTelemetry telemetry = log_world_telemetry(actions, false);

    const int state = read_field<int>(actions.fishingSet, 0x128);
    const bool castable = can_cast_or_hitch(actions);
    char buffer[320]{};
    sprintf_s(
        buffer,
        "bot decision input: state=%d canCast=%s fish=%zu nearest=%s%.2f line=%s%.2f span=%s%.2f reelGuess=%s",
        state,
        castable ? "true" : "false",
        telemetry.fishCount,
        telemetry.nearestFishOk ? "" : "!",
        telemetry.nearestFishOk ? telemetry.nearestFishDistance : -1.0f,
        telemetry.lineDistanceOk ? "" : "!",
        telemetry.lineDistanceOk ? telemetry.lineDistance : -1.0,
        telemetry.connectorSpanOk ? "" : "!",
        telemetry.connectorSpanOk ? telemetry.connectorSpan : -1.0f,
        g_botRuntime.reelActiveGuess ? "on" : "off");
    log_line(buffer);

    const bool referenceIsLure =
        telemetry.referenceLabel.find("Lure") != std::string::npos ||
        telemetry.referenceLabel.find("Rig") != std::string::npos;
    const bool lineOutForFishing =
        telemetry.lineDistanceOk &&
        telemetry.lineDistance > 3.0;
    const bool fishAtLure =
        telemetry.nearestFishOk &&
        referenceIsLure &&
        lineOutForFishing &&
        telemetry.nearestFishDistance <= 3.25f;
    if (fishAtLure) {
        if (now - g_botRuntime.lastHookPulse > 450) {
            bot_pulse_action("start_hooking", actions.startHooking);
            g_botRuntime.lastHookPulse = now;
        }
        Sleep(160);
        quick_pull_probe(actions);
        reset_bot_runtime();
        const ULONGLONG landedAt = GetTickCount64();
        g_botRuntime.lastQuickPull = landedAt;
        g_botRuntime.lastCatchLand = landedAt;
        log_line("bot decision: fish is at lure -> hook + instant land");
        log_line("bot tick finished");
        return;
    }

    const bool catchLandCooldown =
        g_botRuntime.lastCatchLand != 0 &&
        now - g_botRuntime.lastCatchLand < 6500 &&
        (!telemetry.lineDistanceOk || telemetry.lineDistance < 4.5 || state == 0);
    if (catchLandCooldown) {
        log_line("bot decision: recent fish land -> wait before recast");
        log_line("bot tick finished");
        return;
    }

    if (telemetry_suggests_stuck(telemetry) && now - g_botRuntime.lastAntiSnag > 8000) {
        log_line("bot decision: connector looks stuck or overextended -> anti_snag");
        anti_snag_probe(actions, "telemetry stuck");
        g_botRuntime.snagStrikes = 0;
        g_botRuntime.lastAntiSnag = now;
        log_line("bot tick finished");
        return;
    }

    const ULONGLONG timeSinceCast = g_botRuntime.lastCastAttempt != 0
        ? now - g_botRuntime.lastCastAttempt
        : 0;
    const bool waitingForCastFlight =
        state == 3 &&
        g_botRuntime.lastCastAttempt != 0 &&
        timeSinceCast < 7800 &&
        (!telemetry.lineDistanceOk || telemetry.lineDistance < 16.0);
    if (waitingForCastFlight) {
        char waitBuffer[192]{};
        sprintf_s(
            waitBuffer,
            "bot decision: cast is still opening -> wait before retrieve dt=%llums line=%s%.2f",
            static_cast<unsigned long long>(timeSinceCast),
            telemetry.lineDistanceOk ? "" : "!",
            telemetry.lineDistanceOk ? telemetry.lineDistance : -1.0);
        log_line(waitBuffer);
        log_line("bot tick finished");
        return;
    }

    const bool lineIsClose =
        telemetry.lineDistanceOk &&
        telemetry.lineDistance <= 4.0;
    const bool lureCloseEnough =
        telemetry.lineDistanceOk &&
        telemetry.lineDistance <= 4.0 &&
        (!telemetry.connectorSpanOk || telemetry.connectorSpan <= 2.5f);
    if (state == 3 && lureCloseEnough && now - g_botRuntime.lastQuickPull > 900) {
        log_line("bot decision: lure is back at rod but state is still cast -> quick_pull/idle reset");
        quick_pull_probe(actions);
        reset_bot_runtime();
        g_botRuntime.lastQuickPull = now;
        log_line("bot tick finished");
        return;
    }

    const bool spanMeansOut =
        telemetry.connectorSpanOk &&
        telemetry.connectorSpan > 2.5 &&
        !(state == 0 && lineIsClose);
    const bool lureOut =
        (telemetry.lineDistanceOk && telemetry.lineDistance > 4.0) ||
        spanMeansOut ||
        state == 3;
    if (lureOut) {
        const int retrieveTicks = telemetry.nearestFishOk ? 18 : 14;
        RetrieveResult retrieve = spinning_retrieve_probe(actions, retrieveTicks);
        const bool lineMovedEnough =
            retrieve.lineBeforeOk &&
            retrieve.lineAfterOk &&
            (retrieve.lineBefore - retrieve.lineAfter) > 0.22;
        const bool likelySnag =
            retrieve.lineBeforeOk &&
            retrieve.lineAfterOk &&
            retrieve.lineBefore > 4.0 &&
            std::abs(retrieve.lineBefore - retrieve.lineAfter) < 0.16 &&
            retrieve.lureMove < 0.28f &&
            !fishAtLure;
        if (lineMovedEnough || retrieve.lureMove > 0.35f) {
            g_botRuntime.snagStrikes = 0;
        } else if (likelySnag) {
            ++g_botRuntime.snagStrikes;
            char snagBuffer[160]{};
            sprintf_s(
                snagBuffer,
                "bot snag suspicion: strikes=%u line=%.2f->%.2f lureMove=%.2f",
                g_botRuntime.snagStrikes,
                retrieve.lineBefore,
                retrieve.lineAfter,
                retrieve.lureMove);
            log_line(snagBuffer);
        }
        if (g_botRuntime.snagStrikes >= 2 && now - g_botRuntime.lastAntiSnag > 2500) {
            anti_snag_probe(actions, "slow retrieve stalled");
            g_botRuntime.snagStrikes = 0;
            g_botRuntime.lastAntiSnag = now;
        }
        g_botRuntime.lastSlowRetrieve = now;
        log_line(telemetry.nearestFishOk
            ? "bot decision: lure out -> slow spin retrieve toward fish"
            : "bot decision: lure out -> slow spin retrieve/search");
        log_line("bot tick finished");
        return;
    }

    if (now - g_botRuntime.lastCastAttempt > 2200) {
        if (!castable) {
            log_line("bot decision: not castable -> direct physics prepare/cast");
        } else {
            log_line("bot decision: castable -> soft max cast");
        }

        physics_spin_cast_probe(actions, 0.0f, false);
        g_botRuntime.lastCastAttempt = GetTickCount64();
        g_botRuntime.reelActiveGuess = false;
        g_botRuntime.snagStrikes = 0;
        log_line("bot tick finished");
        return;
    }

    log_line("bot decision: cooldown");
    log_line("bot tick finished");
}

void auto_cast_tick(const ActionSet& actions) {
    if (!actions.fishingSet) {
        log_line("auto cast tick: missing FishingSet");
        return;
    }

    log_line("auto cast tick: autonomous bot path");
    autonomous_bot_tick(actions);
}

void cycle_return_cast_probe(const ActionSet& actions) {
    log_line("cycle return/cast started");
    quick_pull_probe(actions);
    Sleep(450);
    const ActionSet refreshed = discover_instances(false);
    max_cast_probe(refreshed);
    log_line("cycle return/cast finished");
}

void dispatcher_pulse_probe(const ActionSet& actions) {
    log_line("dispatcher pulse probe started");
    log_actions(actions);
    rebind_throw_input_probe(actions);
    log_dispatcher_pressed(actions, true, 0.016f);
    Sleep(80);
    log_dispatcher_pressed(actions, false, 0.016f);
    log_actions(actions);
    log_line("dispatcher pulse probe finished");
}

void direct_probe(const ActionSet& actions) {
    log_line("direct probe started");

    log_direct_bool1(
        "FishingSet.TryHitchOrCastPressed",
        actions.fishingSetInput,
        raw_action_offsets::fishing_set_try_hitch_or_cast_pressed,
        true);
    log_direct_bool1(
        "FishingSet.TryHitchOrCastPressed",
        actions.fishingSetInput,
        raw_action_offsets::fishing_set_try_hitch_or_cast_pressed,
        false);
    log_direct_void_bool(
        "FishingSet.ThrowPowerPressed",
        actions.fishingSetInput,
        raw_action_offsets::fishing_set_throw_power_pressed,
        true);
    log_direct_void_bool(
        "FishingSet.ThrowPowerPressed",
        actions.fishingSetInput,
        raw_action_offsets::fishing_set_throw_power_pressed,
        false);

    log_line("direct probe: skipping ResetThrowInput because it clears dispatcher handlers");
    log_direct_void0("FishingSet.idkphhcfgjo", actions.fishingSetInput, 0x8AC430);

    // HandItemInputController handlers.
    for (int value : {-1, 0, 1}) {
        log_direct_void_int("HandItem.jgmojmpanhl", actions.handItemInput, 0x50C680, value);
    }
    for (int key : {0, 97, 100, 102, 114, 323, 324}) {
        log_direct_bool_key("HandItem.cgafklncfgl", actions.handItemInput, 0x50C770, true, key);
        log_direct_bool_key("HandItem.lhaanipmhpd", actions.handItemInput, 0x50C850, true, key);
        log_direct_bool_key("HandItem.lnccgdcmmln", actions.handItemInput, 0x50C990, true, key);
    }

    // ReelUserInputController handlers.
    for (int key : {0, 97, 100, 102, 114, 323, 324}) {
        log_direct_bool_key("Reel.cgafklncfgl", actions.reelInput, 0x6501C0, true, key);
        log_direct_bool_key("Reel.hocjiobfopf", actions.reelInput, 0x650260, true, key);
        log_direct_bool_key("Reel.cmpofcfnifn", actions.reelInput, 0x650770, true, key);
        log_direct_bool_key("Reel.lhaanipmhpd", actions.reelInput, 0x650FA0, true, key);
    }

    log_line("direct probe finished");
}

void direct_cast_probe(const ActionSet& actions) {
    log_line("direct cast probe started");
    game_actions::call_void_bool(
        actions.fishingSetInput,
        raw_action_offsets::fishing_set_throw_power_pressed,
        false);
    for (int i = 0; i < 210; ++i) {
        game_actions::call_void_bool(
            actions.fishingSetInput,
            raw_action_offsets::fishing_set_throw_power_pressed,
            true);
        Sleep(16);
    }
    log_direct_bool1(
        "FishingSet.TryHitchOrCastPressed",
        actions.fishingSetInput,
        raw_action_offsets::fishing_set_try_hitch_or_cast_pressed,
        true);
    Sleep(50);
    log_direct_bool1(
        "FishingSet.TryHitchOrCastPressed",
        actions.fishingSetInput,
        raw_action_offsets::fishing_set_try_hitch_or_cast_pressed,
        false);
    log_direct_void_bool(
        "FishingSet.ThrowPowerPressed",
        actions.fishingSetInput,
        raw_action_offsets::fishing_set_throw_power_pressed,
        false);
    log_line("direct cast probe finished");
}

bool perform_named_action(const std::string& command, const ActionSet& actions) {
    void* action = nullptr;

    auto pulse_debug_action = [](const char* label, uintptr_t fieldOffset) {
        void* inputActions = game_actions::input_system_actions();
        void* debugAction = game_actions::action_at(inputActions, fieldOffset);
        log_action_status(label, debugAction);
        game_actions::enable_input_action(debugAction);
        const bool performed = game_actions::pulse_input_action(debugAction);
        Sleep(100);
        log_action_status(label, debugAction);
        log_line(std::string(label) + (performed ? ": performed" : ": failed"));
        return true;
    };

    if (command == "toggle_reel") {
        action = actions.toggleReel;
    } else if (command == "start_hooking") {
        action = actions.startHooking;
    } else if (command == "switch_throw_mode") {
        action = actions.switchThrowMode;
    } else if (command == "hitch") {
        action = actions.hitch;
    } else if (command == "return_idle") {
        action = actions.returnToIdle;
    } else if (command == "change_throw_distance") {
        action = actions.changeThrowDistance;
    } else if (command == "debug_spawn_fish") {
        return pulse_debug_action(
            "Debug.SpawnFish",
            action_offsets::UnityInput_Generated_InputSystemActions_m_Debug_SpawnFish_field);
    } else if (command == "debug_catch_fish") {
        return pulse_debug_action(
            "Debug.CatchFish",
            action_offsets::UnityInput_Generated_InputSystemActions_m_Debug_CatchFish_field);
    } else if (command == "debug_left_fish") {
        return pulse_debug_action(
            "Debug.LeftFish",
            action_offsets::UnityInput_Generated_InputSystemActions_m_Debug_LeftFish_field);
    } else if (command == "debug_fish_jump") {
        return pulse_debug_action(
            "Debug.FishJump",
            action_offsets::UnityInput_Generated_InputSystemActions_m_Debug_FishJump_field);
    } else if (command == "debug_hitch") {
        return pulse_debug_action(
            "Debug.Hitch",
            action_offsets::UnityInput_Generated_InputSystemActions_m_Debug_Hitch_field);
    } else if (command == "direct_probe") {
        direct_probe(actions);
        return true;
    } else if (command == "fish_wh" || command == "telemetry") {
        fish_wh_probe(actions);
        return true;
    } else if (command == "water_info") {
        water_info_probe(actions);
        return true;
    } else if (command == "bot_tick") {
        autonomous_bot_tick(actions);
        return true;
    } else if (command == "dump_refs") {
        dump_fishing_set_refs(actions);
        return true;
    } else if (command == "rig_info") {
        rig_info_probe(actions);
        return true;
    } else if (command == "cast_info") {
        cast_info_probe(actions);
        return true;
    } else if (command == "repair_cast_target") {
        repair_cast_target_for_cast(actions, "manual command");
        return true;
    } else if (command == "power_info") {
        power_info_probe(actions);
        return true;
    } else if (command == "rebind_throw_input") {
        rebind_throw_input_probe(actions);
        return true;
    } else if (command == "quick_pull") {
        quick_pull_probe(actions);
        return true;
    } else if (command == "reel_crank") {
        reel_crank_probe(actions, 60);
        return true;
    } else if (command == "reel_crank_180") {
        reel_crank_probe(actions, 180);
        return true;
    } else if (command == "spin_retrieve") {
        spinning_retrieve_probe(actions, 18);
        return true;
    } else if (command == "anti_snag") {
        anti_snag_probe(actions, "manual command");
        return true;
    } else if (command == "disable_snag") {
        neutralize_surface_connector(actions, "manual command");
        return true;
    } else if (command == "cycle_return_cast") {
        cycle_return_cast_probe(actions);
        return true;
    } else if (command == "cast_press") {
        log_direct_bool1(
            "FishingSet.TryHitchOrCastPressed",
            actions.fishingSetInput,
            raw_action_offsets::fishing_set_try_hitch_or_cast_pressed,
            true);
        return true;
    } else if (command == "cast_release") {
        log_direct_bool1(
            "FishingSet.TryHitchOrCastPressed",
            actions.fishingSetInput,
            raw_action_offsets::fishing_set_try_hitch_or_cast_pressed,
            false);
        return true;
    } else if (command == "power_press") {
        log_direct_void_bool(
            "FishingSet.ThrowPowerPressed",
            actions.fishingSetInput,
            raw_action_offsets::fishing_set_throw_power_pressed,
            true);
        return true;
    } else if (command == "power_release") {
        log_direct_void_bool(
            "FishingSet.ThrowPowerPressed",
            actions.fishingSetInput,
            raw_action_offsets::fishing_set_throw_power_pressed,
            false);
        return true;
    } else if (command == "reset_throw_input") {
        log_line("reset_throw_input warning: this clears dispatcher handlers; rebinding immediately after reset");
        log_direct_void0("FishingSet.ResetThrowInput", actions.fishingSetInput, 0x8ABEA0);
        rebind_throw_input_probe(actions);
        return true;
    } else if (command == "direct_cast") {
        direct_cast_probe(actions);
        return true;
    } else if (command == "sdk_cast") {
        sdk_cast_probe(actions, 100);
        return true;
    } else if (command == "sdk_cast_min") {
        sdk_cast_probe(actions, 1);
        return true;
    } else if (command == "sdk_auto_cast") {
        sdk_auto_cast_probe(actions);
        return true;
    } else if (command == "force_ready_cast_auto") {
        force_ready_cast_auto_probe(actions);
        return true;
    } else if (command == "max_cast") {
        max_cast_probe(actions);
        return true;
    } else if (command == "max_cast_8") {
        max_cast_probe(actions, 8);
        return true;
    } else if (command == "max_cast_60") {
        max_cast_probe(actions, 60);
        return true;
    } else if (command == "physics_cast") {
        physics_spin_cast_probe(actions, 0.0f, false);
        return true;
    } else if (command == "physics_cast_18") {
        physics_spin_cast_probe(actions, 18.0f, false);
        return true;
    } else if (command == "physics_cast_28") {
        physics_spin_cast_probe(actions, 28.0f, false);
        return true;
    } else if (command == "physics_cast_38") {
        physics_spin_cast_probe(actions, 38.0f, false);
        return true;
    } else if (command == "physics_cast_52") {
        physics_spin_cast_probe(actions, 52.0f, false);
        return true;
    } else if (command == "spin_cast_reel") {
        physics_spin_cast_probe(actions, 0.0f, true);
        return true;
    } else if (command == "scene_update_cast") {
        max_cast_probe(actions);
        return true;
    } else if (command == "scene_cast") {
        scene_dispatcher_cast_probe(actions);
        return true;
    } else if (command == "scene_cast_300") {
        scene_dispatcher_cast_probe(actions, 300);
        return true;
    } else if (command == "dispatcher_cast") {
        dispatcher_cast_probe(actions);
        return true;
    } else if (command == "dispatcher_cast_300") {
        dispatcher_cast_probe(actions, 300);
        return true;
    } else if (command == "charged_cast") {
        charged_cast_probe(actions);
        return true;
    } else if (command == "charged_cast_120") {
        charged_cast_probe(actions, 120);
        return true;
    } else if (command == "charged_cast_300") {
        charged_cast_probe(actions, 300);
        return true;
    } else if (command == "prepare_cast") {
        force_ready_probe(actions);
        return true;
    } else if (command == "force_ready") {
        force_ready_probe(actions);
        return true;
    } else if (command == "force_idle") {
        force_idle_probe(actions);
        return true;
    } else if (command == "force_ready_cast") {
        force_ready_cast_probe(actions);
        return true;
    } else if (command == "force_ready_cast_2") {
        force_ready_cast_probe(actions, 2);
        return true;
    } else if (command == "force_ready_cast_4") {
        force_ready_cast_probe(actions, 4);
        return true;
    } else if (command == "force_ready_cast_10") {
        force_ready_cast_probe(actions, 10);
        return true;
    } else if (command == "force_ready_cast_100") {
        force_ready_cast_probe(actions, 100);
        return true;
    } else if (command == "force_ready_cast_1000") {
        force_ready_cast_probe(actions, 1000);
        return true;
    } else if (command == "force_ready_cast_10000") {
        force_ready_cast_probe(actions, 10000);
        return true;
    } else if (command == "force_ready_cast_100000") {
        force_ready_cast_probe(actions, 100000);
        return true;
    } else if (command == "auto_cast_tick") {
        auto_cast_tick(actions);
        return true;
    } else if (command == "dispatcher_press") {
        log_dispatcher_pressed(actions, true, 0.016f);
        return true;
    } else if (command == "dispatcher_release") {
        log_dispatcher_pressed(actions, false, 0.016f);
        return true;
    } else if (command == "dispatcher_pulse") {
        dispatcher_pulse_probe(actions);
        return true;
    } else {
        return false;
    }

    const bool performed = game_actions::pulse_input_action(action);
    log_line(command + (performed ? ": performed" : ": failed"));
    return true;
}

ActionSet discover_instances(bool verbose = false) {
    il2cpp_runtime::ThreadAttach attach;
    if (!attach.attached()) {
        log_line("discovery skipped: il2cpp_thread_attach failed");
        return {};
    }

    void* input = game_actions::find_fishing_scene_input_controller();
    void* fisher = game_actions::find_fisher();
    void* inputActions = game_actions::input_system_actions();
    void* fishingSetInput = game_actions::find_fishing_set_user_input_controller();
    void* setFromController = object_field(fishingSetInput, 0x40);
    void* setFromFinder = game_actions::find_fishing_set();
    void* set = setFromController ? setFromController : setFromFinder;
    void* rodFromSet = object_field(set, 0x38);
    void* rigFromSet = object_field(set, 0x40);
    void* connectorFromSet = object_field(set, 0x90);
    void* interactiveRodFromSet = object_field(set, 0x100);
    void* rigFromFinder = game_actions::find_any_rig();
    ActionSet actions{
        set,
        rodFromSet,
        rigFromSet,
        connectorFromSet,
        interactiveRodFromSet,
        game_actions::fishing_toggle_reel_action(),
        game_actions::fishing_start_hooking_action(),
        game_actions::fishing_set_switch_throw_mode_action(),
        game_actions::fishing_set_hitch_action(),
        game_actions::fishing_set_return_to_idle_action(),
        game_actions::hand_item_change_throw_distance_action(),
        fishingSetInput,
        game_actions::find_hand_item_input_controller(),
        game_actions::find_reel_user_input_controller(),
        game_actions::find_reel(),
        game_actions::find_fish(),
        rigFromSet ? rigFromSet : rigFromFinder,
        input,
        fisher,
    };

    game_actions::capture_common_instances();
    enable_actions(actions);
    if (verbose) {
        log_ptr("FishingSceneInputController", input);
        log_ptr("FishingSet", set);
        log_ptr("FishingSet.fromController", setFromController);
        log_ptr("FishingSet.fromFinder", setFromFinder);
        log_ptr("Fisher", fisher);
        log_ptr("InputSystemActions", inputActions);
        log_ptr("Fishing.ToggleReel", actions.toggleReel);
        log_ptr("Fishing.StartHooking", actions.startHooking);
        log_ptr("FishingSet.SwitchThrowMode", actions.switchThrowMode);
        log_ptr("FishingSet.Hitch", actions.hitch);
        log_ptr("FishingSet.ReturnToIdle", actions.returnToIdle);
        log_ptr("HandItem.ChangeThrowDistance", actions.changeThrowDistance);
        log_ptr("FishingSetUserInputController", actions.fishingSetInput);
        log_ptr("HandItemInputController", actions.handItemInput);
        log_ptr("ReelUserInputController", actions.reelInput);
        log_ptr("Reel", actions.reel);
        log_ptr("Fish", actions.fish);
        log_ptr("Rig", actions.rig);
        log_ptr("Rig.fromSet", rigFromSet);
        log_ptr("Rig.fromFinder", rigFromFinder);
        log_il2cpp_type("Fisher", fisher);
        log_actions(actions);
        log_callback_lists(inputActions);
    }

    return actions;
}

void command_loop() {
    log_line("pulse v2 action command loop started");
    log_line("command file: il2cpp_action_commands_v2.txt");
    log_line("commands: status, telemetry, fish_wh, water_info, rig_info, cast_info, repair_cast_target, power_info, dump_refs, bot_on, bot_off, bot_tick, debug_spawn_fish, debug_catch_fish, debug_left_fish, debug_fish_jump, debug_hitch, prepare_cast, max_cast, max_cast_8, max_cast_60, physics_cast, physics_cast_18, physics_cast_28, physics_cast_38, physics_cast_52, spin_cast_reel, reel_crank, reel_crank_180, spin_retrieve, anti_snag, disable_snag, scene_update_cast, scene_cast, scene_cast_300, dispatcher_cast, dispatcher_cast_300, rebind_throw_input, force_ready_cast, force_ready_cast_2, force_ready_cast_4, force_ready_cast_10, force_ready_cast_100, force_ready_cast_auto, charged_cast, force_ready_cast_1000, force_ready_cast_10000, quick_pull, cycle_return_cast, auto_cast_on, auto_cast_off, force_ready, force_idle, reset_throw_input, quit, unload");

    bool autoCast = false;
    bool autoCycle = false;
    ULONGLONG nextAutoCastTick = 0;
    for (;;) {
        for (const auto& command : take_commands()) {
            log_line("command received: " + command);
            if (command == "quit") {
                log_line("action command loop stopped");
                return;
            }
            if (command == "unload") {
                log_line("unloading action module");
                InterlockedExchange(&g_unloading, 1);
                if (g_menuWindow) {
                    PostMessageW(g_menuWindow, kCloseMenuForUnloadMessage, 0, 0);
                    for (int i = 0; i < 40 && g_menuWindow; ++i) {
                        Sleep(50);
                    }
                }
                clear_command_file();
                FreeLibraryAndExitThread(g_module, 0);
            }
            if (command == "auto_cast_on" || command == "bot_on") {
                autoCast = true;
                autoCycle = false;
                nextAutoCastTick = 0;
                reset_bot_runtime();
                log_line("autonomous bot loop enabled");
                continue;
            }
            if (command == "auto_cycle_on") {
                autoCycle = true;
                autoCast = false;
                nextAutoCastTick = 0;
                log_line("auto cycle loop enabled");
                continue;
            }
            if (command == "auto_cast_off" || command == "bot_off") {
                autoCast = false;
                autoCycle = false;
                reset_bot_runtime();
                log_line("autonomous bot loop disabled");
                continue;
            }
            const ActionSet actions = discover_instances(command == "status");
            if (command == "status") {
                log_actions(actions);
                continue;
            }
            if (!perform_named_action(command, actions)) {
                log_line("unknown command: " + command);
            }
        }

        if (autoCast || autoCycle) {
            const ULONGLONG now = GetTickCount64();
            if (now >= nextAutoCastTick) {
                const ActionSet actions = discover_instances();
                if (autoCycle) {
                    cycle_return_cast_probe(actions);
                    nextAutoCastTick = now + 6500;
                } else {
                    auto_cast_tick(actions);
                    nextAutoCastTick = now + 1300;
                }
            }
        }
        Sleep(100);
    }
}

DWORD WINAPI worker_thread(void*) {
    clear_log_file();
    log_line("pulse v2 action module loaded");
    clear_command_file();
    discover_instances(true);
    log_line("action module discovery finished");
    command_loop();
    return 0;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        InterlockedExchange(&g_unloading, 0);
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(nullptr, 0, worker_thread, module, 0, nullptr);
        if (thread) {
            CloseHandle(thread);
        }
        HANDLE menu = CreateThread(nullptr, 0, menu_thread, module, 0, nullptr);
        if (menu) {
            CloseHandle(menu);
        }
    }
    return TRUE;
}
