#pragma once
#include <windows.h>
#include <string>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Security.Credentials.UI.h>
namespace pawse {
enum class HelloResult : WPARAM {
  kVerified, // the user confirmed with their PIN, fingerprint or face
  kUnavailable, // Windows Hello isn't set up or is turned off by policy
  kFailed, // wrong PIN too often, device busy, or an unexpected error
  kCanceled, // the user closed the prompt
};
// Asks Windows whether Windows Hello is set up. Posts kMessageHelloAvailability to the window.
void QueryHelloAvailability(HWND notify_window);
// Shows the Windows Hello prompt for a window and reports the outcome as kMessageHelloResult.
// The PIN never passes through Pawse: Windows shows the prompt and only returns the result.
class HelloVerifier {
public:
  // Returns false if the prompt couldn't be shown at all.
  bool Begin(HWND owner_window, const std::wstring &message, HWND notify_window);
  // Cancels the prompt; a result that arrives later is ignored.
  void Abandon();
  // Call when kMessageHelloResult arrives. Returns false for results of abandoned attempts.
  bool Finish(LPARAM attempt_id);
  bool IsActive() const {
    return active_;
  }
private:
  winrt::Windows::Foundation::IAsyncOperation<
    winrt::Windows::Security::Credentials::UI::UserConsentVerificationResult>
    operation_{nullptr};
  LPARAM attempt_id_ = 0;
  bool active_ = false;
};
} // namespace pawse
