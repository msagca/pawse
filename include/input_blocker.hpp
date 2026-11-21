#pragma once
#include <array>
#include <thread>
#include <common.hpp>
#include <key_binding.hpp>
namespace pawse {
// Blocks physical keyboard and mouse input with low-level hooks on a dedicated thread.
//
// The hooks run on their own thread so a busy main window (a dialog, a menu) never delays input.
// While the keyboard is blocked, the hook itself watches for the key bindings, because blocked
// keys never reach RegisterHotKey. A matched binding is posted to the main window as
// kMessageBindingPressed. Other blocked input is reported as kMessageInputBlocked, at most a
// few times a second.
//
// Safety: if the main window stops responding while input is blocked, the hook thread removes
// its hooks and posts kMessageHookFailed, so a hung Pawse can't trap the user.
class InputBlocker {
public:
  InputBlocker() = default;
  InputBlocker(const InputBlocker &) = delete;
  InputBlocker &operator=(const InputBlocker &) = delete;
  ~InputBlocker();
  bool Start(HWND notify_window);
  void Stop();
  // Blocks exactly the given devices (LockedDevices flags) from now on.
  void Update(unsigned blocked_devices, const std::array<KeyBinding, kBindingCount> &bindings);
private:
  std::thread thread_;
  DWORD thread_id_ = 0;
};
} // namespace pawse
