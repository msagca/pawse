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
// The tray icon and message color for each lock state, indexed by LockedDevices. They match the
// tray icons drawn by tools/make_icons.py.
constexpr size_t kLockStateCount = kLockBoth + 1;
constexpr COLORREF kLockStateColors[kLockStateCount] = {
  RGB(74, 158, 255), // nothing locked: blue
  RGB(242, 140, 40), // keyboard: orange
  RGB(245, 197, 66), // mouse: yellow
  RGB(229, 72, 77), // both: red
};
// Messages posted to the main window. Each comment says what wParam and lParam hold.
constexpr UINT kMessageBindingPressed = WM_APP + 1; // wParam: BindingIndex
constexpr UINT kMessageHookFailed = WM_APP + 2; // wParam: devices no longer blocked
constexpr UINT kMessageHelloResult = WM_APP + 3; // wParam: HelloResult, lParam: attempt id
constexpr UINT kMessageHelloAvailability = WM_APP + 4; // wParam: 1 if Windows Hello is set up
constexpr UINT kMessageTrayIcon = WM_APP + 5; // Shell_NotifyIcon callback
constexpr UINT kMessageInputBlocked = WM_APP + 6; // a locked device was used
constexpr UINT kMessageUpdateChecked = WM_APP + 7; // wParam: packed latest Version, 0 on failure
// wParam: UpdateDownloadResult, lParam: std::wstring* installer path when verified (receiver frees)
constexpr UINT kMessageUpdateDownloaded = WM_APP + 8;
// Marks keyboard input that Pawse injects itself, so its own hooks let it through.
constexpr ULONG_PTR kPawseInputTag = 0x50415753; // "PAWS"
} // namespace pawse
