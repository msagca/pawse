#include "input_blocker.h"

#include <future>
#include <memory>

namespace pawse {
namespace {

constexpr UINT kHookMessageUpdate = WM_APP + 100;
constexpr UINT kWatchdogIntervalMilliseconds = 1000;
// IsHungAppWindow already waits about 5 seconds; this many failed checks in a row on top of
// that removes the hooks.
constexpr int kHungChecksBeforeRelease = 3;

enum MouseButton : int {
    kMouseButtonLeft,
    kMouseButtonRight,
    kMouseButtonMiddle,
    kMouseButtonX1,
    kMouseButtonX2,
    kMouseButtonCount,
};

struct UpdateRequest {
    unsigned blocked_devices = kLockNone;
    std::array<KeyBinding, kBindingCount> bindings{};
};

// Everything the hook callbacks use. Only the hook thread touches it.
struct HookThreadState {
    HWND notify_window = nullptr;
    HHOOK keyboard_hook = nullptr;
    HHOOK mouse_hook = nullptr;
    std::array<KeyBinding, kBindingCount> bindings{};
    // Keys the hook has seen go down and not yet up, to match bindings.
    std::array<bool, 256> key_is_down{};
    // Keys and buttons that were already held when blocking started. Their release is let
    // through so Windows doesn't think they are stuck down after unlocking.
    std::array<bool, 256> pass_key_release{};
    std::array<bool, kMouseButtonCount> pass_button_release{};
    int hung_checks = 0;
};

HookThreadState g_hook_state;

bool IsTaggedPawseInput(ULONG_PTR extra_info) { return extra_info == kPawseInputTag; }

bool IsModifierKey(UINT virtual_key) {
    switch (virtual_key) {
        case VK_SHIFT:
        case VK_CONTROL:
        case VK_MENU:
        case VK_LSHIFT:
        case VK_RSHIFT:
        case VK_LCONTROL:
        case VK_RCONTROL:
        case VK_LMENU:
        case VK_RMENU:
        case VK_LWIN:
        case VK_RWIN:
            return true;
        default:
            return false;
    }
}

bool IsMouseButtonKey(UINT virtual_key) {
    return virtual_key == VK_LBUTTON || virtual_key == VK_RBUTTON || virtual_key == VK_MBUTTON ||
           virtual_key == VK_XBUTTON1 || virtual_key == VK_XBUTTON2 || virtual_key == VK_CANCEL;
}

UINT GetHeldModifiers(const std::array<bool, 256>& key_is_down) {
    UINT modifiers = 0;
    if (key_is_down[VK_LCONTROL] || key_is_down[VK_RCONTROL]) {
        modifiers |= MOD_CONTROL;
    }
    if (key_is_down[VK_LMENU] || key_is_down[VK_RMENU]) {
        modifiers |= MOD_ALT;
    }
    if (key_is_down[VK_LSHIFT] || key_is_down[VK_RSHIFT]) {
        modifiers |= MOD_SHIFT;
    }
    if (key_is_down[VK_LWIN] || key_is_down[VK_RWIN]) {
        modifiers |= MOD_WIN;
    }
    return modifiers;
}

LRESULT CALLBACK KeyboardHookProc(int code, WPARAM message, LPARAM data) {
    if (code != HC_ACTION) {
        return CallNextHookEx(nullptr, code, message, data);
    }
    const auto& event = *reinterpret_cast<const KBDLLHOOKSTRUCT*>(data);
    if (IsTaggedPawseInput(event.dwExtraInfo)) {
        return CallNextHookEx(nullptr, code, message, data);
    }

    HookThreadState& state = g_hook_state;
    const UINT virtual_key = event.vkCode & 0xFF;
    if (event.flags & LLKHF_UP) {
        state.key_is_down[virtual_key] = false;
        if (state.pass_key_release[virtual_key]) {
            state.pass_key_release[virtual_key] = false;
            return CallNextHookEx(nullptr, code, message, data);
        }
        return 1;
    }

    const bool is_auto_repeat = state.key_is_down[virtual_key];
    state.key_is_down[virtual_key] = true;
    if (!is_auto_repeat && !IsModifierKey(virtual_key)) {
        const KeyBinding pressed{GetHeldModifiers(state.key_is_down), virtual_key};
        for (size_t index = 0; index < kBindingCount; ++index) {
            if (state.bindings[index] == pressed) {
                PostMessageW(state.notify_window, kMessageBindingPressed, index, 0);
                break;
            }
        }
    }
    return 1;
}

int GetReleasedButton(WPARAM message, const MSLLHOOKSTRUCT& event) {
    switch (message) {
        case WM_LBUTTONUP:
            return kMouseButtonLeft;
        case WM_RBUTTONUP:
            return kMouseButtonRight;
        case WM_MBUTTONUP:
            return kMouseButtonMiddle;
        case WM_XBUTTONUP:
            return HIWORD(event.mouseData) == XBUTTON1 ? kMouseButtonX1 : kMouseButtonX2;
        default:
            return -1;
    }
}

LRESULT CALLBACK MouseHookProc(int code, WPARAM message, LPARAM data) {
    if (code == HC_ACTION) {
        const auto& event = *reinterpret_cast<const MSLLHOOKSTRUCT*>(data);
        if (!IsTaggedPawseInput(event.dwExtraInfo)) {
            const int released_button = GetReleasedButton(message, event);
            if (released_button >= 0 && g_hook_state.pass_button_release[released_button]) {
                g_hook_state.pass_button_release[released_button] = false;
                return CallNextHookEx(nullptr, code, message, data);
            }
            return 1;
        }
    }
    return CallNextHookEx(nullptr, code, message, data);
}

bool IsPhysicallyDown(int virtual_key) { return (GetAsyncKeyState(virtual_key) & 0x8000) != 0; }

void RememberHeldKeys(HookThreadState& state) {
    for (UINT virtual_key = 1; virtual_key < 256; ++virtual_key) {
        const bool tracked = !IsMouseButtonKey(virtual_key) && virtual_key != VK_SHIFT &&
                             virtual_key != VK_CONTROL && virtual_key != VK_MENU;
        const bool held = tracked && IsPhysicallyDown(static_cast<int>(virtual_key));
        state.key_is_down[virtual_key] = held;
        state.pass_key_release[virtual_key] = held;
    }
}

void RememberHeldButtons(HookThreadState& state) {
    // GetAsyncKeyState reports physical buttons; the hook reports logical ones.
    const bool buttons_swapped = GetSystemMetrics(SM_SWAPBUTTON) != 0;
    const bool primary_down = IsPhysicallyDown(VK_LBUTTON);
    const bool secondary_down = IsPhysicallyDown(VK_RBUTTON);
    state.pass_button_release[kMouseButtonLeft] = buttons_swapped ? secondary_down : primary_down;
    state.pass_button_release[kMouseButtonRight] = buttons_swapped ? primary_down : secondary_down;
    state.pass_button_release[kMouseButtonMiddle] = IsPhysicallyDown(VK_MBUTTON);
    state.pass_button_release[kMouseButtonX1] = IsPhysicallyDown(VK_XBUTTON1);
    state.pass_button_release[kMouseButtonX2] = IsPhysicallyDown(VK_XBUTTON2);
}

void RemoveHook(HHOOK& hook) {
    if (hook != nullptr) {
        UnhookWindowsHookEx(hook);
        hook = nullptr;
    }
}

// Installs or removes the hooks to match the request. Reports any hook Windows refused.
void ApplyUpdate(HookThreadState& state, const UpdateRequest& request) {
    state.bindings = request.bindings;
    state.hung_checks = 0;
    const HINSTANCE module = GetModuleHandleW(nullptr);
    unsigned failed_devices = kLockNone;

    if (request.blocked_devices & kLockKeyboard) {
        if (state.keyboard_hook == nullptr) {
            RememberHeldKeys(state);
            state.keyboard_hook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHookProc, module, 0);
            if (state.keyboard_hook == nullptr) {
                failed_devices |= kLockKeyboard;
            }
        }
    } else {
        RemoveHook(state.keyboard_hook);
    }

    if (request.blocked_devices & kLockMouse) {
        if (state.mouse_hook == nullptr) {
            RememberHeldButtons(state);
            state.mouse_hook = SetWindowsHookExW(WH_MOUSE_LL, MouseHookProc, module, 0);
            if (state.mouse_hook == nullptr) {
                failed_devices |= kLockMouse;
            }
        }
    } else {
        RemoveHook(state.mouse_hook);
    }

    if (failed_devices != kLockNone) {
        PostMessageW(state.notify_window, kMessageHookFailed, failed_devices, 0);
    }
}

void CheckMainWindowResponds(HookThreadState& state) {
    if (state.keyboard_hook == nullptr && state.mouse_hook == nullptr) {
        state.hung_checks = 0;
        return;
    }
    if (!IsHungAppWindow(state.notify_window)) {
        state.hung_checks = 0;
        return;
    }
    if (++state.hung_checks < kHungChecksBeforeRelease) {
        return;
    }
    unsigned released_devices = kLockNone;
    if (state.keyboard_hook != nullptr) {
        released_devices |= kLockKeyboard;
    }
    if (state.mouse_hook != nullptr) {
        released_devices |= kLockMouse;
    }
    RemoveHook(state.keyboard_hook);
    RemoveHook(state.mouse_hook);
    state.hung_checks = 0;
    PostMessageW(state.notify_window, kMessageHookFailed, released_devices, 0);
}

void RunHookThread(HWND notify_window, std::promise<void> ready) {
    MSG message;
    // Creates this thread's message queue before the main thread posts to it.
    PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    HookThreadState& state = g_hook_state;
    state.notify_window = notify_window;
    const UINT_PTR watchdog_timer = SetTimer(nullptr, 0, kWatchdogIntervalMilliseconds, nullptr);
    ready.set_value();

    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (message.message == kHookMessageUpdate) {
            const std::unique_ptr<UpdateRequest> request(
                reinterpret_cast<UpdateRequest*>(message.lParam));
            ApplyUpdate(state, *request);
        } else if (message.message == WM_TIMER && message.hwnd == nullptr) {
            CheckMainWindowResponds(state);
        }
    }

    KillTimer(nullptr, watchdog_timer);
    RemoveHook(state.keyboard_hook);
    RemoveHook(state.mouse_hook);
}

}  // namespace

InputBlocker::~InputBlocker() { Stop(); }

bool InputBlocker::Start(HWND notify_window) {
    std::promise<void> ready;
    std::future<void> thread_ready = ready.get_future();
    thread_ = std::thread(RunHookThread, notify_window, std::move(ready));
    thread_ready.wait();
    thread_id_ = GetThreadId(thread_.native_handle());
    return thread_id_ != 0;
}

void InputBlocker::Stop() {
    if (!thread_.joinable()) {
        return;
    }
    PostThreadMessageW(thread_id_, WM_QUIT, 0, 0);
    thread_.join();
    thread_id_ = 0;
}

void InputBlocker::Update(unsigned blocked_devices,
                          const std::array<KeyBinding, kBindingCount>& bindings) {
    auto request = std::make_unique<UpdateRequest>(UpdateRequest{blocked_devices, bindings});
    if (PostThreadMessageW(thread_id_, kHookMessageUpdate, 0,
                           reinterpret_cast<LPARAM>(request.get()))) {
        static_cast<void>(request.release());  // the hook thread owns it now
    }
}

}  // namespace pawse
