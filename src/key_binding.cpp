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
  return FormatModifiers(binding.modifiers) + GetKeyName(binding.virtual_key);
}
std::wstring FormatModifiers(UINT modifiers) {
  std::wstring text;
  if (modifiers & MOD_CONTROL) {
    text += L"Ctrl+";
  }
  if (modifiers & MOD_ALT) {
    text += L"Alt+";
  }
  if (modifiers & MOD_SHIFT) {
    text += L"Shift+";
  }
  if (modifiers & MOD_WIN) {
    text += L"Win+";
  }
  return text;
}
bool IsModifierKey(UINT virtual_key) {
  switch (virtual_key) {
  case VK_SHIFT:
  case VK_CONTROL:
  case VK_MENU:
  case VK_LSHIFT:
  case VK_RSHIFT:
  case VK_LCONTROL:
  case VK_RCONTROL:
  case VK_LMENU:
  case VK_RMENU:
  case VK_LWIN:
  case VK_RWIN:
    return true;
  default:
    return false;
  }
}
bool IsAllowedKeyBinding(const KeyBinding &binding) {
  if (binding.IsEmpty() || IsModifierKey(binding.virtual_key)) {
    return false;
  }
  if (binding.modifiers & (MOD_CONTROL | MOD_ALT | MOD_WIN)) {
    return true;
  }
  const UINT key = binding.virtual_key;
  return (key >= VK_F1 && key <= VK_F24) || key == VK_PAUSE || key == VK_SCROLL;
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
  if (!has_known_modifiers || !has_valid_key || !IsAllowedKeyBinding({modifiers, virtual_key})) {
    return fallback;
  }
  return KeyBinding{modifiers, virtual_key};
}
} // namespace pawse
