#pragma once

/**
 * @file CanvasInternal.h
 * @brief 캔버스 구현 파일들이 함께 쓰는 상수와 도우미 (CanvasWnd*.cpp 전용)
 */

namespace canvas
{
    constexpr int kMargin = 12;     ///< 작업 영역 가장자리와 이미지 사이 여백 (크기 핸들이 들어갈 자리)
    constexpr int kShadow = 3;      ///< 이미지 그림자 두께
    constexpr int kHandle = 7;      ///< 캔버스 크기 핸들 크기
    constexpr int kScrollLine = 20; ///< 스크롤 한 줄 (픽셀)
    constexpr int kMaxImageSide = 16000; ///< 캔버스 한 변의 최대 크기
    constexpr UINT kTextEditId = 100;    ///< 글상자 입력 창 ID
    constexpr UINT_PTR kAirbrushTimer = 1; ///< 에어브러시 분사 타이머 ID
    constexpr int kRulerSize = 20;       ///< 눈금자 두께

    constexpr COLORREF kWorkspaceColor = RGB(24, 24, 24);   ///< 그림 바깥 작업 영역 (theme::kWorkspace)
    constexpr COLORREF kShadowColor = RGB(10, 10, 10);      ///< 그림 그림자

    /** @brief 확대 단계 */
    constexpr double kZoomLevels[] = { 0.125, 0.25, 0.5, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0 };

    /** @brief COLORREF(0x00BBGGRR) → 32비트 DIB 픽셀(0x00RRGGBB) */
    inline uint32_t ToPixel(COLORREF c)
    {
        return (static_cast<uint32_t>(GetRValue(c)) << 16)
             | (static_cast<uint32_t>(GetGValue(c)) << 8)
             | static_cast<uint32_t>(GetBValue(c));
    }

    /** @brief 32비트 DIB 픽셀 → COLORREF */
    inline COLORREF ToColor(uint32_t p)
    {
        return RGB((p >> 16) & 0xFF, (p >> 8) & 0xFF, p & 0xFF);
    }

    /**
     * @brief 미리 곱한 알파 픽셀 src를 dst 위에 합성 (결과의 알파 바이트는 0: 일반 이미지 픽셀)
     */
    inline uint32_t BlendOver(uint32_t dst, uint32_t src)
    {
        const uint32_t a = src >> 24;
        if (a == 255)
            return src & 0x00FFFFFF;
        if (a == 0)
            return dst & 0x00FFFFFF;
        uint32_t out = 0;
        for (int shift = 0; shift < 24; shift += 8)
        {
            const uint32_t s = (src >> shift) & 0xFF;
            const uint32_t d = (dst >> shift) & 0xFF;
            const uint32_t v = s + (d * (255 - a) + 127) / 255;
            out |= std::min<uint32_t>(v, 255) << shift;
        }
        return out;
    }

    /**
     * @brief 사각형 둘레의 크기 핸들 8개 (클라이언트 좌표)
     * 순서: 0 왼쪽위, 1 위, 2 오른쪽위, 3 오른쪽, 4 오른쪽아래, 5 아래, 6 왼쪽아래, 7 왼쪽
     */
    inline std::vector<CRect> BoxHandleRects(const CRect& box)
    {
        auto at = [](int x, int y) { return CRect(x - kHandle / 2, y - kHandle / 2, x - kHandle / 2 + kHandle, y - kHandle / 2 + kHandle); };
        const int mx = (box.left + box.right) / 2;
        const int my = (box.top + box.bottom) / 2;
        return { at(box.left, box.top), at(mx, box.top), at(box.right, box.top), at(box.right, my),
                 at(box.right, box.bottom), at(mx, box.bottom), at(box.left, box.bottom), at(box.left, my) };
    }

    /** @brief 핸들 번호(BoxHandleRects 순서)에 맞는 커서 */
    inline LPCTSTR HandleCursor(int handle)
    {
        static const LPCTSTR kCursors[] = { IDC_SIZENWSE, IDC_SIZENS, IDC_SIZENESW, IDC_SIZEWE,
                                            IDC_SIZENWSE, IDC_SIZENS, IDC_SIZENESW, IDC_SIZEWE };
        return (handle >= 0 && handle < 8) ? kCursors[handle] : IDC_SIZEALL;
    }

    /** @brief 핸들로 사각형 크기 조정: 핸들 쪽 가장자리만 dx, dy만큼 옮기고 최소 1px 유지 */
    inline CRect ResizeByHandle(CRect rc, int handle, int dx, int dy)
    {
        const bool left = (handle == 0 || handle == 6 || handle == 7);
        const bool right = (handle == 2 || handle == 3 || handle == 4);
        const bool top = (handle == 0 || handle == 1 || handle == 2);
        const bool bottom = (handle == 4 || handle == 5 || handle == 6);
        if (left) rc.left += dx;
        if (right) rc.right += dx;
        if (top) rc.top += dy;
        if (bottom) rc.bottom += dy;
        if (rc.Width() < 1) { if (left) rc.left = rc.right - 1; else rc.right = rc.left + 1; }
        if (rc.Height() < 1) { if (top) rc.top = rc.bottom - 1; else rc.bottom = rc.top + 1; }
        return rc;
    }

    /** @brief 점선 사각형 (흰 바탕 위 검은 점선) */
    inline void DrawDashedRect(CDC& dc, const CRect& rc)
    {
        CPen pen(PS_DOT, 1, RGB(0, 0, 0));
        CPen* pOldPen = dc.SelectObject(&pen);
        CGdiObject* pOldBrush = dc.SelectStockObject(NULL_BRUSH);
        const int oldMode = dc.SetBkMode(OPAQUE);
        const COLORREF oldBk = dc.SetBkColor(RGB(255, 255, 255));
        dc.Rectangle(rc);
        dc.SetBkColor(oldBk);
        dc.SetBkMode(oldMode);
        dc.SelectObject(pOldBrush);
        dc.SelectObject(pOldPen);
    }

    /** @brief 점선 꺾은선 (closed면 마지막 점과 첫 점도 이음) */
    inline void DrawDashedPolyline(CDC& dc, const std::vector<CPoint>& pts, bool closed)
    {
        if (pts.size() < 2)
            return;
        CPen pen(PS_DOT, 1, RGB(0, 0, 0));
        CPen* pOldPen = dc.SelectObject(&pen);
        const int oldMode = dc.SetBkMode(OPAQUE);
        const COLORREF oldBk = dc.SetBkColor(RGB(255, 255, 255));
        dc.Polyline(pts.data(), static_cast<int>(pts.size()));
        if (closed)
        {
            dc.MoveTo(pts.back());
            dc.LineTo(pts.front());
        }
        dc.SetBkColor(oldBk);
        dc.SetBkMode(oldMode);
        dc.SelectObject(pOldPen);
    }
}
