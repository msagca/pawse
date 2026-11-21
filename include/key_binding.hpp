#pragma once
#include <windows.h>
#include <string>
namespace pawse {
// A key combination such as Ctrl+Alt+K, in the form RegisterHotKey expects.
struct KeyBinding {
  UINT modifiers = 0; // MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN
  UINT virtual_key = 0; // VK_* code; 0 means no binding
  bool IsEmpty() const {
    return virtual_key == 0;
  }
  bool operator==(const KeyBinding &) const = default;
};
// Human-readable text such as "Ctrl+Alt+K", using the keyboard layout's key names.
std::wstring FormatKeyBinding(const KeyBinding &binding);
// Conversions to and from the value a hotkey control (msctls_hotkey32) uses.
WORD KeyBindingToHotkeyControlValue(const KeyBinding &binding);
KeyBinding KeyBindingFromHotkeyControlValue(WORD value);
// Text form stored in the settings file: "<modifiers>,<virtual key>".
std::wstring SerializeKeyBinding(const KeyBinding &binding);
KeyBinding ParseKeyBinding(const std::wstring &text, const KeyBinding &fallback);
} // namespace pawse
