/**
 * @file CanvasView.cpp
 * @brief 캔버스 보기 기능: 눈금자, 격자선, 전체 화면용 합성 이미지, 에어브러시 타이머
 */
#include "pch.h"
#include "CanvasWnd.h"
#include "CanvasInternal.h"

#include <cmath>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

using namespace canvas;

namespace
{
    constexpr COLORREF kRulerBack = RGB(36, 36, 36);
    constexpr COLORREF kRulerLine = RGB(110, 110, 110);
    constexpr COLORREF kRulerText = RGB(170, 170, 170);
    constexpr COLORREF kRulerCursor = RGB(76, 194, 255);   ///< 강조색 (theme::kAccent)

    /** @brief 눈금 간격 후보 (이미지 픽셀) */
    constexpr int kRulerSteps[] = { 1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000 };
}

// ---------------------------------------------------------------------------
// 켜고 끄기
// ---------------------------------------------------------------------------

int CCanvasWnd::RulerSize() const
{
    return m_showRulers ? kRulerSize : 0;
}

bool CCanvasWnd::IsInRuler(CPoint clientPt) const
{
    return m_showRulers && (clientPt.x < kRulerSize || clientPt.y < kRulerSize);
}

void CCanvasWnd::SetShowRulers(bool show)
{
    if (m_showRulers == show)
        return;
    m_showRulers = show;
    // 눈금자 두께만큼 그림이 밀리므로 스크롤 범위와 글상자 위치를 다시 계산
    UpdateScrollBars();
    UpdateTextEditPlacement();
    Invalidate(FALSE);
}

void CCanvasWnd::SetShowGrid(bool show)
{
    m_showGrid = show;
    Invalidate(FALSE);
}

// ---------------------------------------------------------------------------
// 눈금자
// ---------------------------------------------------------------------------

void CCanvasWnd::DrawRulers(CDC& dc) const
{
    CRect client;
    GetClientRect(&client);
    const int R = kRulerSize;

    dc.FillSolidRect(0, 0, client.right, R, kRulerBack);
    dc.FillSolidRect(0, 0, R, client.bottom, kRulerBack);

    // 숫자 사이가 화면에서 50px 이상 벌어지는 가장 작은 간격을 큰 눈금으로
    int major = kRulerSteps[_countof(kRulerSteps) - 1];
    for (int s : kRulerSteps)
    {
        if (s * m_zoom >= 50)
        {
            major = s;
            break;
        }
    }
    int minor = major;
    if (major % 5 == 0 && (major / 5) * m_zoom >= 5)
        minor = major / 5;
    else if (major % 2 == 0 && (major / 2) * m_zoom >= 5)
        minor = major / 2;

    CPen pen(PS_SOLID, 1, kRulerLine);
    CPen* pOldPen = dc.SelectObject(&pen);
    CFont* pFont = CFont::FromHandle(static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT)));
    LOGFONT lf{};
    pFont->GetLogFont(&lf);
    lf.lfHeight = -11;
    CFont fontSmall, fontVertical;
    fontSmall.CreateFontIndirect(&lf);
    lf.lfEscapement = lf.lfOrientation = 900; // 세로 눈금자 숫자는 90° 돌려서
    fontVertical.CreateFontIndirect(&lf);
    CFont* pOldFont = dc.SelectObject(&fontSmall);
    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(kRulerText);

    const CPoint org = ImageOrigin();

    // 가로 눈금자
    {
        const int first = static_cast<int>(std::floor((R - org.x) / m_zoom / minor)) * minor;
        const int last = static_cast<int>(std::ceil((client.right - org.x) / m_zoom));
        for (int v = first; v <= last; v += minor)
        {
            const int x = org.x + static_cast<int>(std::floor(v * m_zoom));
            if (x < R)
                continue;
            const bool isMajor = (v % major) == 0;
            dc.MoveTo(x, R - (isMajor ? 10 : 4));
            dc.LineTo(x, R);
            if (isMajor)
            {
                CString label;
                label.Format(_T("%d"), v);
                dc.TextOut(x + 2, 1, label);
            }
        }
    }

    // 세로 눈금자
    dc.SelectObject(&fontVertical);
    {
        const int first = static_cast<int>(std::floor((R - org.y) / m_zoom / minor)) * minor;
        const int last = static_cast<int>(std::ceil((client.bottom - org.y) / m_zoom));
        for (int v = first; v <= last; v += minor)
        {
            const int y = org.y + static_cast<int>(std::floor(v * m_zoom));
            if (y < R)
                continue;
            const bool isMajor = (v % major) == 0;
            dc.MoveTo(R - (isMajor ? 10 : 4), y);
            dc.LineTo(R, y);
            if (isMajor)
            {
                CString label;
                label.Format(_T("%d"), v);
                // 90° 돌린 글자는 기준점에서 위쪽으로 써지므로, 글자 길이만큼 아래에서 시작
                dc.SelectObject(&fontSmall);
                const int textWidth = dc.GetTextExtent(label).cx;
                dc.SelectObject(&fontVertical);
                dc.TextOut(1, y + 2 + textWidth, label);
            }
        }
    }
    dc.SelectObject(pOldFont);

    // 경계선과 마우스 위치 표시
    dc.MoveTo(R, R - 1);
    dc.LineTo(client.right, R - 1);
    dc.MoveTo(R - 1, R);
    dc.LineTo(R - 1, client.bottom);
    dc.SelectObject(pOldPen);

    if (m_hoverValid)
    {
        const CPoint c = ImageToClient(m_hoverPt);
        const int half = std::max(0, static_cast<int>(m_zoom / 2));
        if (c.x + half >= R)
            dc.FillSolidRect(c.x + half, 0, 1, R, kRulerCursor);
        if (c.y + half >= R)
            dc.FillSolidRect(0, c.y + half, R, 1, kRulerCursor);
    }
    dc.FillSolidRect(0, 0, R, R, kRulerBack); // 왼쪽 위 모서리
}

// ---------------------------------------------------------------------------
// 격자선
// ---------------------------------------------------------------------------

void CCanvasWnd::DrawGrid(CDC& dc, const CRect& client) const
{
    // 400% 이상이면 픽셀마다, 그 아래는 칸이 화면에서 8px 이상이 되는 간격으로
    int cell = 500;
    if (m_zoom >= 4.0)
    {
        cell = 1;
    }
    else
    {
        for (int c : { 2, 5, 10, 20, 50, 100, 200, 500 })
        {
            if (c * m_zoom >= 8)
            {
                cell = c;
                break;
            }
        }
    }

    CRect visible;
    if (!visible.IntersectRect(ImageToClient(ImageBounds()), client))
        return;

    CPen pen(PS_SOLID, 1, cell == 1 ? RGB(200, 200, 200) : RGB(180, 180, 180));
    CPen* pOld = dc.SelectObject(&pen);
    const CPoint org = ImageOrigin();
    const CSize size = GetImageSize();

    const int x0 = std::max(0, static_cast<int>(std::floor((visible.left - org.x) / m_zoom / cell)) * cell);
    for (int v = x0; v <= size.cx; v += cell)
    {
        const int x = org.x + static_cast<int>(std::floor(v * m_zoom));
        if (x >= visible.right)
            break;
        if (x >= visible.left)
        {
            dc.MoveTo(x, visible.top);
            dc.LineTo(x, visible.bottom);
        }
    }
    const int y0 = std::max(0, static_cast<int>(std::floor((visible.top - org.y) / m_zoom / cell)) * cell);
    for (int v = y0; v <= size.cy; v += cell)
    {
        const int y = org.y + static_cast<int>(std::floor(v * m_zoom));
        if (y >= visible.bottom)
            break;
        if (y >= visible.top)
        {
            dc.MoveTo(visible.left, y);
            dc.LineTo(visible.right, y);
        }
    }
    dc.SelectObject(pOld);
}

// ---------------------------------------------------------------------------
// 전체 화면용 합성 이미지
// ---------------------------------------------------------------------------

bool CCanvasWnd::GetCompositeImage(CImage& out)
{
    if (m_image.IsNull())
        return false;
    const CSize size = GetImageSize();
    if (!CreateDib32(out, size.cx, size.cy, RGB(255, 255, 255)))
        return false;

    // 편집 중인 도형이 있으면 미리 보기 이미지, 떠 있는 선택 영역은 그 위에 합성
    CImage& base = (IsShapeActive() && !m_preview.IsNull()) ? m_preview : m_image;
    HDC hdc = out.GetDC();
    base.BitBlt(hdc, 0, 0);
    out.ReleaseDC();

    if (m_floating && !m_floatImage.IsNull())
    {
        GdiFlush();
        for (int y = 0; y < m_floatImage.GetHeight(); ++y)
        {
            const int iy = m_selRect.top + y;
            if (iy < 0 || iy >= size.cy)
                continue;
            const auto* src = static_cast<const uint32_t*>(m_floatImage.GetPixelAddress(0, y));
            auto* row = static_cast<uint32_t*>(out.GetPixelAddress(0, iy));
            for (int x = 0; x < m_floatImage.GetWidth(); ++x)
            {
                const int ix = m_selRect.left + x;
                if (ix >= 0 && ix < size.cx)
                    row[ix] = BlendOver(row[ix], src[x]);
            }
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// 에어브러시 타이머
// ---------------------------------------------------------------------------

void CCanvasWnd::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == kAirbrushTimer && m_dragMode == DragMode::Freehand && m_tool == ToolType::Brush)
    {
        GdiFlush();
        InvalidateImageRect(m_brushStroke.Spray(m_dragLast)); // 마우스가 멈춰 있어도 계속 뿌림
        return;
    }
    CWnd::OnTimer(nIDEvent);
}
