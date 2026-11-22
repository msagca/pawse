#pragma once
#include <string>
#include <windows.h>
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
// Shift, Ctrl, Alt and Win, on either side.
bool IsModifierKey(UINT virtual_key);
// Bindings without Ctrl, Alt or Win are too easy to press by accident (or by paw), so only keys
// that are rarely typed can go without: F1 to F24, Pause and Scroll Lock.
bool IsAllowedKeyBinding(const KeyBinding &binding);
// Human-readable text such as "Ctrl+Alt+K", using the keyboard layout's key names.
std::wstring FormatKeyBinding(const KeyBinding &binding);
// The modifier part of that text, such as "Ctrl+Alt+".
std::wstring FormatModifiers(UINT modifiers);
// Text form stored in the settings file: "<modifiers>,<virtual key>".
std::wstring SerializeKeyBinding(const KeyBinding &binding);
KeyBinding ParseKeyBinding(const std::wstring &text, const KeyBinding &fallback);
} // namespace pawse
