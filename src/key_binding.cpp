#include <commctrl.h>
#include <cwchar>
#include <key_binding.hpp>
namespace pawse {
namespace {
  constexpr UINT kKnownModifiers = MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN;
  // Keys whose scan code needs the "extended" flag to get the right name.
  bool IsExtendedKey(UINT virtual_key) {
    switch (virtual_key) {
    case VK_INSERT:
    case VK_DELETE:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_UP:
    case VK_DOWN:
    case VK_DIVIDE:
    case VK_NUMLOCK:
    case VK_SNAPSHOT:
    case VK_RCONTROL:
    case VK_RMENU:
    case VK_APPS:
    case VK_LWIN:
    case VK_RWIN:
      return true;
    default:
      return false;
    }
  }
  std::wstring GetKeyName(UINT virtual_key) {
    const UINT scan_code = MapVirtualKeyW(virtual_key, MAPVK_VK_TO_VSC);
    LONG key_name_parameter = static_cast<LONG>(scan_code << 16);
    if (IsExtendedKey(virtual_key)) {
      key_name_parameter |= 1 << 24;
    }
    wchar_t key_name[64] = {};
    if (scan_code != 0 && GetKeyNameTextW(key_name_parameter, key_name, ARRAYSIZE(key_name)) > 0) {
      return key_name;
    }
    wchar_t fallback_name[16] = {};
    swprintf_s(fallback_name, L"Key %02X", virtual_key);
    return fallback_name;
  }
} // namespace
std::wstring FormatKeyBinding(const KeyBinding &binding) {
  if (binding.IsEmpty()) {
    return L"(none)";
  }
  std::wstring text;
  if (binding.modifiers & MOD_CONTROL) {
    text += L"Ctrl+";
  }
  if (binding.modifiers & MOD_ALT) {
    text += L"Alt+";
  }
  if (binding.modifiers & MOD_SHIFT) {
    text += L"Shift+";
  }
  if (binding.modifiers & MOD_WIN) {
    text += L"Win+";
  }
  return text + GetKeyName(binding.virtual_key);
}
WORD KeyBindingToHotkeyControlValue(const KeyBinding &binding) {
  BYTE flags = 0;
  if (binding.modifiers & MOD_SHIFT) {
    flags |= HOTKEYF_SHIFT;
  }
  if (binding.modifiers & MOD_CONTROL) {
    flags |= HOTKEYF_CONTROL;
  }
  if (binding.modifiers & MOD_ALT) {
    flags |= HOTKEYF_ALT;
  }
  if (IsExtendedKey(binding.virtual_key)) {
    flags |= HOTKEYF_EXT;
  }
  return MAKEWORD(static_cast<BYTE>(binding.virtual_key), flags);
}
KeyBinding KeyBindingFromHotkeyControlValue(WORD value) {
  const BYTE flags = HIBYTE(value);
  KeyBinding binding;
  binding.virtual_key = LOBYTE(value);
  if (flags & HOTKEYF_SHIFT) {
    binding.modifiers |= MOD_SHIFT;
  }
  if (flags & HOTKEYF_CONTROL) {
    binding.modifiers |= MOD_CONTROL;
  }
  if (flags & HOTKEYF_ALT) {
    binding.modifiers |= MOD_ALT;
  }
  return binding;
}
std::wstring SerializeKeyBinding(const KeyBinding &binding) {
  wchar_t text[32] = {};
  swprintf_s(text, L"%u,%u", binding.modifiers, binding.virtual_key);
  return text;
}
KeyBinding ParseKeyBinding(const std::wstring &text, const KeyBinding &fallback) {
  UINT modifiers = 0;
  UINT virtual_key = 0;
  if (swscanf_s(text.c_str(), L"%u,%u", &modifiers, &virtual_key) != 2) {
    return fallback;
  }
  const bool has_known_modifiers = (modifiers & ~kKnownModifiers) == 0;
  const bool has_valid_key = virtual_key > 0 && virtual_key < 0xFF;
  if (!has_known_modifiers || !has_valid_key) {
    return fallback;
  }
  return KeyBinding{modifiers, virtual_key};
}
} // namespace pawse
