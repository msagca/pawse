#include <array>
#include <bcrypt.h>
#include <common.hpp>
#include <cstring>
#include <filesystem>
#include <memory>
#include <optional>
#include <shellapi.h>
#include <string_view>
#include <thread>
#include <updater.hpp>
#include <version.hpp>
#include <winhttp.h>
namespace pawse {
namespace {
  constexpr wchar_t kHost[] = L"github.com";
  constexpr wchar_t kLatestReleasePath[] = L"/msagca/pawse/releases/latest";
  constexpr wchar_t kDownloadPath[] = L"/msagca/pawse/releases/download/";
  constexpr int kTimeoutMilliseconds = 30 * 1000;
  constexpr size_t kMaxInstallerBytes = 64 * 1024 * 1024;
  constexpr size_t kMaxSignatureBytes = 1024;
  // The public half of the release signing key, an ECDSA P-256 key: the X and Y coordinates of
  // installer/release-signing-key.pub.pem, which is the same key in PEM form.
  constexpr std::array<BYTE, 64> kReleaseSigningKey = {
    0x9e,
    0x62,
    0x18,
    0xad,
    0x26,
    0x69,
    0xcd,
    0x2e,
    0x3e,
    0x67,
    0x87,
    0xa7,
    0xe1,
    0x8d,
    0xf7,
    0x06,
    0xdd,
    0x74,
    0x75,
    0xcb,
    0xb1,
    0xcb,
    0x05,
    0xda,
    0xe8,
    0xaf,
    0x08,
    0xb6,
    0xa8,
    0x99,
    0x8d,
    0x2e,
    0x58,
    0x10,
    0xc4,
    0x38,
    0xd1,
    0x1f,
    0x8c,
    0x11,
    0x8a,
    0x6e,
    0xeb,
    0x48,
    0xac,
    0x76,
    0x97,
    0x46,
    0x47,
    0xf6,
    0x58,
    0x24,
    0xad,
    0xb9,
    0xa9,
    0xe2,
    0x73,
    0x4a,
    0x95,
    0xa8,
    0x71,
    0x0c,
    0x20,
    0xd1,
  };
  struct InternetHandleCloser {
    void operator()(HINTERNET handle) const {
      WinHttpCloseHandle(handle);
    }
  };
  using InternetHandle = std::unique_ptr<void, InternetHandleCloser>;
  struct Connection {
    InternetHandle session;
    InternetHandle connection;
  };
  std::optional<Connection> Connect() {
    const std::wstring user_agent = L"Pawse/" + FormatVersion(GetCurrentVersion());
    InternetHandle session(WinHttpOpen(user_agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) {
      return std::nullopt;
    }
    WinHttpSetTimeouts(session.get(), kTimeoutMilliseconds, kTimeoutMilliseconds, kTimeoutMilliseconds, kTimeoutMilliseconds);
    InternetHandle connection(WinHttpConnect(session.get(), kHost, INTERNET_DEFAULT_HTTPS_PORT, 0));
    if (!connection) {
      return std::nullopt;
    }
    return Connection{std::move(session), std::move(connection)};
  }
  // Sends a GET request over HTTPS and waits for the response headers.
  InternetHandle SendRequest(const Connection &connection, const std::wstring &path, bool follow_redirects) {
    InternetHandle request(WinHttpOpenRequest(connection.connection.get(), L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    if (!request) {
      return nullptr;
    }
    if (!follow_redirects) {
      DWORD feature = WINHTTP_DISABLE_REDIRECTS;
      WinHttpSetOption(request.get(), WINHTTP_OPTION_DISABLE_FEATURE, &feature, sizeof(feature));
    }
    if (!WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.get(), nullptr)) {
      return nullptr;
    }
    return request;
  }
  DWORD GetStatusCode(HINTERNET request) {
    DWORD status = 0;
    DWORD size = sizeof(status);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
    return status;
  }
  std::wstring GetLocationHeader(HINTERNET request) {
    DWORD size = 0;
    WinHttpQueryHeaders(request, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &size, WINHTTP_NO_HEADER_INDEX);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || size == 0) {
      return {};
    }
    std::wstring location(size / sizeof(wchar_t), L'\0');
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX, location.data(), &size, WINHTTP_NO_HEADER_INDEX)) {
      return {};
    }
    location.resize(size / sizeof(wchar_t));
    return location;
  }
  // Downloads a file, following redirects to GitHub's file servers. Fails if it is bigger than
  // max_bytes.
  std::optional<std::string> Download(const Connection &connection, const std::wstring &path, size_t max_bytes) {
    const InternetHandle request = SendRequest(connection, path, true);
    if (!request || GetStatusCode(request.get()) != 200) {
      return std::nullopt;
    }
    std::string body;
    for (;;) {
      DWORD available = 0;
      if (!WinHttpQueryDataAvailable(request.get(), &available)) {
        return std::nullopt;
      }
      if (available == 0) {
        return body;
      }
      if (body.size() + available > max_bytes) {
        return std::nullopt;
      }
      const size_t offset = body.size();
      body.resize(offset + available);
      DWORD read = 0;
      if (!WinHttpReadData(request.get(), body.data() + offset, available, &read)) {
        return std::nullopt;
      }
      body.resize(offset + read);
    }
  }
  bool ParseNumber(std::wstring_view &text, UINT &number) {
    size_t length = 0;
    number = 0;
    while (length < text.size() && length < 5 && text[length] >= L'0' && text[length] <= L'9') {
      number = number * 10 + (text[length] - L'0');
      ++length;
    }
    text.remove_prefix(length);
    return length > 0 && number <= 0xFFFF;
  }
  // Reads a release tag such as "v1.2.3". Anything else, such as a pre-release suffix, is rejected.
  std::optional<Version> ParseTag(std::wstring_view tag) {
    if (tag.empty() || tag.front() != L'v') {
      return std::nullopt;
    }
    tag.remove_prefix(1);
    Version version;
    UINT *parts[] = {&version.major, &version.minor, &version.patch};
    for (size_t index = 0; index < 3; ++index) {
      if (index > 0) {
        if (tag.empty() || tag.front() != L'.') {
          return std::nullopt;
        }
        tag.remove_prefix(1);
      }
      if (!ParseNumber(tag, *parts[index])) {
        return std::nullopt;
      }
    }
    if (!tag.empty()) {
      return std::nullopt;
    }
    return version;
  }
  // GitHub answers /releases/latest with a redirect to /releases/tag/<tag>.
  std::optional<Version> FindLatestVersion() {
    const std::optional<Connection> connection = Connect();
    if (!connection) {
      return std::nullopt;
    }
    const InternetHandle request = SendRequest(*connection, kLatestReleasePath, false);
    if (!request) {
      return std::nullopt;
    }
    const DWORD status = GetStatusCode(request.get());
    if (status < 300 || status >= 400) {
      return std::nullopt;
    }
    const std::wstring location = GetLocationHeader(request.get());
    constexpr std::wstring_view kTagMarker = L"/releases/tag/";
    const size_t marker = location.rfind(kTagMarker);
    if (marker == std::wstring::npos) {
      return std::nullopt;
    }
    return ParseTag(std::wstring_view(location).substr(marker + kTagMarker.size()));
  }
  std::wstring GetInstallerName(const Version &version) {
    return L"pawse-" + FormatVersion(version) + L"-x64-setup.exe";
  }
  std::optional<std::array<BYTE, 32>> Sha256(std::string_view data) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (!BCRYPT_SUCCESS(
          BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0))) {
      return std::nullopt;
    }
    std::array<BYTE, 32> digest{};
    const NTSTATUS status = BCryptHash(
      algorithm,
      nullptr,
      0,
      reinterpret_cast<PUCHAR>(const_cast<char *>(data.data())),
      static_cast<ULONG>(data.size()),
      digest.data(),
      static_cast<ULONG>(digest.size()));
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (!BCRYPT_SUCCESS(status)) {
      return std::nullopt;
    }
    return digest;
  }
  // OpenSSL writes ECDSA signatures in DER: SEQUENCE { INTEGER r, INTEGER s }. BCrypt wants r and s
  // as 32-byte big-endian numbers, one after the other.
  std::optional<std::array<BYTE, 64>> ConvertDerSignature(std::string_view der) {
    const auto byte_at = [&](size_t index) -> int {
      return index < der.size() ? static_cast<BYTE>(der[index]) : -1;
    };
    if (byte_at(0) != 0x30 || byte_at(1) < 0 || byte_at(1) >= 0x80 ||
        static_cast<size_t>(byte_at(1)) + 2 != der.size()) {
      return std::nullopt;
    }
    std::array<BYTE, 64> raw{};
    size_t position = 2;
    for (size_t part = 0; part < 2; ++part) {
      const int length = byte_at(position + 1);
      if (byte_at(position) != 0x02 || length <= 0 || length >= 0x80 ||
          position + 2 + length > der.size()) {
        return std::nullopt;
      }
      std::string_view number = der.substr(position + 2, length);
      while (!number.empty() && number.front() == '\0') {
        number.remove_prefix(1);
      }
      if (number.size() > 32) {
        return std::nullopt;
      }
      std::memcpy(raw.data() + part * 32 + (32 - number.size()), number.data(), number.size());
      position += 2 + length;
    }
    if (position != der.size()) {
      return std::nullopt;
    }
    return raw;
  }
  // CI signs a line in sha256sum format, "<hash>  <file name>\n", so the signature also ties the
  // installer to its version: an older signed installer can't pass for a newer one.
  bool IsSignatureValid(std::string_view installer, const std::wstring &installer_name, std::string_view der_signature) {
    const std::optional<std::array<BYTE, 32>> installer_hash = Sha256(installer);
    const std::optional<std::array<BYTE, 64>> signature = ConvertDerSignature(der_signature);
    if (!installer_hash || !signature) {
      return false;
    }
    constexpr char kHexDigits[] = "0123456789abcdef";
    std::string signed_line;
    for (const BYTE value : *installer_hash) {
      signed_line += kHexDigits[value >> 4];
      signed_line += kHexDigits[value & 0x0F];
    }
    signed_line += "  ";
    for (const wchar_t character : installer_name) {
      signed_line += static_cast<char>(character); // the name is plain ASCII
    }
    signed_line += '\n';
    const std::optional<std::array<BYTE, 32>> line_hash = Sha256(signed_line);
    if (!line_hash) {
      return false;
    }
    std::array<BYTE, sizeof(BCRYPT_ECCKEY_BLOB) + kReleaseSigningKey.size()> key_blob{};
    const BCRYPT_ECCKEY_BLOB header = {BCRYPT_ECDSA_PUBLIC_P256_MAGIC, 32};
    std::memcpy(key_blob.data(), &header, sizeof(header));
    std::memcpy(key_blob.data() + sizeof(header), kReleaseSigningKey.data(), kReleaseSigningKey.size());
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (!BCRYPT_SUCCESS(
          BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0))) {
      return false;
    }
    BCRYPT_KEY_HANDLE key = nullptr;
    bool valid = false;
    if (BCRYPT_SUCCESS(BCryptImportKeyPair(algorithm, nullptr, BCRYPT_ECCPUBLIC_BLOB, &key, key_blob.data(), static_cast<ULONG>(key_blob.size()), 0))) {
      auto signature_bytes = *signature;
      auto digest = *line_hash;
      valid = BCRYPT_SUCCESS(BCryptVerifySignature(
        key,
        nullptr,
        digest.data(),
        static_cast<ULONG>(digest.size()),
        signature_bytes.data(),
        static_cast<ULONG>(signature_bytes.size()),
        0));
      BCryptDestroyKey(key);
    }
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return valid;
  }
  std::filesystem::path GetUpdatesDirectory() {
    wchar_t temp_path[MAX_PATH + 1] = {};
    if (GetTempPathW(ARRAYSIZE(temp_path), temp_path) == 0) {
      return {};
    }
    return std::filesystem::path(temp_path) / L"Pawse" / L"Updates";
  }
  bool SaveFile(const std::filesystem::path &path, std::string_view contents) {
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
      return false;
    }
    DWORD written = 0;
    const bool saved =
      WriteFile(file, contents.data(), static_cast<DWORD>(contents.size()), &written, nullptr) &&
      written == contents.size();
    CloseHandle(file);
    if (!saved) {
      DeleteFileW(path.c_str());
    }
    return saved;
  }
  void PostDownloadResult(HWND notify_window, UpdateDownloadResult result, std::unique_ptr<std::wstring> installer_path = nullptr) {
    if (PostMessageW(notify_window, kMessageUpdateDownloaded, static_cast<WPARAM>(result), reinterpret_cast<LPARAM>(installer_path.get()))) {
      static_cast<void>(installer_path.release()); // the window owns it now
    }
  }
  void DownloadUpdate(HWND notify_window, Version version) {
    const std::wstring installer_name = GetInstallerName(version);
    const std::wstring release_path =
      kDownloadPath + std::wstring(L"v") + FormatVersion(version) + L"/" + installer_name;
    const std::optional<Connection> connection = Connect();
    std::optional<std::string> installer;
    std::optional<std::string> signature;
    if (connection) {
      installer = Download(*connection, release_path, kMaxInstallerBytes);
      signature = Download(*connection, release_path + L".sig", kMaxSignatureBytes);
    }
    if (!installer || !signature) {
      PostDownloadResult(notify_window, UpdateDownloadResult::kDownloadFailed);
      return;
    }
    if (!IsSignatureValid(*installer, installer_name, *signature)) {
      PostDownloadResult(notify_window, UpdateDownloadResult::kSignatureInvalid);
      return;
    }
    const std::filesystem::path directory = GetUpdatesDirectory();
    std::error_code error;
    if (directory.empty() || (!std::filesystem::create_directories(directory, error) && error)) {
      PostDownloadResult(notify_window, UpdateDownloadResult::kSaveFailed);
      return;
    }
    const std::filesystem::path installer_path = directory / installer_name;
    if (!SaveFile(installer_path, *installer)) {
      PostDownloadResult(notify_window, UpdateDownloadResult::kSaveFailed);
      return;
    }
    PostDownloadResult(notify_window, UpdateDownloadResult::kVerified, std::make_unique<std::wstring>(installer_path.wstring()));
  }
} // namespace
Version GetCurrentVersion() {
  return Version{PAWSE_VERSION_MAJOR, PAWSE_VERSION_MINOR, PAWSE_VERSION_PATCH};
}
std::wstring FormatVersion(const Version &version) {
  return std::to_wstring(version.major) + L"." + std::to_wstring(version.minor) + L"." +
         std::to_wstring(version.patch);
}
WPARAM PackVersion(const Version &version) {
  return (static_cast<WPARAM>(version.major) << 32) | (static_cast<WPARAM>(version.minor) << 16) |
         version.patch;
}
Version UnpackVersion(WPARAM packed) {
  return Version{static_cast<UINT>(packed >> 32), static_cast<UINT>((packed >> 16) & 0xFFFF), static_cast<UINT>(packed & 0xFFFF)};
}
void StartUpdateCheck(HWND notify_window) {
  // Detached: a slow network must never hold up Pawse, not even when it exits.
  std::thread([notify_window] {
    const std::optional<Version> latest = FindLatestVersion();
    PostMessageW(notify_window, kMessageUpdateChecked, latest ? PackVersion(*latest) : 0, 0);
  }).detach();
}
void StartUpdateDownload(HWND notify_window, Version version) {
  std::thread(DownloadUpdate, notify_window, version).detach();
}
bool StartInstaller(const std::wstring &installer_path) {
  // /PAWSEUPDATE=yes makes setup start Pawse again when it's done (see installer/pawse.iss).
  SHELLEXECUTEINFOW execute_info = {sizeof(execute_info)};
  execute_info.fMask = SEE_MASK_NOASYNC;
  execute_info.lpVerb = L"open";
  execute_info.lpFile = installer_path.c_str();
  execute_info.lpParameters = L"/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /PAWSEUPDATE=yes";
  execute_info.nShow = SW_SHOWNORMAL;
  return ShellExecuteExW(&execute_info) != FALSE;
}
void DeleteDownloadedUpdates() {
  const std::filesystem::path directory = GetUpdatesDirectory();
  if (directory.empty()) {
    return;
  }
  std::error_code error;
  for (const auto &entry : std::filesystem::directory_iterator(directory, error)) {
    std::filesystem::remove(entry.path(), error); // an installer still running stays
  }
}
} // namespace pawse
