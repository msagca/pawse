#include <windows.h>

#include <cwchar>

#include <winrt/base.h>

#include "app.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR command_line, int) {
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    const bool started_at_sign_in =
        command_line != nullptr && std::wcsstr(command_line, L"--autostart") != nullptr;
    pawse::App app;
    return app.Run(instance, started_at_sign_in);
}
