#pragma once

#include <array>

#include "common.h"
#include "key_binding.h"

namespace pawse {

constexpr UINT kMaxAutoUnlockMinutes = 24 * 60;

struct Settings {
    // Indexed by BindingIndex.
    std::array<KeyBinding, kBindingCount> bindings = {{
        {MOD_CONTROL | MOD_ALT, 'K'},
        {MOD_CONTROL | MOD_ALT, 'M'},
        {MOD_CONTROL | MOD_ALT, 'L'},
    }};
    bool require_windows_hello = false;
    UINT auto_unlock_minutes = 0;  // 0 means never
    bool show_banner = true;
};

// Settings live in %APPDATA%\Pawse\settings.ini. Missing or invalid values fall back to defaults.
Settings LoadSettings();
bool SaveSettings(const Settings& settings);

// "Start with Windows" is a value under HKCU\...\CurrentVersion\Run, not part of the settings file.
bool IsStartWithWindowsEnabled();
bool SetStartWithWindows(bool enabled);

}  // namespace pawse
