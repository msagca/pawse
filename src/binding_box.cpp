#include <binding_box.hpp>
#include <resource.hpp>
#include <string>
namespace pawse {
namespace {
  constexpr wchar_t kClassName[] = L"" BINDING_BOX_CLASS_NAME;
  // Sizes in pixels at 96 DPI.
  constexpr int kBorderWidth = 1;
  constexpr int kRecordingBorderWidth = 2;
  constexpr int kTextPadding = 4;
  struct BindingBoxState {
    KeyBinding binding;
    HFONT font = nullptr;
    bool is_recording = false;
    // After a key is recorded, the keys still held are released. Their key-up and character
    // messages are dropped, so a released Alt doesn't open the window menu and Esc doesn't
    // reach the dialog.
    bool is_dropping_released_keys = false;
  };
  BindingBoxState *GetState(HWND box) {
    return reinterpret_cast<BindingBoxState *>(GetWindowLongPtrW(box, GWLP_USERDATA));
  }
  int Scale(int value, HWND box) {
    return MulDiv(value, static_cast<int>(GetDpiForWindow(box)), 96);
  }
  bool IsKeyHeld(int virtual_key) {
    return (GetKeyState(virtual_key) & 0x8000) != 0;
  }
  UINT GetHeldModifiers() {
    UINT modifiers = 0;
    if (IsKeyHeld(VK_CONTROL)) {
      modifiers |= MOD_CONTROL;
    }
    if (IsKeyHeld(VK_MENU)) {
      modifiers |= MOD_ALT;
    }
    if (IsKeyHeld(VK_SHIFT)) {
      modifiers |= MOD_SHIFT;
    }
    if (IsKeyHeld(VK_LWIN) || IsKeyHeld(VK_RWIN)) {
      modifiers |= MOD_WIN;
    }
    return modifiers;
  }
  // Mouse buttons and the codes input methods send can't be bindings.
  bool IsBindableKey(UINT virtual_key) {
    switch (virtual_key) {
    case VK_LBUTTON:
    case VK_RBUTTON:
    case VK_MBUTTON:
    case VK_XBUTTON1:
    case VK_XBUTTON2:
    case VK_PROCESSKEY:
    case VK_PACKET:
      return false;
    default:
      return virtual_key > 0 && virtual_key < 0xFF;
    }
  }
  void StartRecording(HWND box, BindingBoxState &state) {
    state.is_recording = true;
    state.is_dropping_released_keys = false;
    InvalidateRect(box, nullptr, FALSE);
  }
  void StopRecording(HWND box, BindingBoxState &state, bool by_key) {
    state.is_recording = false;
    state.is_dropping_released_keys = by_key;
    InvalidateRect(box, nullptr, FALSE);
  }
  void OnRecordingKeyDown(HWND box, BindingBoxState &state, UINT virtual_key) {
    const UINT modifiers = GetHeldModifiers();
    if (IsModifierKey(virtual_key)) {
      InvalidateRect(box, nullptr, FALSE); // shows the modifiers held so far
      return;
    }
    if (modifiers == 0 && virtual_key == VK_ESCAPE) {
      StopRecording(box, state, true);
      return;
    }
    if (modifiers == 0 && virtual_key == VK_BACK) {
      state.binding = {};
      StopRecording(box, state, true);
      return;
    }
    if ((modifiers & ~MOD_SHIFT) == 0 && virtual_key == VK_TAB) {
      StopRecording(box, state, false);
      SendMessageW(GetParent(box), WM_NEXTDLGCTL, (modifiers & MOD_SHIFT) ? 1 : 0, FALSE);
      return;
    }
    const KeyBinding pressed{modifiers, virtual_key};
    if (!IsBindableKey(virtual_key) || !IsAllowedKeyBinding(pressed)) {
      MessageBeep(MB_ICONWARNING);
      return;
    }
    state.binding = pressed;
    StopRecording(box, state, true);
  }
  std::wstring GetDisplayText(const BindingBoxState &state) {
    if (!state.is_recording) {
      return FormatKeyBinding(state.binding);
    }
    const UINT modifiers = GetHeldModifiers();
    if (modifiers == 0) {
      return L"Press the new keys";
    }
    return FormatModifiers(modifiers) + L"…";
  }
  void Paint(HWND box, const BindingBoxState &state) {
    PAINTSTRUCT paint;
    const HDC dc = BeginPaint(box, &paint);
    RECT rect;
    GetClientRect(box, &rect);
    const HGDIOBJ brush = GetStockObject(DC_BRUSH);
    SetDCBrushColor(dc, GetSysColor(state.is_recording ? COLOR_HIGHLIGHT : COLOR_BTNSHADOW));
    FillRect(dc, &rect, static_cast<HBRUSH>(brush));
    const int border = Scale(state.is_recording ? kRecordingBorderWidth : kBorderWidth, box);
    InflateRect(&rect, -border, -border);
    SetDCBrushColor(dc, GetSysColor(COLOR_BTNFACE));
    FillRect(dc, &rect, static_cast<HBRUSH>(brush));
    const bool is_prompt = state.is_recording || !IsWindowEnabled(box);
    SetTextColor(dc, GetSysColor(is_prompt ? COLOR_GRAYTEXT : COLOR_BTNTEXT));
    SetBkMode(dc, TRANSPARENT);
    const HGDIOBJ old_font = SelectObject(dc, state.font != nullptr ? state.font : GetStockObject(DEFAULT_GUI_FONT));
    RECT text_rect = rect;
    InflateRect(&text_rect, -Scale(kTextPadding, box), 0);
    const std::wstring text = GetDisplayText(state);
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &text_rect, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX | DT_END_ELLIPSIS);
    SelectObject(dc, old_font);
    const auto ui_state = static_cast<UINT>(SendMessageW(box, WM_QUERYUISTATE, 0, 0));
    if (GetFocus() == box && !state.is_recording && (ui_state & UISF_HIDEFOCUS) == 0) {
      InflateRect(&rect, -1, -1);
      DrawFocusRect(dc, &rect);
    }
    EndPaint(box, &paint);
  }
  LRESULT CALLBACK BindingBoxProc(HWND box, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_NCCREATE) {
      SetWindowLongPtrW(box, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(new BindingBoxState{}));
      return DefWindowProcW(box, message, wparam, lparam);
    }
    BindingBoxState *state = GetState(box);
    if (state == nullptr) {
      return DefWindowProcW(box, message, wparam, lparam);
    }
    switch (message) {
    case WM_NCDESTROY:
      SetWindowLongPtrW(box, GWLP_USERDATA, 0);
      delete state;
      break;
    case WM_SETFONT:
      state->font = reinterpret_cast<HFONT>(wparam);
      if (LOWORD(lparam) != 0) {
        InvalidateRect(box, nullptr, FALSE);
      }
      return 0;
    case WM_GETFONT:
      return reinterpret_cast<LRESULT>(state->font);
    case WM_ERASEBKGND:
      return 1; // WM_PAINT fills the whole box
    case WM_PAINT:
      Paint(box, *state);
      return 0;
    case WM_ENABLE:
    case WM_SETFOCUS:
    case WM_UPDATEUISTATE:
      InvalidateRect(box, nullptr, FALSE);
      break;
    case WM_KILLFOCUS:
      StopRecording(box, *state, false);
      break;
    case WM_LBUTTONDOWN:
      SetFocus(box);
      StartRecording(box, *state);
      return 0;
    case WM_GETDLGCODE: {
      // While recording, every key comes here instead of moving focus or pressing a button.
      if (state->is_recording || state->is_dropping_released_keys) {
        return DLGC_WANTALLKEYS | DLGC_WANTMESSAGE | DLGC_WANTCHARS;
      }
      const auto *pending = reinterpret_cast<const MSG *>(lparam);
      if (pending != nullptr && pending->message == WM_KEYDOWN && pending->wParam == VK_RETURN) {
        return DLGC_WANTMESSAGE; // Enter starts recording, like it presses a focused button
      }
      return 0;
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
      const bool is_repeat = (lparam & (1 << 30)) != 0;
      if (state->is_recording) {
        if (!is_repeat) {
          OnRecordingKeyDown(box, *state, static_cast<UINT>(wparam));
        }
        return 0;
      }
      const bool is_start_key = wparam == VK_SPACE || wparam == VK_RETURN;
      if (message == WM_KEYDOWN && is_start_key && !is_repeat && GetHeldModifiers() == 0) {
        StartRecording(box, *state);
        return 0;
      }
      break;
    }
    case WM_KEYUP:
    case WM_SYSKEYUP:
      if (state->is_recording) {
        // Print Screen only ever sends a key-up.
        if (wparam == VK_SNAPSHOT) {
          OnRecordingKeyDown(box, *state, VK_SNAPSHOT);
        } else {
          InvalidateRect(box, nullptr, FALSE);
        }
        return 0;
      }
      if (state->is_dropping_released_keys) {
        state->is_dropping_released_keys = GetHeldModifiers() != 0;
        return 0;
      }
      break;
    case WM_CHAR:
    case WM_DEADCHAR:
    case WM_SYSCHAR:
    case WM_SYSDEADCHAR:
      if (state->is_recording || state->is_dropping_released_keys) {
        return 0;
      }
      break;
    default:
      break;
    }
    return DefWindowProcW(box, message, wparam, lparam);
  }
} // namespace
bool RegisterBindingBoxClass(HINSTANCE instance) {
  WNDCLASSEXW window_class = {sizeof(window_class)};
  window_class.lpfnWndProc = BindingBoxProc;
  window_class.hInstance = instance;
  window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  window_class.lpszClassName = kClassName;
  return RegisterClassExW(&window_class) != 0;
}
void SetBindingBoxValue(HWND box, const KeyBinding &binding) {
  if (BindingBoxState *state = GetState(box)) {
    state->binding = binding;
    InvalidateRect(box, nullptr, FALSE);
  }
}
KeyBinding GetBindingBoxValue(HWND box) {
  const BindingBoxState *state = GetState(box);
  return state != nullptr ? state->binding : KeyBinding{};
}
} // namespace pawse
