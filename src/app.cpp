#include "app.h"

#include <commctrl.h>
#include <shellapi.h>
#include <windowsx.h>

#include <cwctype>
#include <utility>

#include "resource.h"
#include "settings_dialog.h"

namespace pawse {
namespace {

constexpr wchar_t kMainWindowClassName[] = L"Pawse.MainWindow";
constexpr wchar_t kSingleInstanceMutexName[] = L"Local\\Pawse.SingleInstance";
constexpr UINT kTrayIconId = 1;
constexpr UINT_PTR kTimerAutoUnlock = 1;
constexpr UINT_PTR kTimerHelloTimeout = 2;
// The Windows Hello prompt lets the keyboard through, so it must not stay open forever.
constexpr UINT kHelloTimeoutMilliseconds = 60 * 1000;

enum MenuCommand : UINT {
    kCommandLockKeyboard = 40001,
    kCommandLockMouse,
    kCommandLockBoth,
    kCommandSettings,
    kCommandExit,
};

constexpr UINT kLockCommands[kBindingCount] = {kCommandLockKeyboard, kCommandLockMouse,
                                               kCommandLockBoth};
constexpr const wchar_t* kLockMenuLabels[kBindingCount] = {L"Lock keyboard", L"Lock mouse",
                                                           L"Lock keyboard and mouse"};

std::wstring DescribeDevices(unsigned devices) {
    switch (devices) {
        case kLockKeyboard:
            return L"keyboard";
        case kLockMouse:
            return L"mouse";
        case kLockBoth:
            return L"keyboard and mouse";
        default:
            return L"nothing";
    }
}

std::wstring Capitalize(std::wstring text) {
    if (!text.empty()) {
        text[0] = static_cast<wchar_t>(std::towupper(text[0]));
    }
    return text;
}

// The binding that unlocks exactly these devices.
size_t GetUnlockBinding(unsigned locked_devices) {
    switch (locked_devices) {
        case kLockKeyboard:
            return kBindingKeyboard;
        case kLockMouse:
            return kBindingMouse;
        default:
            return kBindingBoth;
    }
}

template <size_t Capacity>
void CopyText(wchar_t (&destination)[Capacity], const std::wstring& text) {
    wcsncpy_s(destination, text.c_str(), _TRUNCATE);
}

}  // namespace

int App::Run(HINSTANCE instance, bool started_at_sign_in) {
    instance_ = instance;
    show_settings_message_ = RegisterWindowMessageW(L"Pawse.ShowSettings");

    const HANDLE single_instance_mutex = CreateMutexW(nullptr, FALSE, kSingleInstanceMutexName);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Starting Pawse again opens the settings of the copy that is already running.
        if (const HWND running_window = FindWindowW(kMainWindowClassName, nullptr)) {
            AllowSetForegroundWindow(ASFW_ANY);
            PostMessageW(running_window, show_settings_message_, 0, 0);
        }
        if (single_instance_mutex != nullptr) {
            CloseHandle(single_instance_mutex);
        }
        return 0;
    }

    INITCOMMONCONTROLSEX common_controls = {
        sizeof(common_controls), ICC_HOTKEY_CLASS | ICC_UPDOWN_CLASS | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&common_controls);
    settings_ = LoadSettings();
    if (IsStartWithWindowsEnabled()) {
        SetStartWithWindows(true);  // keeps the sign-in entry pointing at this copy of Pawse
    }
    LoadIconMetric(instance, MAKEINTRESOURCEW(IDI_PAWSE), LIM_SMALL, &icon_unlocked_);
    LoadIconMetric(instance, MAKEINTRESOURCEW(IDI_PAWSE_LOCKED), LIM_SMALL, &icon_locked_);
    taskbar_created_message_ = RegisterWindowMessageW(L"TaskbarCreated");

    if (!CreateMainWindow() || !banner_.Create(instance) || !input_blocker_.Start(window_)) {
        MessageBoxW(nullptr, L"Pawse couldn't start.", L"Pawse", MB_ICONERROR | MB_OK);
        return 1;
    }
    AddTrayIcon();
    const std::wstring refused_bindings = RegisterHotkeys();
    RefreshHelloAvailability();
    if (!refused_bindings.empty()) {
        ShowNotification(
            L"Some key bindings are taken",
            refused_bindings + L" is used by another app. Choose a different binding in Settings.",
            true);
    } else if (!started_at_sign_in) {
        ShowNotification(L"Pawse is running",
                         L"Press " + FormatKeyBinding(settings_.bindings[kBindingBoth]) +
                             L" to lock the keyboard and mouse. Click the paw icon for more.",
                         false);
    }

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    input_blocker_.Stop();
    DestroyIcon(icon_unlocked_);
    DestroyIcon(icon_locked_);
    if (single_instance_mutex != nullptr) {
        CloseHandle(single_instance_mutex);
    }
    return static_cast<int>(message.wParam);
}

void App::RefreshHelloAvailability() { QueryHelloAvailability(window_); }

std::wstring App::ApplySettings(const Settings& settings, bool start_with_windows) {
    const Settings previous_settings = settings_;
    settings_ = settings;
    // Try the new bindings now; the settings dialog registers them for good when it closes.
    const std::wstring refused_bindings = RegisterHotkeys();
    UnregisterHotkeys();
    if (!refused_bindings.empty()) {
        settings_ = previous_settings;
        return L"Windows didn't accept " + refused_bindings +
               L". Another app probably uses it. Choose a different key binding.";
    }
    if (!SaveSettings(settings_)) {
        ShowNotification(L"Settings not saved",
                         L"Pawse uses the new settings now but couldn't save them for next time.",
                         true);
    }
    if (!SetStartWithWindows(start_with_windows)) {
        ShowNotification(L"Start with Windows not changed",
                         L"Pawse couldn't change whether it starts when you sign in.", true);
    }
    ApplyLockState();
    return {};
}

LRESULT CALLBACK App::WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        auto* app = static_cast<App*>(create->lpCreateParams);
        app->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    auto* app = reinterpret_cast<App*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (app == nullptr) {
        return DefWindowProcW(window, message, wparam, lparam);
    }
    return app->HandleMessage(message, wparam, lparam);
}

LRESULT App::HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == taskbar_created_message_ && message != 0) {
        AddTrayIcon();  // Explorer restarted and lost the icon
        return 0;
    }
    if (message == show_settings_message_ && message != 0) {
        OpenSettings();
        return 0;
    }

    switch (message) {
        case kMessageTrayIcon:
            switch (LOWORD(lparam)) {
                case NIN_SELECT:
                case NIN_KEYSELECT:
                case WM_CONTEXTMENU:
                    ShowTrayMenu(POINT{GET_X_LPARAM(wparam), GET_Y_LPARAM(wparam)});
                    break;
                default:
                    break;
            }
            return 0;
        case WM_COMMAND:
            HandleCommand(LOWORD(wparam));
            return 0;
        case WM_HOTKEY:
            if (wparam >= 1 && wparam <= kBindingCount) {
                OnBindingPressed(wparam - 1);
            }
            return 0;
        case kMessageBindingPressed:
            if (wparam < kBindingCount) {
                OnBindingPressed(wparam);
            }
            return 0;
        case kMessageHookFailed:
            OnHooksFailed(static_cast<unsigned>(wparam));
            return 0;
        case kMessageHelloResult:
            if (hello_verifier_.Finish(lparam)) {
                FinishUnlockAttempt(static_cast<HelloResult>(wparam));
            }
            return 0;
        case kMessageHelloAvailability:
            hello_availability_ =
                wparam != 0 ? HelloAvailability::kAvailable : HelloAvailability::kUnavailable;
            if (settings_dialog_ != nullptr) {
                PostMessageW(settings_dialog_, kMessageHelloAvailability, wparam, 0);
            }
            UpdateBanner();
            return 0;
        case WM_TIMER:
            if (wparam == kTimerAutoUnlock) {
                OnAutoUnlockTimer();
            } else if (wparam == kTimerHelloTimeout) {
                hello_verifier_.Abandon();
                FinishUnlockAttempt(HelloResult::kCanceled);
            }
            return 0;
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
            return 0;  // keeps the Alt tap in BringMainWindowToFront from opening a menu
        case WM_DESTROY:
            hello_verifier_.Abandon();
            input_blocker_.Update(kLockNone, settings_.bindings);
            UnregisterHotkeys();
            RemoveTrayIcon();
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(window_, message, wparam, lparam);
    }
}

bool App::CreateMainWindow() {
    WNDCLASSEXW window_class = {sizeof(window_class)};
    window_class.lpfnWndProc = WindowProc;
    window_class.hInstance = instance_;
    window_class.hIcon = LoadIconW(instance_, MAKEINTRESOURCEW(IDI_PAWSE));
    window_class.lpszClassName = kMainWindowClassName;
    if (RegisterClassExW(&window_class) == 0) {
        return false;
    }
    // A fully transparent 1x1 window. It is shown only while the Windows Hello prompt is open,
    // because the prompt needs an owner window that is in front.
    CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TOPMOST, kMainWindowClassName,
                    L"Pawse", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, instance_, this);
    if (window_ == nullptr) {
        return false;
    }
    SetLayeredWindowAttributes(window_, 0, 0, LWA_ALPHA);
    return true;
}

void App::AddTrayIcon() {
    NOTIFYICONDATAW data = {sizeof(data)};
    data.hWnd = window_;
    data.uID = kTrayIconId;
    data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    data.uCallbackMessage = kMessageTrayIcon;
    data.hIcon = icon_unlocked_;
    CopyText(data.szTip, L"Pawse");
    Shell_NotifyIconW(NIM_ADD, &data);
    data.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &data);
    UpdateTrayIcon();
}

void App::UpdateTrayIcon() {
    NOTIFYICONDATAW data = {sizeof(data)};
    data.hWnd = window_;
    data.uID = kTrayIconId;
    data.uFlags = NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    const bool locked = locked_devices_ != kLockNone;
    data.hIcon = locked ? icon_locked_ : icon_unlocked_;
    CopyText(data.szTip, locked ? L"Pawse: " + DescribeDevices(locked_devices_) + L" locked"
                                : std::wstring(L"Pawse: unlocked"));
    Shell_NotifyIconW(NIM_MODIFY, &data);
}

void App::RemoveTrayIcon() {
    NOTIFYICONDATAW data = {sizeof(data)};
    data.hWnd = window_;
    data.uID = kTrayIconId;
    Shell_NotifyIconW(NIM_DELETE, &data);
}

void App::ShowNotification(const std::wstring& title, const std::wstring& text, bool is_warning) {
    NOTIFYICONDATAW data = {sizeof(data)};
    data.hWnd = window_;
    data.uID = kTrayIconId;
    data.uFlags = NIF_INFO;
    data.dwInfoFlags = (is_warning ? NIIF_WARNING : NIIF_INFO) | NIIF_RESPECT_QUIET_TIME;
    CopyText(data.szInfoTitle, title);
    CopyText(data.szInfo, text);
    Shell_NotifyIconW(NIM_MODIFY, &data);
}

void App::ShowTrayMenu(POINT anchor) {
    const HMENU menu = CreatePopupMenu();
    const bool dialog_open = settings_dialog_ != nullptr;
    for (size_t index = 0; index < kBindingCount; ++index) {
        const unsigned devices = kBindingDevices[index];
        const bool already_locked = (locked_devices_ & devices) == devices;
        UINT flags = MF_STRING;
        if (already_locked) {
            flags |= MF_CHECKED;
        }
        if (already_locked || dialog_open) {
            flags |= MF_GRAYED;
        }
        const std::wstring label = std::wstring(kLockMenuLabels[index]) + L"\t" +
                                   FormatKeyBinding(settings_.bindings[index]);
        AppendMenuW(menu, flags, kLockCommands[index], label.c_str());
    }
    // Unlocking is only possible with a key binding, so a cat can't click its way out.
    const UINT locked_flag = locked_devices_ != kLockNone ? MF_GRAYED : 0;
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | locked_flag, kCommandSettings, L"Settings…");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | locked_flag, kCommandExit, L"Exit Pawse");

    SetForegroundWindow(window_);
    const UINT alignment = GetSystemMetrics(SM_MENUDROPALIGNMENT) ? TPM_RIGHTALIGN : TPM_LEFTALIGN;
    TrackPopupMenuEx(menu, alignment | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON, anchor.x, anchor.y,
                     window_, nullptr);
    PostMessageW(window_, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

void App::HandleCommand(UINT command) {
    for (size_t index = 0; index < kBindingCount; ++index) {
        if (command == kLockCommands[index]) {
            Lock(kBindingDevices[index]);
            return;
        }
    }
    if (command == kCommandSettings) {
        OpenSettings();
    } else if (command == kCommandExit && locked_devices_ == kLockNone) {
        DestroyWindow(window_);
    }
}

void App::OpenSettings() {
    if (settings_dialog_ != nullptr) {
        SetForegroundWindow(settings_dialog_);
        return;
    }
    if (locked_devices_ != kLockNone) {
        return;
    }
    // While the dialog is open, the hotkey boxes need to receive the key bindings themselves.
    UnregisterHotkeys();
    ShowSettingsDialog(instance_, *this);
    const std::wstring refused_bindings = RegisterHotkeys();
    if (!refused_bindings.empty()) {
        ShowNotification(L"Some key bindings are taken",
                         refused_bindings + L" is used by another app.", true);
    }
}

std::wstring App::RegisterHotkeys() {
    UnregisterHotkeys();
    std::wstring refused_bindings;
    for (size_t index = 0; index < kBindingCount; ++index) {
        const KeyBinding& binding = settings_.bindings[index];
        if (binding.IsEmpty()) {
            continue;
        }
        if (!RegisterHotKey(window_, static_cast<int>(index + 1), binding.modifiers | MOD_NOREPEAT,
                            binding.virtual_key)) {
            if (!refused_bindings.empty()) {
                refused_bindings += L", ";
            }
            refused_bindings += FormatKeyBinding(binding);
        }
    }
    return refused_bindings;
}

void App::UnregisterHotkeys() {
    for (size_t index = 0; index < kBindingCount; ++index) {
        UnregisterHotKey(window_, static_cast<int>(index + 1));
    }
}

void App::OnBindingPressed(size_t binding_index) {
    if (hello_verifier_.IsActive() || settings_dialog_ != nullptr) {
        return;
    }
    const unsigned target_devices = kBindingDevices[binding_index];
    const unsigned locked_target_devices = locked_devices_ & target_devices;
    if (locked_target_devices != kLockNone) {
        RequestUnlock(locked_target_devices);
    } else {
        Lock(target_devices);
    }
}

void App::Lock(unsigned devices) {
    const unsigned newly_locked_devices = devices & ~locked_devices_;
    if (newly_locked_devices == kLockNone) {
        return;
    }
    if (locked_devices_ == kLockNone && settings_.auto_unlock_minutes > 0) {
        SetTimer(window_, kTimerAutoUnlock, settings_.auto_unlock_minutes * 60 * 1000, nullptr);
    }
    locked_devices_ |= newly_locked_devices;
    ApplyLockState();
}

void App::RequestUnlock(unsigned devices) {
    const bool use_windows_hello =
        settings_.require_windows_hello && hello_availability_ != HelloAvailability::kUnavailable;
    if (!use_windows_hello) {
        Unlock(devices);
        return;
    }
    pending_unlock_devices_ = devices;
    BringMainWindowToFront();
    const std::wstring prompt = L"Unlock the " + DescribeDevices(devices) + L" locked by Pawse.";
    if (!hello_verifier_.Begin(window_, prompt, window_)) {
        FinishUnlockAttempt(HelloResult::kUnavailable);
        return;
    }
    SetTimer(window_, kTimerHelloTimeout, kHelloTimeoutMilliseconds, nullptr);
    ApplyLockState();  // lets the keyboard through for the PIN
}

void App::Unlock(unsigned devices) {
    locked_devices_ &= ~devices;
    if (locked_devices_ == kLockNone) {
        KillTimer(window_, kTimerAutoUnlock);
    }
    ApplyLockState();
}

void App::FinishUnlockAttempt(HelloResult result) {
    KillTimer(window_, kTimerHelloTimeout);
    ShowWindow(window_, SW_HIDE);
    const unsigned devices = std::exchange(pending_unlock_devices_, kLockNone);
    switch (result) {
        case HelloResult::kVerified:
            Unlock(devices);
            break;
        case HelloResult::kUnavailable:
            // Never trap the user: without Windows Hello, the key binding is enough.
            hello_availability_ = HelloAvailability::kUnavailable;
            Unlock(devices);
            ShowNotification(L"Unlocked without Windows Hello",
                             L"Windows Hello isn't available, so the key binding alone unlocked "
                             L"the " +
                                 DescribeDevices(devices) + L".",
                             true);
            break;
        case HelloResult::kFailed:
        case HelloResult::kCanceled:
            ApplyLockState();  // stays locked; blocks the keyboard again
            break;
    }
}

void App::OnHooksFailed(unsigned devices) {
    const unsigned affected_devices = devices & locked_devices_;
    if (affected_devices == kLockNone) {
        return;
    }
    if (hello_verifier_.IsActive()) {
        hello_verifier_.Abandon();
        KillTimer(window_, kTimerHelloTimeout);
        ShowWindow(window_, SW_HIDE);
        pending_unlock_devices_ = kLockNone;
    }
    Unlock(affected_devices);
    ShowNotification(L"Pawse unlocked the " + DescribeDevices(affected_devices),
                     L"Windows stopped Pawse from blocking input, so it unlocked rather than "
                     L"risk locking you out.",
                     true);
}

void App::OnAutoUnlockTimer() {
    KillTimer(window_, kTimerAutoUnlock);
    if (hello_verifier_.IsActive()) {
        hello_verifier_.Abandon();
        KillTimer(window_, kTimerHelloTimeout);
        ShowWindow(window_, SW_HIDE);
        pending_unlock_devices_ = kLockNone;
    }
    const unsigned devices = locked_devices_;
    if (devices == kLockNone) {
        return;
    }
    Unlock(devices);
    ShowNotification(L"Pawse unlocked automatically",
                     L"The " + DescribeDevices(devices) + L" unlocked after " +
                         std::to_wstring(settings_.auto_unlock_minutes) +
                         L" minutes, as set in Settings.",
                     false);
}

void App::ApplyLockState() {
    unsigned blocked_devices = locked_devices_;
    if (hello_verifier_.IsActive()) {
        blocked_devices &= ~kLockKeyboard;
    }
    input_blocker_.Update(blocked_devices, settings_.bindings);
    UpdateTrayIcon();
    UpdateBanner();
}

void App::UpdateBanner() {
    if (!settings_.show_banner || locked_devices_ == kLockNone) {
        banner_.Hide();
        return;
    }
    if (hello_verifier_.IsActive()) {
        banner_.Show(
            L"Waiting for Windows Hello",
            L"Confirm it's you to unlock the " + DescribeDevices(pending_unlock_devices_) + L".");
        return;
    }
    const std::wstring title = Capitalize(DescribeDevices(locked_devices_)) +
                               (locked_devices_ == kLockBoth ? L" are locked" : L" is locked");
    std::wstring detail = L"Press " +
                          FormatKeyBinding(settings_.bindings[GetUnlockBinding(locked_devices_)]) +
                          L" to unlock";
    if (settings_.require_windows_hello && hello_availability_ != HelloAvailability::kUnavailable) {
        detail += L", then confirm with Windows Hello";
    }
    banner_.Show(title, detail + L".");
}

bool App::BringMainWindowToFront() {
    const HMONITOR monitor = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO monitor_info = {sizeof(monitor_info)};
    GetMonitorInfoW(monitor, &monitor_info);
    const RECT& work_area = monitor_info.rcWork;
    SetWindowPos(window_, HWND_TOPMOST, (work_area.left + work_area.right) / 2,
                 (work_area.top + work_area.bottom) / 2, 1, 1, SWP_SHOWWINDOW);
    if (SetForegroundWindow(window_) && GetForegroundWindow() == window_) {
        return true;
    }
    // Windows only lets the app that received the last input take the foreground. A tagged
    // Alt tap, which Pawse's own hooks let through, counts as that input.
    INPUT alt_tap[2] = {};
    alt_tap[0].type = INPUT_KEYBOARD;
    alt_tap[0].ki.wVk = VK_MENU;
    alt_tap[0].ki.dwExtraInfo = kPawseInputTag;
    alt_tap[1] = alt_tap[0];
    alt_tap[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &alt_tap[0], sizeof(INPUT));
    SetForegroundWindow(window_);
    SendInput(1, &alt_tap[1], sizeof(INPUT));
    return GetForegroundWindow() == window_;
}

}  // namespace pawse
