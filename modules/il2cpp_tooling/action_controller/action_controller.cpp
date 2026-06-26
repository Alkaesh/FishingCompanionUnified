#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <commctrl.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <string>

#pragma comment(linker, \
    "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' " \
    "version='6.0.0.0' processorArchitecture='*' " \
    "publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace {

constexpr int kPathEdit = 100;
constexpr int kBrowseButton = 101;
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
constexpr int kPowerPressButton = 212;
constexpr int kPowerReleaseButton = 213;
constexpr int kCastPressButton = 214;
constexpr int kCastReleaseButton = 215;
constexpr int kSdkCastButton = 216;
constexpr int kSdkCastMinButton = 217;
constexpr int kSdkAutoCastButton = 218;
constexpr int kDispatcherPulseButton = 219;
constexpr int kForceReadyButton = 220;
constexpr int kForceReadyCastButton = 221;
constexpr int kAutoCastOnButton = 222;
constexpr int kAutoCastOffButton = 223;
constexpr int kAutoCastTickButton = 224;
constexpr int kDumpRefsButton = 225;
constexpr int kQuickPullButton = 226;
constexpr int kCycleButton = 227;
constexpr int kAutoCycleButton = 228;
constexpr int kRigInfoButton = 229;
constexpr int kCastPower1kButton = 230;
constexpr int kCastPower10kButton = 231;
constexpr int kCastInfoButton = 232;
constexpr int kMessageLabel = 300;

HWND g_pathEdit{};
HWND g_messageLabel{};
HINSTANCE g_instance{};
HFONT g_uiFont{};
HFONT g_headingFont{};
HBRUSH g_backgroundBrush{};

constexpr COLORREF kBackgroundColor = RGB(248, 249, 250);
constexpr int kWindowWidth = 800;
constexpr int kWindowHeight = 620;

std::wstring default_game_dir() {
    wchar_t envPath[MAX_PATH]{};
    size_t required = 0;
    if (_wgetenv_s(&required, envPath, L"RF4_GAME_DIR") == 0 && required > 1) {
        return envPath;
    }

    return L"D:\\SteamLibrary\\steamapps\\common\\RussianFishing4";
}

std::wstring window_text(HWND hwnd) {
    const int length = GetWindowTextLengthW(hwnd);
    if (length <= 0) {
        return {};
    }

    std::wstring text(static_cast<size_t>(length + 1), L'\0');
    const int copied = GetWindowTextW(hwnd, text.data(), length + 1);
    text.resize(static_cast<size_t>(copied));
    return text;
}

void set_message(const std::wstring& message) {
    SetWindowTextW(g_messageLabel, message.c_str());
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

void ensure_fonts(HWND hwnd) {
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

std::filesystem::path command_file() {
    return std::filesystem::path(window_text(g_pathEdit)) / L"il2cpp_action_commands_v2.txt";
}

std::filesystem::path log_file() {
    return std::filesystem::path(window_text(g_pathEdit)) / L"il2cpp_action_module.log";
}

bool write_commands(const std::string& commands) {
    const auto path = command_file();
    std::filesystem::path temp = path;
    temp += L".tmp.";
    temp += std::to_wstring(GetCurrentProcessId());
    temp += L".";
    temp += std::to_wstring(GetTickCount64());

    std::ofstream out(temp, std::ios::out | std::ios::trunc);
    if (!out) {
        set_message(L"Cannot write il2cpp_action_commands_v2.txt. Check game folder path.");
        return false;
    }

    out << commands;
    if (commands.empty() || commands.back() != '\n') {
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
        set_message(L"Cannot publish command file. Check game folder path.");
        return false;
    }

    set_message(L"Sent command to il2cpp_action_commands_v2.txt");
    return true;
}

void browse_folder(HWND owner) {
    BROWSEINFOW info{};
    info.hwndOwner = owner;
    info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    info.lpszTitle = L"Select game folder containing rf4_x64.exe";

    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&info);
    if (!pidl) {
        return;
    }

    wchar_t path[MAX_PATH]{};
    if (SHGetPathFromIDListW(pidl, path)) {
        SetWindowTextW(g_pathEdit, path);
        set_message(L"Game folder selected");
    }
    CoTaskMemFree(pidl);
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
        g_instance,
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
        g_instance,
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
        g_instance,
        nullptr);
    set_control_font(group, g_headingFont);
    return group;
}

void create_ui(HWND hwnd) {
    ensure_fonts(hwnd);

    add_group(hwnd, L"Target", 16, 14, 752, 86);
    add_static(hwnd, L"Game folder", 32, 42, 76, 24, SS_CENTERIMAGE);

    g_pathEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        default_game_dir().c_str(),
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        112,
        40,
        526,
        26,
        hwnd,
        reinterpret_cast<HMENU>(static_cast<intptr_t>(kPathEdit)),
        g_instance,
        nullptr);
    set_control_font(g_pathEdit);

    add_button(hwnd, kBrowseButton, L"Browse", 652, 40, 96, 26);

    add_group(hwnd, L"Cast", 16, 112, 368, 176);
    add_button(hwnd, kForceReadyCastButton, L"Scene cast", 32, 140, 144, 34);
    add_button(hwnd, kQuickPullButton, L"Quick pull", 208, 140, 144, 34);
    add_button(hwnd, kCastPower1kButton, L"Cast 1k", 32, 180, 144, 34);
    add_button(hwnd, kCastPower10kButton, L"Cast 10k", 208, 180, 144, 34);
    add_button(hwnd, kForceReadyButton, L"Force ready", 32, 220, 144, 34);
    add_button(hwnd, kDirectCastButton, L"Direct cast", 208, 220, 144, 34);

    add_group(hwnd, L"Input actions", 400, 112, 368, 176);
    add_button(hwnd, kHitchButton, L"Hitch", 416, 140, 144, 34);
    add_button(hwnd, kStartHookingButton, L"Start hooking", 592, 140, 144, 34);
    add_button(hwnd, kToggleReelButton, L"Toggle reel", 416, 180, 144, 34);
    add_button(hwnd, kSwitchThrowModeButton, L"Switch mode", 592, 180, 144, 34);
    add_button(hwnd, kChangeThrowDistanceButton, L"Change distance", 416, 220, 144, 34);
    add_button(hwnd, kReturnIdleButton, L"Force idle", 592, 220, 144, 34);

    add_group(hwnd, L"Automation", 16, 304, 368, 176);
    add_button(hwnd, kAutoCastOnButton, L"Bot ON", 32, 332, 144, 34);
    add_button(hwnd, kAutoCycleButton, L"Cycle ON", 208, 332, 144, 34);
    add_button(hwnd, kAutoCastTickButton, L"Bot tick", 32, 372, 144, 34);
    add_button(hwnd, kAutoCastOffButton, L"Bot OFF", 208, 372, 144, 34);
    add_button(hwnd, kSdkAutoCastButton, L"SDK auto cast", 32, 412, 144, 34);
    add_button(hwnd, kAutoCastButton, L"Fish WH", 208, 412, 144, 34);

    add_group(hwnd, L"Diagnostics", 400, 304, 368, 176);
    add_button(hwnd, kStatusButton, L"Status", 416, 332, 144, 34);
    add_button(hwnd, kRigInfoButton, L"Rig info", 592, 332, 144, 34);
    add_button(hwnd, kDumpRefsButton, L"Dump refs", 416, 372, 144, 34);
    add_button(hwnd, kCastInfoButton, L"Cast info", 592, 372, 144, 34);
    add_button(hwnd, kSdkCastButton, L"SDK cast", 416, 412, 144, 34);
    add_button(hwnd, kDispatcherPulseButton, L"Dispatcher", 592, 412, 144, 34);

    add_group(hwnd, L"Status", 16, 496, 752, 72);
    g_messageLabel = CreateWindowExW(
        WS_EX_STATICEDGE,
        L"STATIC",
        L"Select the game folder, inject the DLL, then send commands.",
        WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP | SS_CENTERIMAGE,
        32,
        522,
        496,
        28,
        hwnd,
        reinterpret_cast<HMENU>(static_cast<intptr_t>(kMessageLabel)),
        g_instance,
        nullptr);
    set_control_font(g_messageLabel);

    add_button(hwnd, kOpenLogButton, L"Open log", 544, 520, 96, 32);
    add_button(hwnd, kQuitButton, L"Unload DLL", 652, 520, 96, 32);
}

void handle_command(HWND hwnd, int id) {
    switch (id) {
    case kBrowseButton:
        browse_folder(hwnd);
        break;
    case kStatusButton:
        write_commands("status");
        break;
    case kHitchButton:
        write_commands("hitch");
        break;
    case kStartHookingButton:
        write_commands("start_hooking");
        break;
    case kToggleReelButton:
        write_commands("toggle_reel");
        break;
    case kSwitchThrowModeButton:
        write_commands("switch_throw_mode");
        break;
    case kChangeThrowDistanceButton:
        write_commands("change_throw_distance");
        break;
    case kReturnIdleButton:
        write_commands("force_idle");
        break;
    case kQuickPullButton:
        write_commands("quick_pull");
        break;
    case kCycleButton:
        write_commands("cycle_return_cast");
        break;
    case kAutoCycleButton:
        write_commands("auto_cycle_on");
        break;
    case kDumpRefsButton:
        write_commands("dump_refs");
        break;
    case kRigInfoButton:
        write_commands("rig_info");
        break;
    case kCastInfoButton:
        write_commands("cast_info");
        break;
    case kAutoCastButton:
        write_commands("fish_wh");
        break;
    case kDirectProbeButton:
        write_commands("direct_probe");
        break;
    case kDirectCastButton:
        write_commands("direct_cast");
        break;
    case kPowerPressButton:
        write_commands("power_press");
        break;
    case kPowerReleaseButton:
        write_commands("power_release");
        break;
    case kCastPressButton:
        write_commands("cast_press");
        break;
    case kCastReleaseButton:
        write_commands("cast_release");
        break;
    case kSdkCastButton:
        write_commands("sdk_cast");
        break;
    case kSdkCastMinButton:
        write_commands("sdk_cast_min");
        break;
    case kSdkAutoCastButton:
        write_commands("sdk_auto_cast");
        break;
    case kDispatcherPulseButton:
        write_commands("dispatcher_pulse");
        break;
    case kForceReadyButton:
        write_commands("force_ready");
        break;
    case kForceReadyCastButton:
        write_commands("max_cast");
        break;
    case kCastPower1kButton:
        write_commands("force_ready_cast_1000");
        break;
    case kCastPower10kButton:
        write_commands("force_ready_cast_10000");
        break;
    case kAutoCastTickButton:
        write_commands("bot_tick");
        break;
    case kAutoCastOnButton:
        write_commands("bot_on");
        break;
    case kAutoCastOffButton:
        write_commands("bot_off");
        break;
    case kOpenLogButton:
        ShellExecuteW(hwnd, L"open", log_file().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        break;
    case kQuitButton:
        write_commands("unload");
        break;
    default:
        break;
    }
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE:
        create_ui(hwnd);
        return 0;
    case WM_COMMAND:
        handle_command(hwnd, LOWORD(wparam));
        return 0;
    case WM_CTLCOLORDLG:
        return reinterpret_cast<LRESULT>(g_backgroundBrush);
    case WM_CTLCOLORSTATIC:
        SetBkMode(reinterpret_cast<HDC>(wparam), TRANSPARENT);
        return reinterpret_cast<LRESULT>(g_backgroundBrush);
    case WM_DESTROY:
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

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    g_instance = instance;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    INITCOMMONCONTROLSEX commonControls{
        sizeof(commonControls),
        ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES
    };
    InitCommonControlsEx(&commonControls);

    WNDCLASSW wc{};
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.lpszClassName = L"Il2CppActionControllerWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    g_backgroundBrush = CreateSolidBrush(kBackgroundColor);
    wc.hbrBackground = g_backgroundBrush
        ? g_backgroundBrush
        : reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"RF4 Action Controller",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        kWindowWidth,
        kWindowHeight,
        nullptr,
        nullptr,
        instance,
        nullptr);

    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CoUninitialize();
    return 0;
}
