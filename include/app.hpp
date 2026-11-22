#pragma once
#include <windows.h>
#include <banner.hpp>
#include <hello_verifier.hpp>
#include <input_blocker.hpp>
#include <memory>
#include <settings.hpp>
#include <string>
#include <updater.hpp>
namespace pawse {
enum class HelloAvailability { kUnknown,
  kAvailable,
  kUnavailable };
enum class StartReason { kUser,
  kSignIn,
  kUpdated };
// What clicking the most recent notification does.
enum class NotificationAction { kNone,
  kUpdate };
// The tray app: owns the hidden main window, the tray icon and the lock state.
class App {
public:
  int Run(HINSTANCE instance, StartReason reason);
  // Used by the settings dialog.
  const Settings &GetSettings() const {
    return settings_;
  }
  HelloAvailability GetHelloAvailability() const {
    return hello_availability_;
  }
  void RefreshHelloAvailability();
  // Saves and applies new settings. Returns an error message, or an empty string on success.
  std::wstring ApplySettings(const Settings &settings, bool start_with_windows);
  void SetSettingsDialog(HWND dialog) {
    settings_dialog_ = dialog;
  }
private:
  static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);
  bool CreateMainWindow();
  void AddTrayIcon();
  void UpdateTrayIcon();
  void RemoveTrayIcon();
  void ShowNotification(const std::wstring &title, const std::wstring &text, bool is_warning,
    NotificationAction action = NotificationAction::kNone);
  void OnNotificationClicked();
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
  // Briefly shows what is locked, or what was just unlocked, if messages are turned on.
  void FlashLockedMessage();
  void FlashUnlockedMessage(unsigned unlocked_devices);
  void StartHideMessageTimer();
  void ShowLockedMessage();
  void HideLockedMessage();
  bool BringMainWindowToFront();
  bool HasUpdate() const {
    return available_update_ > GetCurrentVersion();
  }
  void ScheduleUpdateCheck(UINT delay_milliseconds);
  void OnUpdateCheckTimer();
  void OnUpdateChecked(Version latest);
  void StartUpdate();
  void OnUpdateDownloaded(UpdateDownloadResult result, std::unique_ptr<std::wstring> installer);
  void InstallUpdate();
  HINSTANCE instance_ = nullptr;
  HWND window_ = nullptr;
  HWND settings_dialog_ = nullptr;
  HANDLE single_instance_mutex_ = nullptr;
  // The tray icon for each lock state, indexed by LockedDevices.
  HICON tray_icons_[kLockStateCount] = {};
  UINT taskbar_created_message_ = 0;
  UINT show_settings_message_ = 0;
  NotificationAction notification_action_ = NotificationAction::kNone;
  Settings settings_;
  HelloAvailability hello_availability_ = HelloAvailability::kUnknown;
  InputBlocker input_blocker_;
  HelloVerifier hello_verifier_;
  Banner banner_;
  unsigned locked_devices_ = kLockNone;
  // Devices waiting for Windows Hello before they unlock. The keyboard is let through while
  // the prompt is open so the user can type their PIN.
  unsigned pending_unlock_devices_ = kLockNone;
  // The newest release found, and the newest one the user has been told about.
  Version available_update_;
  Version notified_update_;
  bool checking_for_update_ = false;
  bool downloading_update_ = false;
  // The verified installer for available_update_, once downloaded.
  std::wstring downloaded_installer_;
};
} // namespace pawse
