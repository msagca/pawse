#pragma once

#include <windows.h>

#include <string>

namespace pawse {

// A translucent, click-through message in the middle of the screen that says what is locked and
// how to unlock it. It never takes focus.
class Banner {
public:
    Banner() = default;
    Banner(const Banner&) = delete;
    Banner& operator=(const Banner&) = delete;
    ~Banner();

    bool Create(HINSTANCE instance);
    // The accent color marks the stripe on the left edge.
    void Show(const std::wstring& title, const std::wstring& detail, COLORREF accent_color);
    void Hide();
    bool IsVisible() const { return window_ != nullptr && IsWindowVisible(window_); }

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
    COLORREF accent_color_ = 0;
};

}  // namespace pawse
