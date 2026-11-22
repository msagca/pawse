#include <algorithm>
#include <app.hpp>
#include <binding_box.hpp>
#include <commctrl.h>
#include <resource.hpp>
#include <settings_dialog.hpp>
#include <shellapi.h>
#include <string>
namespace pawse {
namespace {
  constexpr int kBindingControlIds[kBindingCount] = {IDC_BINDING_KEYBOARD, IDC_BINDING_MOUSE, IDC_BINDING_BOTH};
  constexpr const wchar_t *kBindingTargets[kBindingCount] = {L"the keyboard", L"the mouse", L"both"};
  // The message duration only matters while the message is turned on.
  void UpdateMessageControls(HWND dialog) {
    const bool show_message = IsDlgButtonChecked(dialog, IDC_SHOW_MESSAGE) == BST_CHECKED;
    EnableWindow(GetDlgItem(dialog, IDC_MESSAGE_SECONDS), show_message);
    EnableWindow(GetDlgItem(dialog, IDC_MESSAGE_SECONDS_SPIN), show_message);
  }
  void ShowSettingsInControls(HWND dialog, const Settings &settings, bool start_with_windows) {
    for (size_t index = 0; index < kBindingCount; ++index) {
      SetBindingBoxValue(GetDlgItem(dialog, kBindingControlIds[index]), settings.bindings[index]);
    }
    CheckDlgButton(dialog, IDC_REQUIRE_HELLO, settings.require_windows_hello ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemInt(dialog, IDC_AUTO_UNLOCK, settings.auto_unlock_minutes, FALSE);
    CheckDlgButton(dialog, IDC_SHOW_MESSAGE, settings.show_message ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemInt(dialog, IDC_MESSAGE_SECONDS, settings.message_seconds, FALSE);
    CheckDlgButton(dialog, IDC_CHECK_FOR_UPDATES, settings.check_for_updates ? BST_CHECKED : BST_UNCHECKED);
    UpdateMessageControls(dialog);
    CheckDlgButton(dialog, IDC_START_WITH_WINDOWS, start_with_windows ? BST_CHECKED : BST_UNCHECKED);
  }
  Settings ReadSettingsFromControls(HWND dialog) {
    Settings settings;
    for (size_t index = 0; index < kBindingCount; ++index) {
      settings.bindings[index] = GetBindingBoxValue(GetDlgItem(dialog, kBindingControlIds[index]));
    }
    settings.require_windows_hello = IsDlgButtonChecked(dialog, IDC_REQUIRE_HELLO) == BST_CHECKED;
    BOOL is_number = FALSE;
    const UINT minutes = GetDlgItemInt(dialog, IDC_AUTO_UNLOCK, &is_number, FALSE);
    settings.auto_unlock_minutes = is_number ? std::min(minutes, kMaxAutoUnlockMinutes) : 0;
    settings.show_message = IsDlgButtonChecked(dialog, IDC_SHOW_MESSAGE) == BST_CHECKED;
    const UINT seconds = GetDlgItemInt(dialog, IDC_MESSAGE_SECONDS, &is_number, FALSE);
    settings.message_seconds = is_number
                                 ? std::clamp(seconds, kMinMessageSeconds, kMaxMessageSeconds)
                                 : kDefaultMessageSeconds;
    settings.check_for_updates = IsDlgButtonChecked(dialog, IDC_CHECK_FOR_UPDATES) == BST_CHECKED;
    return settings;
  }
  void ShowHelloAvailability(HWND dialog, HelloAvailability availability) {
    const wchar_t *status = L"Checking whether Windows Hello is set up…";
    if (availability == HelloAvailability::kAvailable) {
      status =
        L"Windows Hello is set up. After the key binding, Windows asks for your PIN, "
        L"fingerprint or face.";
    } else if (availability == HelloAvailability::kUnavailable) {
      status =
        L"Windows Hello isn't set up, so the key binding alone unlocks. Add a PIN in "
        L"Windows Settings > Accounts > Sign-in options.";
    }
    SetDlgItemTextW(dialog, IDC_HELLO_STATUS, status);
    EnableWindow(GetDlgItem(dialog, IDC_REQUIRE_HELLO),
      availability != HelloAvailability::kUnavailable);
  }
  std::wstring ValidateBindings(const Settings &settings) {
    for (size_t index = 0; index < kBindingCount; ++index) {
      if (settings.bindings[index].IsEmpty()) {
        return L"Choose a key binding for locking " + std::wstring(kBindingTargets[index]) +
               L".";
      }
    }
    for (size_t first = 0; first < kBindingCount; ++first) {
      for (size_t second = first + 1; second < kBindingCount; ++second) {
        if (settings.bindings[first] == settings.bindings[second]) {
          return L"Locking " + std::wstring(kBindingTargets[first]) + L" and " +
                 kBindingTargets[second] +
                 L" use the same key binding. Choose a different one for each.";
        }
      }
    }
    return {};
  }
  void SetDialogIcons(HWND dialog) {
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    HICON small_icon = nullptr;
    HICON large_icon = nullptr;
    LoadIconMetric(instance, MAKEINTRESOURCEW(IDI_PAWSE), LIM_SMALL, &small_icon);
    LoadIconMetric(instance, MAKEINTRESOURCEW(IDI_PAWSE), LIM_LARGE, &large_icon);
    SendMessageW(dialog, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small_icon));
    SendMessageW(dialog, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(large_icon));
  }
  void DestroyDialogIcons(HWND dialog) {
    for (const WPARAM icon_type : {ICON_SMALL, ICON_BIG}) {
      const auto icon = reinterpret_cast<HICON>(SendMessageW(dialog, WM_SETICON, icon_type, 0));
      if (icon != nullptr) {
        DestroyIcon(icon);
      }
    }
  }
  INT_PTR CALLBACK SettingsDialogProc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam) {
    auto *app = reinterpret_cast<App *>(GetWindowLongPtrW(dialog, DWLP_USER));
    switch (message) {
    case WM_INITDIALOG: {
      app = reinterpret_cast<App *>(lparam);
      SetWindowLongPtrW(dialog, DWLP_USER, lparam);
      app->SetSettingsDialog(dialog);
      SendDlgItemMessageW(dialog, IDC_AUTO_UNLOCK_SPIN, UDM_SETRANGE32, 0, kMaxAutoUnlockMinutes);
      SendDlgItemMessageW(dialog, IDC_MESSAGE_SECONDS_SPIN, UDM_SETRANGE32, kMinMessageSeconds, kMaxMessageSeconds);
      ShowSettingsInControls(dialog, app->GetSettings(), IsStartWithWindowsEnabled());
      ShowHelloAvailability(dialog, app->GetHelloAvailability());
      app->RefreshHelloAvailability();
      SetDialogIcons(dialog);
      SetForegroundWindow(dialog);
      return TRUE;
    }
    case WM_NOTIFY: {
      const auto *notify = reinterpret_cast<const NMHDR *>(lparam);
      if (notify->idFrom == IDC_REPOSITORY_LINK &&
          (notify->code == NM_CLICK || notify->code == NM_RETURN)) {
        const auto *link = reinterpret_cast<const NMLINK *>(lparam);
        ShellExecuteW(dialog, L"open", link->item.szUrl, nullptr, nullptr, SW_SHOWNORMAL);
        return TRUE;
      }
      break;
    }
    case kMessageHelloAvailability:
      ShowHelloAvailability(dialog, wparam != 0 ? HelloAvailability::kAvailable : HelloAvailability::kUnavailable);
      return TRUE;
    case WM_COMMAND:
      switch (LOWORD(wparam)) {
      case IDC_SHOW_MESSAGE:
        UpdateMessageControls(dialog);
        return TRUE;
      case IDC_RESTORE_DEFAULTS:
        ShowSettingsInControls(
          dialog,
          Settings{},
          IsDlgButtonChecked(dialog, IDC_START_WITH_WINDOWS) == BST_CHECKED);
        return TRUE;
      case IDOK: {
        const Settings settings = ReadSettingsFromControls(dialog);
        const bool start_with_windows =
          IsDlgButtonChecked(dialog, IDC_START_WITH_WINDOWS) == BST_CHECKED;
        std::wstring error = ValidateBindings(settings);
        if (error.empty()) {
          error = app->ApplySettings(settings, start_with_windows);
        }
        if (!error.empty()) {
          MessageBoxW(dialog, error.c_str(), L"Pawse", MB_ICONWARNING | MB_OK);
          return TRUE;
        }
        EndDialog(dialog, IDOK);
        return TRUE;
      }
      case IDCANCEL:
        EndDialog(dialog, IDCANCEL);
        return TRUE;
      default:
        break;
      }
      break;
    case WM_DESTROY:
      DestroyDialogIcons(dialog);
      if (app != nullptr) {
        app->SetSettingsDialog(nullptr);
      }
      break;
    default:
      break;
    }
    return FALSE;
  }
} // namespace
void ShowSettingsDialog(HINSTANCE instance, App &app) {
  DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_SETTINGS), nullptr, SettingsDialogProc, reinterpret_cast<LPARAM>(&app));
}
} // namespace pawse
