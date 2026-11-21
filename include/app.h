#pragma once

#include <windows.h>

#include <string>

#include "banner.h"
#include "hello_verifier.h"
#include "input_blocker.h"
#include "settings.h"

namespace pawse {

enum class HelloAvailability { kUnknown, kAvailable, kUnavailable };

// The tray app: owns the hidden main window, the tray icon and the lock state.
class App {
public:
    int Run(HINSTANCE instance, bool started_at_sign_in);

    // Used by the settings dialog.
    const Settings& GetSettings() const { return settings_; }
    HelloAvailability GetHelloAvailability() const { return hello_availability_; }
    void RefreshHelloAvailability();
    // Saves and applies new settings. Returns an error message, or an empty string on success.
    std::wstring ApplySettings(const Settings& settings, bool start_with_windows);
    void SetSettingsDialog(HWND dialog) { settings_dialog_ = dialog; }

private:
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);

    bool CreateMainWindow();
    void AddTrayIcon();
    void UpdateTrayIcon();
    void RemoveTrayIcon();
    void ShowNotification(const std::wstring& title, const std::wstring& text, bool is_warning);
    void ShowTrayMenu(POINT anchor);
    void HandleCommand(UINT command);
    void OpenSettings();

    // Registers every binding as a global hotkey, replacing earlier registrations.
    // Returns the bindings Windows refused (already used by another app), or an empty string.
    std::wstring RegisterHotkeys();
    void UnregisterHotkeys();

    void OnBindingPressed(size_t binding_index);
    void Lock(unsigned devices);
    void RequestUnlock(unsigned devices);
    void Unlock(unsigned devices);
    void FinishUnlockAttempt(HelloResult result);
    void OnHooksFailed(unsigned devices);
    void OnAutoUnlockTimer();
    void ApplyLockState();
    bool CanUnlockFromMenu() const;
    void OnInputBlocked();
    void ShowLockedMessage();
    void HideLockedMessage();
    bool BringMainWindowToFront();

    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
    HWND settings_dialog_ = nullptr;
    HICON icon_unlocked_ = nullptr;
    HICON icon_locked_ = nullptr;
    UINT taskbar_created_message_ = 0;
    UINT show_settings_message_ = 0;

    Settings settings_;
    HelloAvailability hello_availability_ = HelloAvailability::kUnknown;
    InputBlocker input_blocker_;
    HelloVerifier hello_verifier_;
    Banner banner_;

    unsigned locked_devices_ = kLockNone;
    // Devices waiting for Windows Hello before they unlock. The keyboard is let through while
    // the prompt is open so the user can type their PIN.
    unsigned pending_unlock_devices_ = kLockNone;
};

}  // namespace pawse
