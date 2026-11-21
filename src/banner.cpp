#include "banner.h"

#include <shellscalingapi.h>

#include <algorithm>

namespace pawse {
namespace {

constexpr wchar_t kBannerClassName[] = L"Pawse.Banner";
constexpr BYTE kBannerOpacity = 235;
constexpr COLORREF kBackgroundColor = RGB(32, 32, 36);
constexpr COLORREF kAccentColor = RGB(242, 140, 40);
constexpr COLORREF kTitleColor = RGB(255, 255, 255);
constexpr COLORREF kDetailColor = RGB(200, 200, 206);

// Sizes in pixels at 96 DPI.
constexpr int kHorizontalPadding = 28;
constexpr int kVerticalPadding = 18;
constexpr int kAccentWidth = 5;
constexpr int kLineGap = 4;

int Scale(int value, UINT dpi) { return MulDiv(value, static_cast<int>(dpi), 96); }

HFONT CreateUiFont(int point_size, int weight, UINT dpi) {
    return CreateFontW(-MulDiv(point_size, static_cast<int>(dpi), 72), 0, 0, 0, weight, FALSE,
                       FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

SIZE MeasureText(HDC device_context, HFONT font, const std::wstring& text) {
    const HGDIOBJ previous_font = SelectObject(device_context, font);
    RECT bounds = {};
    DrawTextW(device_context, text.c_str(), static_cast<int>(text.size()), &bounds,
              DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(device_context, previous_font);
    return SIZE{bounds.right - bounds.left, bounds.bottom - bounds.top};
}

}  // namespace

Banner::~Banner() {
    if (window_ != nullptr) {
        DestroyWindow(window_);
    }
    if (title_font_ != nullptr) {
        DeleteObject(title_font_);
    }
    if (detail_font_ != nullptr) {
        DeleteObject(detail_font_);
    }
}

bool Banner::Create(HINSTANCE instance) {
    WNDCLASSEXW window_class = {sizeof(window_class)};
    window_class.lpfnWndProc = WindowProc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.lpszClassName = kBannerClassName;
    RegisterClassExW(&window_class);

    constexpr DWORD kExtendedStyle =
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
    window_ = CreateWindowExW(kExtendedStyle, kBannerClassName, L"Pawse", WS_POPUP, 0, 0, 0, 0,
                              nullptr, nullptr, instance, this);
    if (window_ == nullptr) {
        return false;
    }
    SetLayeredWindowAttributes(window_, 0, kBannerOpacity, LWA_ALPHA);
    return true;
}

void Banner::Show(const std::wstring& title, const std::wstring& detail) {
    if (window_ == nullptr) {
        return;
    }
    title_ = title;
    detail_ = detail;
    Layout();
    InvalidateRect(window_, nullptr, TRUE);
}

void Banner::Hide() {
    if (window_ != nullptr) {
        ShowWindow(window_, SW_HIDE);
    }
}

void Banner::UpdateFonts(UINT dpi) {
    if (dpi == font_dpi_) {
        return;
    }
    if (title_font_ != nullptr) {
        DeleteObject(title_font_);
    }
    if (detail_font_ != nullptr) {
        DeleteObject(detail_font_);
    }
    title_font_ = CreateUiFont(14, FW_SEMIBOLD, dpi);
    detail_font_ = CreateUiFont(10, FW_NORMAL, dpi);
    font_dpi_ = dpi;
}

void Banner::Layout() {
    // The screen the user is most likely looking at: the one with the mouse pointer.
    POINT cursor = {};
    GetCursorPos(&cursor);
    const HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO monitor_info = {sizeof(monitor_info)};
    GetMonitorInfoW(monitor, &monitor_info);
    UINT dpi_x = 96;
    UINT dpi_y = 96;
    GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y);
    UpdateFonts(dpi_x);

    const HDC device_context = GetDC(window_);
    const SIZE title_size = MeasureText(device_context, title_font_, title_);
    const SIZE detail_size = MeasureText(device_context, detail_font_, detail_);
    ReleaseDC(window_, device_context);

    const int width = Scale(kAccentWidth + 2 * kHorizontalPadding, dpi_x) +
                      std::max(title_size.cx, detail_size.cx);
    const int height =
        Scale(2 * kVerticalPadding + kLineGap, dpi_x) + title_size.cy + detail_size.cy;
    const RECT& work_area = monitor_info.rcWork;
    const int left = work_area.left + (work_area.right - work_area.left - width) / 2;
    const int top = work_area.top + (work_area.bottom - work_area.top - height) / 2;

    SetWindowPos(window_, HWND_TOPMOST, left, top, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void Banner::Paint(HDC device_context, const RECT& client_area) const {
    const HBRUSH background = CreateSolidBrush(kBackgroundColor);
    FillRect(device_context, &client_area, background);
    DeleteObject(background);

    const RECT accent = {client_area.left, client_area.top,
                         client_area.left + Scale(kAccentWidth, font_dpi_), client_area.bottom};
    const HBRUSH accent_brush = CreateSolidBrush(kAccentColor);
    FillRect(device_context, &accent, accent_brush);
    DeleteObject(accent_brush);

    SetBkMode(device_context, TRANSPARENT);
    const int text_left = Scale(kAccentWidth + kHorizontalPadding, font_dpi_);
    RECT title_area = {text_left, Scale(kVerticalPadding, font_dpi_), client_area.right,
                       client_area.bottom};
    const HGDIOBJ previous_font = SelectObject(device_context, title_font_);
    SetTextColor(device_context, kTitleColor);
    const int title_height =
        DrawTextW(device_context, title_.c_str(), static_cast<int>(title_.size()), &title_area,
                  DT_SINGLELINE | DT_NOPREFIX);

    RECT detail_area = title_area;
    detail_area.top += title_height + Scale(kLineGap, font_dpi_);
    SelectObject(device_context, detail_font_);
    SetTextColor(device_context, kDetailColor);
    DrawTextW(device_context, detail_.c_str(), static_cast<int>(detail_.size()), &detail_area,
              DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(device_context, previous_font);
}

LRESULT CALLBACK Banner::WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        SetWindowLongPtrW(window, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    auto* banner = reinterpret_cast<Banner*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    switch (message) {
        case WM_NCHITTEST:
            return HTTRANSPARENT;
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint;
            const HDC device_context = BeginPaint(window, &paint);
            if (banner != nullptr) {
                RECT client_area;
                GetClientRect(window, &client_area);
                banner->Paint(device_context, client_area);
            }
            EndPaint(window, &paint);
            return 0;
        }
        case WM_DISPLAYCHANGE:
        case WM_DPICHANGED:
        case WM_SETTINGCHANGE:
            if (banner != nullptr && IsWindowVisible(window)) {
                banner->Layout();
                InvalidateRect(window, nullptr, TRUE);
            }
            return 0;
        default:
            return DefWindowProcW(window, message, wparam, lparam);
    }
}

}  // namespace pawse
