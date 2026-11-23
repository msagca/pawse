#include <windows.h>
#include <algorithm>
#include <app.hpp>
#include <binding_box.hpp>
#include <commctrl.h>
#include <resource.hpp>
#include <settings_dialog.hpp>
#include <shellapi.h>
#include <string>
#include <vector>
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
    for (size_t first = 0; first < kBindingCount; ++first) {
      for (size_t second = first + 1; second < kBindingCount; ++second) {
        const KeyBinding &binding = settings.bindings[first];
        if (!binding.IsEmpty() && binding == settings.bindings[second]) {
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
  // How a control follows the dialog's size.
  enum class Anchor {
    kFixed,
    kStretch, // grows to the right
    kBottomLeft, // moves down
    kBottomRight, // moves down and to the right
  };
  struct ControlLayout {
    HWND control;
    RECT rect;
    Anchor anchor;
    LONG max_width; // in the same pixels as `rect`, 0 for no limit
  };
  // The layout from IDD_SETTINGS, in client pixels at `dpi`. The window can't get smaller than it.
  struct DialogLayout {
    UINT dpi = USER_DEFAULT_SCREEN_DPI;
    SIZE client{};
    std::vector<ControlLayout> controls;
  };
  struct DialogState {
    App *app;
    DialogLayout layout;
  };
  // The window's size when it last closed, at 96 DPI. Empty until it first closes.
  struct RememberedSize {
    SIZE window{};
    bool maximized = false;
  };
  RememberedSize g_remembered_size;
  // Controls that end within this many dialog units of the right edge stretch with the window.
  constexpr int kStretchMarginDlu = 20;
  // Key binding boxes stop stretching at this width in dialog units.
  constexpr int kBindingBoxMaxWidthDlu = 200;
  // Posted to the dialog itself after a DPI change, once Windows has rescaled the controls.
  constexpr UINT kMessageRelayout = WM_APP + 100;
  Anchor ChooseAnchor(HWND dialog, int id, const RECT &rect, LONG client_width) {
    switch (id) {
    case IDOK:
    case IDCANCEL:
      return Anchor::kBottomRight;
    case IDC_RESTORE_DEFAULTS:
    case IDC_REPOSITORY_LINK:
      return Anchor::kBottomLeft;
    default:
      break;
    }
    RECT margin{0, 0, kStretchMarginDlu, 0};
    MapDialogRect(dialog, &margin);
    return client_width - rect.right <= margin.right ? Anchor::kStretch : Anchor::kFixed;
  }
  DialogLayout CaptureLayout(HWND dialog) {
    DialogLayout layout;
    layout.dpi = GetDpiForWindow(dialog);
    RECT client{};
    GetClientRect(dialog, &client);
    layout.client = {client.right, client.bottom};
    RECT binding_box_max{0, 0, kBindingBoxMaxWidthDlu, 0};
    MapDialogRect(dialog, &binding_box_max);
    for (HWND control = GetWindow(dialog, GW_CHILD); control != nullptr;
      control = GetWindow(control, GW_HWNDNEXT)) {
      RECT rect{};
      GetWindowRect(control, &rect);
      MapWindowPoints(nullptr, dialog, reinterpret_cast<POINT *>(&rect), 2);
      const int id = GetDlgCtrlID(control);
      const bool is_binding_box = std::ranges::find(kBindingControlIds, id) != std::end(kBindingControlIds);
      layout.controls.push_back({control,
        rect,
        ChooseAnchor(dialog, id, rect, layout.client.cx),
        is_binding_box ? binding_box_max.right : 0});
    }
    return layout;
  }
  LONG ScaleToDpi(LONG value, UINT from_dpi, UINT to_dpi) {
    return MulDiv(value, static_cast<int>(to_dpi), static_cast<int>(from_dpi));
  }
  SIZE GetMinimumWindowSize(HWND dialog, const DialogLayout &layout) {
    const UINT dpi = GetDpiForWindow(dialog);
    RECT window{0, 0, ScaleToDpi(layout.client.cx, layout.dpi, dpi), ScaleToDpi(layout.client.cy, layout.dpi, dpi)};
    AdjustWindowRectExForDpi(&window,
      static_cast<DWORD>(GetWindowLongPtrW(dialog, GWL_STYLE)),
      FALSE,
      static_cast<DWORD>(GetWindowLongPtrW(dialog, GWL_EXSTYLE)),
      dpi);
    return {window.right - window.left, window.bottom - window.top};
  }
  // Moves the controls from their places in IDD_SETTINGS by however much the window grew.
  void ApplyLayout(HWND dialog, const DialogLayout &layout) {
    if (layout.controls.empty()) {
      return;
    }
    const UINT dpi = GetDpiForWindow(dialog);
    const auto scale = [&](LONG value) { return ScaleToDpi(value, layout.dpi, dpi); };
    RECT client{};
    GetClientRect(dialog, &client);
    const LONG grow_x = std::max(0L, client.right - scale(layout.client.cx));
    const LONG grow_y = std::max(0L, client.bottom - scale(layout.client.cy));
    HDWP positions = BeginDeferWindowPos(static_cast<int>(layout.controls.size()));
    for (const ControlLayout &item : layout.controls) {
      if (positions == nullptr) {
        break;
      }
      RECT rect{scale(item.rect.left), scale(item.rect.top), scale(item.rect.right), scale(item.rect.bottom)};
      switch (item.anchor) {
      case Anchor::kStretch:
        rect.right += grow_x;
        if (item.max_width > 0) {
          rect.right = std::min(rect.right, rect.left + scale(item.max_width));
        }
        break;
      case Anchor::kBottomLeft:
        OffsetRect(&rect, 0, grow_y);
        break;
      case Anchor::kBottomRight:
        OffsetRect(&rect, grow_x, grow_y);
        break;
      case Anchor::kFixed:
        break;
      }
      positions = DeferWindowPos(positions, item.control, nullptr, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    if (positions != nullptr) {
      EndDeferWindowPos(positions);
    }
    // Group boxes and wrapped text leave stale pixels behind when they change size.
    RedrawWindow(dialog, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
  }
  // Opens the window at the size it had when it last closed, centered where Windows put it.
  void RestoreRememberedSize(HWND dialog, const DialogLayout &layout) {
    if (g_remembered_size.window.cx == 0) {
      return;
    }
    const UINT dpi = GetDpiForWindow(dialog);
    const SIZE minimum = GetMinimumWindowSize(dialog, layout);
    MONITORINFO monitor{sizeof(monitor)};
    GetMonitorInfoW(MonitorFromWindow(dialog, MONITOR_DEFAULTTONEAREST), &monitor);
    const RECT &work = monitor.rcWork;
    const LONG width = std::clamp(ScaleToDpi(g_remembered_size.window.cx, USER_DEFAULT_SCREEN_DPI, dpi),
      minimum.cx,
      std::max(minimum.cx, work.right - work.left));
    const LONG height = std::clamp(ScaleToDpi(g_remembered_size.window.cy, USER_DEFAULT_SCREEN_DPI, dpi),
      minimum.cy,
      std::max(minimum.cy, work.bottom - work.top));
    RECT window{};
    GetWindowRect(dialog, &window);
    const LONG left = std::clamp((window.left + window.right - width) / 2, work.left, std::max(work.left, work.right - width));
    const LONG top = std::clamp((window.top + window.bottom - height) / 2, work.top, std::max(work.top, work.bottom - height));
    SetWindowPos(dialog, nullptr, left, top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    if (g_remembered_size.maximized) {
      ShowWindow(dialog, SW_SHOWMAXIMIZED);
    }
  }
  void RememberSize(HWND dialog) {
    WINDOWPLACEMENT placement{sizeof(placement)};
    if (!GetWindowPlacement(dialog, &placement)) {
      return;
    }
    const UINT dpi = GetDpiForWindow(dialog);
    const RECT &normal = placement.rcNormalPosition;
    g_remembered_size.window = {ScaleToDpi(normal.right - normal.left, dpi, USER_DEFAULT_SCREEN_DPI),
      ScaleToDpi(normal.bottom - normal.top, dpi, USER_DEFAULT_SCREEN_DPI)};
    g_remembered_size.maximized = placement.showCmd == SW_SHOWMAXIMIZED;
  }
  INT_PTR CALLBACK SettingsDialogProc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam) {
    auto *state = reinterpret_cast<DialogState *>(GetWindowLongPtrW(dialog, DWLP_USER));
    App *app = state != nullptr ? state->app : nullptr;
    switch (message) {
    case WM_INITDIALOG: {
      state = reinterpret_cast<DialogState *>(lparam);
      app = state->app;
      SetWindowLongPtrW(dialog, DWLP_USER, lparam);
      app->SetSettingsDialog(dialog);
      state->layout = CaptureLayout(dialog);
      RestoreRememberedSize(dialog, state->layout);
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
    case WM_SIZE:
      if (state != nullptr) {
        ApplyLayout(dialog, state->layout);
      }
      return TRUE;
    case WM_GETMINMAXINFO:
      if (state != nullptr && !state->layout.controls.empty()) {
        const SIZE minimum = GetMinimumWindowSize(dialog, state->layout);
        reinterpret_cast<MINMAXINFO *>(lparam)->ptMinTrackSize = {minimum.cx, minimum.cy};
        return TRUE;
      }
      break;
    case WM_DPICHANGED:
      PostMessageW(dialog, kMessageRelayout, 0, 0);
      break;
    case kMessageRelayout:
      if (state != nullptr) {
        ApplyLayout(dialog, state->layout);
      }
      return TRUE;
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
      if (state != nullptr && !state->layout.controls.empty()) {
        RememberSize(dialog);
      }
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
  DialogState state{&app};
  DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_SETTINGS), nullptr, SettingsDialogProc, reinterpret_cast<LPARAM>(&state));
}
} // namespace pawse
