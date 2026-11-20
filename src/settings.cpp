#include "settings.h"

#include <knownfolders.h>
#include <shlobj.h>

#include <algorithm>
#include <string>

namespace pawse {
namespace {

constexpr wchar_t kRunKeyPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValueName[] = L"Pawse";

constexpr wchar_t kBindingsSection[] = L"Bindings";
constexpr wchar_t kUnlockSection[] = L"Unlock";
constexpr wchar_t kBannerSection[] = L"Banner";
constexpr const wchar_t* kBindingKeys[kBindingCount] = {L"Keyboard", L"Mouse", L"KeyboardAndMouse"};

std::wstring GetSettingsPath(bool create_directory) {
    PWSTR roaming_folder = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DEFAULT, nullptr,
                                    &roaming_folder))) {
        return {};
    }
    std::wstring directory = roaming_folder;
    CoTaskMemFree(roaming_folder);
    directory += L"\\Pawse";
    if (create_directory) {
        CreateDirectoryW(directory.c_str(), nullptr);
    }
    return directory + L"\\settings.ini";
}

std::wstring ReadString(const std::wstring& path, const wchar_t* section, const wchar_t* key) {
    wchar_t value[128] = {};
    GetPrivateProfileStringW(section, key, L"", value, ARRAYSIZE(value), path.c_str());
    return value;
}

UINT ReadNumber(const std::wstring& path, const wchar_t* section, const wchar_t* key,
                UINT fallback) {
    return GetPrivateProfileIntW(section, key, static_cast<INT>(fallback), path.c_str());
}

bool WriteString(const std::wstring& path, const wchar_t* section, const wchar_t* key,
                 const std::wstring& value) {
    return WritePrivateProfileStringW(section, key, value.c_str(), path.c_str()) != FALSE;
}

std::wstring GetExecutablePath() {
    std::wstring path(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length =
            GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0) {
            return {};
        }
        if (length < path.size()) {
            path.resize(length);
            return path;
        }
        path.resize(path.size() * 2);
    }
}

}  // namespace

Settings LoadSettings() {
    Settings settings;
    const std::wstring path = GetSettingsPath(false);
    if (path.empty()) {
        return settings;
    }
    for (size_t index = 0; index < kBindingCount; ++index) {
        settings.bindings[index] = ParseKeyBinding(
            ReadString(path, kBindingsSection, kBindingKeys[index]), settings.bindings[index]);
    }
    settings.require_windows_hello =
        ReadNumber(path, kUnlockSection, L"RequireWindowsHello", 0) != 0;
    settings.auto_unlock_minutes =
        std::min(ReadNumber(path, kUnlockSection, L"AutoUnlockMinutes", 0), kMaxAutoUnlockMinutes);
    settings.show_banner = ReadNumber(path, kBannerSection, L"Show", 1) != 0;
    return settings;
}

bool SaveSettings(const Settings& settings) {
    const std::wstring path = GetSettingsPath(true);
    if (path.empty()) {
        return false;
    }
    bool saved = true;
    for (size_t index = 0; index < kBindingCount; ++index) {
        saved &= WriteString(path, kBindingsSection, kBindingKeys[index],
                             SerializeKeyBinding(settings.bindings[index]));
    }
    saved &= WriteString(path, kUnlockSection, L"RequireWindowsHello",
                         settings.require_windows_hello ? L"1" : L"0");
    saved &= WriteString(path, kUnlockSection, L"AutoUnlockMinutes",
                         std::to_wstring(settings.auto_unlock_minutes));
    saved &= WriteString(path, kBannerSection, L"Show", settings.show_banner ? L"1" : L"0");
    return saved;
}

bool IsStartWithWindowsEnabled() {
    return RegGetValueW(HKEY_CURRENT_USER, kRunKeyPath, kRunValueName, RRF_RT_REG_SZ, nullptr,
                        nullptr, nullptr) == ERROR_SUCCESS;
}

bool SetStartWithWindows(bool enabled) {
    if (!enabled) {
        const LSTATUS status = RegDeleteKeyValueW(HKEY_CURRENT_USER, kRunKeyPath, kRunValueName);
        return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
    }
    const std::wstring executable_path = GetExecutablePath();
    if (executable_path.empty()) {
        return false;
    }
    const std::wstring command = L"\"" + executable_path + L"\" --autostart";
    const DWORD byte_count = static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t));
    return RegSetKeyValueW(HKEY_CURRENT_USER, kRunKeyPath, kRunValueName, REG_SZ, command.c_str(),
                           byte_count) == ERROR_SUCCESS;
}

}  // namespace pawse
