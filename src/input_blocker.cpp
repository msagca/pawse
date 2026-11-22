#include <future>
#include <input_blocker.hpp>
#include <memory>
namespace pawse {
namespace {
  constexpr UINT kHookMessageUpdate = WM_APP + 100;
  constexpr UINT kWatchdogIntervalMilliseconds = 1000;
  // IsHungAppWindow already waits about 5 seconds; this many failed checks in a row on top of
  // that removes the hooks.
  constexpr int kHungChecksBeforeRelease = 3;
  // Blocked input is reported to the main window at most this often, so mouse moves don't flood it.
  constexpr ULONGLONG kBlockedInputNoticeIntervalMilliseconds = 250;
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
    // Keys held down, to match bindings and to tell auto-repeat from a new press.
    std::array<bool, 256> key_is_down{};
    int hung_checks = 0;
    ULONGLONG last_blocked_input_notice = 0;
  };
  HookThreadState g_hook_state;
  bool IsTaggedPawseInput(ULONG_PTR extra_info) {
    return extra_info == kPawseInputTag;
  }
  // Whether Windows has seen this key or button go down and not yet up. Blocked input never
  // reaches this state. Inside a hook, it doesn't include the event being handled yet.
  bool IsDownForWindows(UINT virtual_key) {
    return (GetAsyncKeyState(static_cast<int>(virtual_key)) & 0x8000) != 0;
  }
  bool IsMouseButtonKey(UINT virtual_key) {
    return virtual_key == VK_LBUTTON || virtual_key == VK_RBUTTON || virtual_key == VK_MBUTTON ||
           virtual_key == VK_XBUTTON1 || virtual_key == VK_XBUTTON2 || virtual_key == VK_CANCEL;
  }
  UINT GetHeldModifiers(const std::array<bool, 256> &key_is_down) {
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
  // Tells the main window that a locked device was used, so it can say how to unlock.
  void ReportBlockedInput(HookThreadState &state) {
    const ULONGLONG now = GetTickCount64();
    if (now - state.last_blocked_input_notice < kBlockedInputNoticeIntervalMilliseconds) {
      return;
    }
    state.last_blocked_input_notice = now;
    PostMessageW(state.notify_window, kMessageInputBlocked, 0, 0);
  }
  LRESULT CALLBACK KeyboardHookProc(int code, WPARAM message, LPARAM data) {
    if (code != HC_ACTION) {
      return CallNextHookEx(nullptr, code, message, data);
    }
    const auto &event = *reinterpret_cast<const KBDLLHOOKSTRUCT *>(data);
    if (IsTaggedPawseInput(event.dwExtraInfo)) {
      return CallNextHookEx(nullptr, code, message, data);
    }
    HookThreadState &state = g_hook_state;
    const UINT virtual_key = event.vkCode & 0xFF;
    if (event.flags & LLKHF_UP) {
      state.key_is_down[virtual_key] = false;
      // A release passes whenever Windows saw the key go down, so no key is left stuck: one
      // held when the lock started, or one pressed where the hook can't see it, such as in a
      // window of an app running as administrator.
      if (IsDownForWindows(virtual_key)) {
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
          return 1;
        }
      }
    }
    ReportBlockedInput(state);
    return 1;
  }
  // The virtual key of the button this message releases, or 0 if it releases none. The hook
  // reports logical buttons, but GetAsyncKeyState reports physical ones.
  UINT GetReleasedButtonKey(WPARAM message, const MSLLHOOKSTRUCT &event) {
    const bool buttons_swapped = GetSystemMetrics(SM_SWAPBUTTON) != 0;
    switch (message) {
    case WM_LBUTTONUP:
      return buttons_swapped ? VK_RBUTTON : VK_LBUTTON;
    case WM_RBUTTONUP:
      return buttons_swapped ? VK_LBUTTON : VK_RBUTTON;
    case WM_MBUTTONUP:
      return VK_MBUTTON;
    case WM_XBUTTONUP:
      return HIWORD(event.mouseData) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2;
    default:
      return 0;
    }
  }
  LRESULT CALLBACK MouseHookProc(int code, WPARAM message, LPARAM data) {
    if (code == HC_ACTION) {
      const auto &event = *reinterpret_cast<const MSLLHOOKSTRUCT *>(data);
      if (!IsTaggedPawseInput(event.dwExtraInfo)) {
        // Like a key release, a button release passes whenever Windows saw the button go down.
        const UINT released_button = GetReleasedButtonKey(message, event);
        if (released_button != 0 && IsDownForWindows(released_button)) {
          return CallNextHookEx(nullptr, code, message, data);
        }
        ReportBlockedInput(g_hook_state);
        return 1;
      }
    }
    return CallNextHookEx(nullptr, code, message, data);
  }
  // Replaces the tracked keys with the keys Windows sees held. The hook reports left and right
  // modifiers, never the combined VK_SHIFT, VK_CONTROL and VK_MENU.
  void RememberHeldKeys(HookThreadState &state) {
    for (UINT virtual_key = 1; virtual_key < 256; ++virtual_key) {
      const bool tracked = !IsMouseButtonKey(virtual_key) && virtual_key != VK_SHIFT &&
                           virtual_key != VK_CONTROL && virtual_key != VK_MENU;
      state.key_is_down[virtual_key] = tracked && IsDownForWindows(virtual_key);
    }
  }
  // The hook never sees input on the secure desktop (the lock screen, Ctrl+Alt+Del, UAC
  // prompts). Keys released there would stay tracked as held and keep bindings from matching.
  void CALLBACK OnDesktopSwitch(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD) {
    if (g_hook_state.keyboard_hook != nullptr) {
      RememberHeldKeys(g_hook_state);
    }
  }
  void RemoveHook(HHOOK &hook) {
    if (hook != nullptr) {
      UnhookWindowsHookEx(hook);
      hook = nullptr;
    }
  }
  // Installs or removes the hooks to match the request. Reports any hook Windows refused.
  void ApplyUpdate(HookThreadState &state, const UpdateRequest &request) {
    state.bindings = request.bindings;
    state.hung_checks = 0;
    const HINSTANCE module = GetModuleHandleW(nullptr);
    unsigned failed_devices = kLockNone;
    if (request.blocked_devices & kLockKeyboard) {
      if (state.keyboard_hook == nullptr) {
        state.keyboard_hook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHookProc, module, 0);
        if (state.keyboard_hook == nullptr) {
          failed_devices |= kLockKeyboard;
        }
        // Read after installing: Windows holds new input for the hook until this thread reads
        // messages again, so this read can't miss a change the hook won't see.
        RememberHeldKeys(state);
      }
    } else {
      RemoveHook(state.keyboard_hook);
    }
    if (request.blocked_devices & kLockMouse) {
      if (state.mouse_hook == nullptr) {
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
  void CheckMainWindowResponds(HookThreadState &state) {
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
    HookThreadState &state = g_hook_state;
    state.notify_window = notify_window;
    const UINT_PTR watchdog_timer = SetTimer(nullptr, 0, kWatchdogIntervalMilliseconds, nullptr);
    const HWINEVENTHOOK desktop_switch_hook = SetWinEventHook(EVENT_SYSTEM_DESKTOPSWITCH, EVENT_SYSTEM_DESKTOPSWITCH, nullptr, OnDesktopSwitch, 0, 0, WINEVENT_OUTOFCONTEXT);
    ready.set_value();
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
      if (message.message == kHookMessageUpdate) {
        const std::unique_ptr<UpdateRequest> request(
          reinterpret_cast<UpdateRequest *>(message.lParam));
        ApplyUpdate(state, *request);
      } else if (message.message == WM_TIMER && message.hwnd == nullptr) {
        CheckMainWindowResponds(state);
      }
    }
    if (desktop_switch_hook != nullptr) {
      UnhookWinEvent(desktop_switch_hook);
    }
    KillTimer(nullptr, watchdog_timer);
    RemoveHook(state.keyboard_hook);
    RemoveHook(state.mouse_hook);
  }
} // namespace
InputBlocker::~InputBlocker() {
  Stop();
}
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
  const std::array<KeyBinding, kBindingCount> &bindings) {
  auto request = std::make_unique<UpdateRequest>(UpdateRequest{blocked_devices, bindings});
  if (PostThreadMessageW(thread_id_, kHookMessageUpdate, 0, reinterpret_cast<LPARAM>(request.get()))) {
    static_cast<void>(request.release()); // the hook thread owns it now
  }
}
} // namespace pawse
