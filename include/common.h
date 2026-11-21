#pragma once

#include <windows.h>

#include <cstddef>

namespace pawse {

// Devices Pawse can block. Values combine as bit flags.
enum LockedDevices : unsigned {
    kLockNone = 0,
    kLockKeyboard = 1u << 0,
    kLockMouse = 1u << 1,
    kLockBoth = kLockKeyboard | kLockMouse,
};

// One key binding per lock target, in this order everywhere.
enum BindingIndex : size_t {
    kBindingKeyboard,
    kBindingMouse,
    kBindingBoth,
    kBindingCount,
};

constexpr unsigned kBindingDevices[kBindingCount] = {kLockKeyboard, kLockMouse, kLockBoth};

// Messages posted to the main window. Each comment says what wParam and lParam hold.
constexpr UINT kMessageBindingPressed = WM_APP + 1;     // wParam: BindingIndex
constexpr UINT kMessageHookFailed = WM_APP + 2;         // wParam: devices no longer blocked
constexpr UINT kMessageHelloResult = WM_APP + 3;        // wParam: HelloResult, lParam: attempt id
constexpr UINT kMessageHelloAvailability = WM_APP + 4;  // wParam: 1 if Windows Hello is set up
constexpr UINT kMessageTrayIcon = WM_APP + 5;           // Shell_NotifyIcon callback
constexpr UINT kMessageInputBlocked = WM_APP + 6;       // a locked device was used

// Marks keyboard input that Pawse injects itself, so its own hooks let it through.
constexpr ULONG_PTR kPawseInputTag = 0x50415753;  // "PAWS"

}  // namespace pawse
