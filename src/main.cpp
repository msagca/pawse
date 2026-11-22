#include <windows.h>
#include <app.hpp>
#include <cwchar>
#include <winrt/base.h>
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR command_line, int) {
  winrt::init_apartment(winrt::apartment_type::single_threaded);
  const auto has_option = [command_line](const wchar_t *option) {
    return command_line != nullptr && std::wcsstr(command_line, option) != nullptr;
  };
  pawse::StartReason reason = pawse::StartReason::kUser;
  if (has_option(L"--updated")) {
    reason = pawse::StartReason::kUpdated;
  } else if (has_option(L"--autostart")) {
    reason = pawse::StartReason::kSignIn;
  }
  pawse::App app;
  return app.Run(instance, reason);
}
