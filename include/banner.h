#pragma once

#include <windows.h>

#include <string>

namespace pawse {

// A small, translucent, click-through notice at the top of the primary screen that says what is
// locked and how to unlock it. It never takes focus.
class Banner {
public:
    Banner() = default;
    Banner(const Banner&) = delete;
    Banner& operator=(const Banner&) = delete;
    ~Banner();

    bool Create(HINSTANCE instance);
    void Show(const std::wstring& title, const std::wstring& detail);
    void Hide();

private:
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    void UpdateFonts(UINT dpi);
    void Layout();
    void Paint(HDC device_context, const RECT& client_area) const;

    HWND window_ = nullptr;
    HFONT title_font_ = nullptr;
    HFONT detail_font_ = nullptr;
    UINT font_dpi_ = 0;
    std::wstring title_;
    std::wstring detail_;
};

}  // namespace pawse
