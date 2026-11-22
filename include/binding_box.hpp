#pragma once
#include <windows.h>
#include <key_binding.hpp>
namespace pawse {
// A read-only box that records a key binding, of the window class BINDING_BOX_CLASS_NAME. Click
// it, or press Space or Enter while it has focus, and its border lights up until a key is
// pressed: Esc cancels, Backspace clears and any other key becomes the binding. Keys that
// IsAllowedKeyBinding refuses beep and keep it waiting. Losing focus or pressing Tab cancels.
bool RegisterBindingBoxClass(HINSTANCE instance);
void SetBindingBoxValue(HWND box, const KeyBinding &binding);
KeyBinding GetBindingBoxValue(HWND box);
} // namespace pawse
