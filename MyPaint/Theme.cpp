/**
 * @file Theme.cpp
 * @brief 어두운 테마 구현
 */
#include "pch.h"
#include "Theme.h"

#include <dwmapi.h>
#include <gdiplus.h>
#include <uxtheme.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "gdiplus.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
    Gdiplus::Color ToGdip(COLORREF c, BYTE alpha = 255)
    {
        return Gdiplus::Color(alpha, GetRValue(c), GetGValue(c), GetBValue(c));
    }

    /** @brief 둥근 사각형 경로 */
    void AddRoundRect(Gdiplus::GraphicsPath& path, const Gdiplus::RectF& r, float radius)
    {
        const float d = std::max(0.0f, radius * 2);
        if (d <= 0.0f)
        {
            path.AddRectangle(r);
            return;
        }
        path.AddArc(r.X, r.Y, d, d, 180, 90);
        path.AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
        path.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
        path.AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
        path.CloseFigure();
    }

    // Windows의 공개되지 않은 어두운 모드 함수 (uxtheme.dll 서수). 많은 프로그램이 쓰는 방법이며,
    // 없는 Windows 버전에서는 함수를 찾지 못하므로 아무 일도 하지 않습니다.
    enum class PreferredAppMode { Default, AllowDark, ForceDark, ForceLight, Max };
    using FnSetPreferredAppMode = PreferredAppMode(WINAPI*)(PreferredAppMode);
    using FnFlushMenuThemes = void(WINAPI*)();
    using FnAllowDarkModeForWindow = bool(WINAPI*)(HWND, bool);

    HMODULE UxTheme()
    {
        static HMODULE module = ::LoadLibraryEx(_T("uxtheme.dll"), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        return module;
    }
}

namespace theme
{
    int Scale(int value, HWND hwnd)
    {
        const UINT dpi = hwnd ? GetDpiForWindow(hwnd) : 96;
        return MulDiv(value, static_cast<int>(dpi ? dpi : 96), 96);
    }

    void DrawGlyph(CDC& dc, wchar_t glyph, const CRect& rc, COLORREF color, int pixelSize)
    {
        LOGFONT lf{};
        lf.lfHeight = -pixelSize;
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfQuality = CLEARTYPE_QUALITY;
        _tcscpy_s(lf.lfFaceName, _T("Segoe MDL2 Assets"));
        CFont font;
        font.CreateFontIndirect(&lf);
        CFont* pOld = dc.SelectObject(&font);
        const int oldMode = dc.SetBkMode(TRANSPARENT);
        const COLORREF oldColor = dc.SetTextColor(color);
        const wchar_t text[2] = { glyph, 0 };
        CRect r = rc;
        dc.DrawText(text, 1, r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        dc.SetTextColor(oldColor);
        dc.SetBkMode(oldMode);
        dc.SelectObject(pOld);
    }

    void DrawIcon(CDC& dc, DrawnIcon icon, const CRect& rc, COLORREF color)
    {
        Gdiplus::Graphics g(dc.GetSafeHdc());
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        const float s = static_cast<float>(std::min(rc.Width(), rc.Height()));
        const float x0 = rc.left + (rc.Width() - s) / 2.0f;
        const float y0 = rc.top + (rc.Height() - s) / 2.0f;
        auto P = [&](float u, float v) { return Gdiplus::PointF(x0 + u * s, y0 + v * s); }; // 0~1 좌표
        Gdiplus::Pen pen(ToGdip(color), std::max(1.0f, s / 16.0f));
        pen.SetLineJoin(Gdiplus::LineJoinRound);
        pen.SetStartCap(Gdiplus::LineCapRound);
        pen.SetEndCap(Gdiplus::LineCapRound);

        switch (icon)
        {
        case DrawnIcon::Bucket:
        {
            // 45° 기운 통 + 흘러내리는 방울
            Gdiplus::PointF body[] = { P(0.16f, 0.46f), P(0.46f, 0.16f), P(0.80f, 0.50f), P(0.50f, 0.80f) };
            g.DrawPolygon(&pen, body, 4);
            g.DrawLine(&pen, P(0.31f, 0.31f), P(0.18f, 0.18f)); // 손잡이
            Gdiplus::SolidBrush drop(ToGdip(color));
            Gdiplus::GraphicsPath dropPath;
            dropPath.AddBezier(P(0.86f, 0.52f), P(0.80f, 0.66f), P(0.76f, 0.74f), P(0.86f, 0.80f));
            dropPath.AddBezier(P(0.86f, 0.80f), P(0.96f, 0.74f), P(0.92f, 0.66f), P(0.86f, 0.52f));
            g.FillPath(&drop, &dropPath);
            break;
        }
        case DrawnIcon::RectSelect:
        {
            Gdiplus::Pen dash(ToGdip(color), std::max(1.0f, s / 16.0f));
            dash.SetDashStyle(Gdiplus::DashStyleDash);
            g.DrawRectangle(&dash, x0 + s * 0.14f, y0 + s * 0.2f, s * 0.72f, s * 0.6f);
            break;
        }
        case DrawnIcon::FreeSelect:
        {
            Gdiplus::Pen dash(ToGdip(color), std::max(1.0f, s / 16.0f));
            dash.SetDashStyle(Gdiplus::DashStyleDash);
            Gdiplus::PointF pts[] = { P(0.2f, 0.45f), P(0.35f, 0.15f), P(0.75f, 0.2f), P(0.85f, 0.5f), P(0.6f, 0.85f), P(0.3f, 0.75f) };
            g.DrawClosedCurve(&dash, pts, 6);
            break;
        }
        case DrawnIcon::LineWidths:
        {
            // 굵기가 다른 가로선 세 개
            Gdiplus::SolidBrush brush(ToGdip(color));
            const float widths[] = { s * 0.05f, s * 0.1f, s * 0.17f };
            float y = y0 + s * 0.2f;
            for (float w : widths)
            {
                g.FillRectangle(&brush, x0 + s * 0.12f, y, s * 0.76f, w);
                y += w + s * 0.14f;
            }
            break;
        }
        case DrawnIcon::Graph:
        {
            // 좌표축 + 3차 곡선
            Gdiplus::Pen thin(ToGdip(color), std::max(1.0f, s / 22.0f));
            g.DrawLine(&thin, P(0.10f, 0.55f), P(0.92f, 0.55f));
            g.DrawLine(&thin, P(0.45f, 0.10f), P(0.45f, 0.92f));
            Gdiplus::PointF curve[] = { P(0.12f, 0.92f), P(0.22f, 0.20f), P(0.40f, 0.30f),
                                        P(0.55f, 0.62f), P(0.75f, 0.80f), P(0.86f, 0.30f), P(0.90f, 0.08f) };
            g.DrawCurve(&pen, curve, 7, 0.4f);
            break;
        }
        }
    }

    void FillRound(CDC& dc, const CRect& rc, int radius, COLORREF color)
    {
        Gdiplus::Graphics g(dc.GetSafeHdc());
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::GraphicsPath path;
        AddRoundRect(path, Gdiplus::RectF(static_cast<float>(rc.left), static_cast<float>(rc.top),
                                          static_cast<float>(rc.Width()) - 0.5f, static_cast<float>(rc.Height()) - 0.5f),
                     static_cast<float>(radius));
        Gdiplus::SolidBrush brush(ToGdip(color));
        g.FillPath(&brush, &path);
    }

    void FrameRound(CDC& dc, const CRect& rc, int radius, COLORREF color, float width)
    {
        Gdiplus::Graphics g(dc.GetSafeHdc());
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::GraphicsPath path;
        AddRoundRect(path, Gdiplus::RectF(rc.left + 0.5f, rc.top + 0.5f, rc.Width() - 1.5f, rc.Height() - 1.5f),
                     static_cast<float>(radius));
        Gdiplus::Pen pen(ToGdip(color), width);
        g.DrawPath(&pen, &path);
    }

    void FillCircle(CDC& dc, const CRect& rc, COLORREF fill, COLORREF border)
    {
        Gdiplus::Graphics g(dc.GetSafeHdc());
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        const Gdiplus::RectF r(rc.left + 0.5f, rc.top + 0.5f, rc.Width() - 1.5f, rc.Height() - 1.5f);
        Gdiplus::SolidBrush brush(ToGdip(fill));
        g.FillEllipse(&brush, r);
        Gdiplus::Pen pen(ToGdip(border), 1.0f);
        g.DrawEllipse(&pen, r);
    }

    void CreateUiFont(CFont& font, int pixelHeight, bool bold)
    {
        LOGFONT lf{};
        lf.lfHeight = -pixelHeight;
        lf.lfWeight = bold ? FW_SEMIBOLD : FW_NORMAL;
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfQuality = CLEARTYPE_QUALITY;
        _tcscpy_s(lf.lfFaceName, _T("맑은 고딕"));
        font.CreateFontIndirect(&lf);
    }

    void InitDarkMenus()
    {
        if (HMODULE ux = UxTheme())
        {
            auto setMode = reinterpret_cast<FnSetPreferredAppMode>(::GetProcAddress(ux, MAKEINTRESOURCEA(135)));
            auto flush = reinterpret_cast<FnFlushMenuThemes>(::GetProcAddress(ux, MAKEINTRESOURCEA(136)));
            if (setMode)
                setMode(PreferredAppMode::ForceDark);
            if (flush)
                flush();
        }
    }

    void UseDarkTitleBar(HWND hwnd)
    {
        const BOOL dark = TRUE;
        // 20: DWMWA_USE_IMMERSIVE_DARK_MODE (Windows 10 2004 이상), 19: 그 이전 빌드의 같은 값
        if (FAILED(::DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark))))
            ::DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));
    }

    void UseDarkScrollbars(HWND hwnd)
    {
        if (HMODULE ux = UxTheme())
        {
            if (auto allow = reinterpret_cast<FnAllowDarkModeForWindow>(::GetProcAddress(ux, MAKEINTRESOURCEA(133))))
                allow(hwnd, true);
        }
        ::SetWindowTheme(hwnd, L"DarkMode_Explorer", nullptr);
    }

    void UseRoundCorners(HWND hwnd)
    {
        const int round = 2; // DWMWCP_ROUND
        ::DwmSetWindowAttribute(hwnd, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &round, sizeof(round));
    }
}
