#include <windows.h>
#include <UserConsentVerifierInterop.h>
#include <common.hpp>
#include <hello_verifier.hpp>
namespace pawse {
namespace {
  using winrt::Windows::Foundation::AsyncStatus;
  using winrt::Windows::Foundation::IAsyncOperation;
  using winrt::Windows::Security::Credentials::UI::UserConsentVerificationResult;
  using winrt::Windows::Security::Credentials::UI::UserConsentVerifier;
  using winrt::Windows::Security::Credentials::UI::UserConsentVerifierAvailability;
  HelloResult ToHelloResult(UserConsentVerificationResult result) {
    switch (result) {
    case UserConsentVerificationResult::Verified:
      return HelloResult::kVerified;
    case UserConsentVerificationResult::DeviceNotPresent:
    case UserConsentVerificationResult::NotConfiguredForUser:
    case UserConsentVerificationResult::DisabledByPolicy:
      return HelloResult::kUnavailable;
    case UserConsentVerificationResult::Canceled:
      return HelloResult::kCanceled;
    default:
      return HelloResult::kFailed;
    }
  }
  void PostAvailability(HWND notify_window, bool available) {
    PostMessageW(notify_window, kMessageHelloAvailability, available ? 1 : 0, 0);
  }
} // namespace
void QueryHelloAvailability(HWND notify_window) {
  try {
    UserConsentVerifier::CheckAvailabilityAsync().Completed(
      [notify_window](const IAsyncOperation<UserConsentVerifierAvailability> &operation,
        AsyncStatus status) {
        bool available = false;
        if (status == AsyncStatus::Completed) {
          try {
            available =
              operation.GetResults() == UserConsentVerifierAvailability::Available;
          } catch (const winrt::hresult_error &) {
          }
        }
        PostAvailability(notify_window, available);
      });
  } catch (const winrt::hresult_error &) {
    PostAvailability(notify_window, false);
  }
}
bool HelloVerifier::Begin(HWND owner_window, const std::wstring &message, HWND notify_window) {
  Abandon();
  const LPARAM attempt_id = ++attempt_id_;
  try {
    // The interop interface ties the prompt to a desktop window; the plain WinRT call
    // expects an app window (CoreWindow) that a desktop app doesn't have.
    const auto interop =
      winrt::get_activation_factory<UserConsentVerifier, IUserConsentVerifierInterop>();
    const winrt::hstring prompt_message{message};
    const winrt::guid operation_interface =
      winrt::guid_of<IAsyncOperation<UserConsentVerificationResult>>();
    IAsyncOperation<UserConsentVerificationResult> operation{nullptr};
    winrt::check_hresult(interop->RequestVerificationForWindowAsync(
      owner_window,
      static_cast<HSTRING>(winrt::get_abi(prompt_message)),
      reinterpret_cast<const IID &>(operation_interface),
      winrt::put_abi(operation)));
    operation.Completed(
      [notify_window, attempt_id](
        const IAsyncOperation<UserConsentVerificationResult> &finished_operation,
        AsyncStatus status) {
        HelloResult result = HelloResult::kFailed;
        if (status == AsyncStatus::Completed) {
          try {
            result = ToHelloResult(finished_operation.GetResults());
          } catch (const winrt::hresult_error &) {
          }
        } else if (status == AsyncStatus::Canceled) {
          result = HelloResult::kCanceled;
        }
        PostMessageW(notify_window, kMessageHelloResult, static_cast<WPARAM>(result), attempt_id);
      });
    operation_ = operation;
    active_ = true;
    return true;
  } catch (const winrt::hresult_error &) {
    return false;
  }
}
void HelloVerifier::Abandon() {
  if (operation_ != nullptr) {
    try {
      operation_.Cancel();
    } catch (const winrt::hresult_error &) {
    }
  }
  operation_ = nullptr;
  active_ = false;
  ++attempt_id_;
}
bool HelloVerifier::Finish(LPARAM attempt_id) {
  if (!active_ || attempt_id != attempt_id_) {
    return false;
  }
  operation_ = nullptr;
  active_ = false;
  return true;
}
} // namespace pawse
