# Sources

How Pawse works is summarized in [README.md](README.md#how-it-works).

## Threads

- **Main thread:** `App`'s message loop. It owns all windows and all lock state.
- **Hook thread:** `InputBlocker` runs the low-level hooks and a watchdog that removes them if the main window stops responding.
- **Background work:** update checks and downloads (`updater.cpp`) and the Windows Hello prompt (`hello_verifier.cpp`).

Other threads report to the main window with the `kMessage*` messages in `include/common.hpp`. Each message's comment says what `wParam` and `lParam` hold.

## Lock and unlock flow

1. A binding arrives as `WM_HOTKEY` (registered in `App::RegisterHotkeys`). While the keyboard is locked, it arrives as `kMessageBindingPressed` from `KeyboardHookProc` instead.
2. `App::OnBindingPressed` calls `Lock` or `RequestUnlock`. With Windows Hello on, `RequestUnlock` shows the prompt and `FinishUnlockAttempt` handles the result.
3. `App::ApplyLockState` calls `InputBlocker::Update`, which posts the new state to `ApplyUpdate` on the hook thread.

Pawse must never trap the user. The mouse can be locked only while a binding can unlock it (`App::CanLock`). Without Windows Hello, the binding alone unlocks. If the hooks fail, `App::OnHooksFailed` unlocks.

## Files

| File | Contents |
| --- | --- |
| `main.cpp` | Entry point. Maps `--updated` and `--autostart` to a `StartReason`. |
| `app.cpp` | `App`: hidden main window, tray icon and menu, notifications, lock state and the update flow. |
| `input_blocker.cpp` | Hook thread, keyboard and mouse hooks, binding matching while locked, watchdog. |
| `hello_verifier.cpp` | The Windows Hello prompt through `UserConsentVerifier`. |
| `banner.cpp` | The message in the middle of the screen. |
| `settings.cpp` | `settings.ini` in `%APPDATA%\Pawse`, and the "start with Windows" Run key. |
| `settings_dialog.cpp` | The Settings dialog (`IDD_SETTINGS` in `resources/pawse.rc`). |
| `binding_box.cpp` | The click-to-record key binding control. |
| `key_binding.cpp` | Formatting, parsing and validating `KeyBinding`. |
| `updater.cpp` | Release lookup, download and signature check, and starting the installer. |

## Where to add

- **A setting:** the `Settings` struct in `include/settings.hpp`, `LoadSettings` and `SaveSettings` in `settings.cpp`, a control in `IDD_SETTINGS` with its id in `resources/resource.hpp`, and `ShowSettingsInControls` and `ReadSettingsFromControls` in `settings_dialog.cpp`. Then describe it in `docs/README.md#settings`.
- **A tray menu item:** `MenuCommand`, `App::ShowTrayMenu` and `App::HandleCommand` in `app.cpp`.
- **A message from another thread:** a `kMessage*` constant in `include/common.hpp` and a case in `App::HandleMessage`.
- **Lock and unlock text:** `App::ShowLockedMessage`, `App::FlashUnlockedMessage` and `App::DescribeUnlock`.
