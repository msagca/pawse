#pragma once

#include <windows.h>

#include <compare>
#include <string>

namespace pawse {

struct Version {
    UINT major = 0;
    UINT minor = 0;
    UINT patch = 0;

    auto operator<=>(const Version&) const = default;
};

Version GetCurrentVersion();
std::wstring FormatVersion(const Version& version);

// Versions travel in a WPARAM between threads.
WPARAM PackVersion(const Version& version);
Version UnpackVersion(WPARAM packed);

enum class UpdateDownloadResult : WPARAM {
    kVerified,
    kDownloadFailed,
    kSignatureInvalid,
    kSaveFailed,
};

// Updates come from the GitHub releases of Pawse. Each release has the installer and a signature
// file next to it (see installer/README.md). The work runs on a background thread, and the
// result is posted to the notify window.

// Looks up the latest release. Posts kMessageUpdateChecked.
void StartUpdateCheck(HWND notify_window);

// Downloads the installer for the given version and checks its signature. Posts
// kMessageUpdateDownloaded.
void StartUpdateDownload(HWND notify_window, Version version);

// Runs a verified installer silently. It updates Pawse in place and starts it again.
bool StartInstaller(const std::wstring& installer_path);

// Removes installers left over from earlier updates.
void DeleteDownloadedUpdates();

}  // namespace pawse
