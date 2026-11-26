/**
 * @file CanvasTools.cpp
 * @brief 캔버스 그리기 도구: 연필, 지우개, 채우기, 스포이트
 */
#include "pch.h"
#include "CanvasWnd.h"
#include "CanvasInternal.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

using namespace canvas;

namespace
{
    /**
     * @brief 스캔라인 방식 영역 채우기 (상하좌우로 이어진 같은 색 영역)
     *
     * 한 점씩 스택에 넣는 단순한 방식은 큰 영역에서 스택이 수백만 개로 커지므로,
     * 가로 한 줄(구간)을 한 번에 칠하고 위아래 줄에서 새 구간의 시작점만 스택에 넣습니다.
     *
     * @param base 첫 줄의 픽셀 주소
     * @param pitch 한 줄의 바이트 수
     * @return 칠했으면 true (이미 같은 색이면 false)
     */
    bool ScanlineFill(BYTE* base, int pitch, int width, int height, int x0, int y0, uint32_t fill)
    {
        auto row = [&](int y) { return reinterpret_cast<uint32_t*>(base + static_cast<ptrdiff_t>(y) * pitch); };
        const uint32_t target = row(y0)[x0] & 0x00FFFFFF;
        if (target == fill)
            return false;
        auto matches = [&](int x, int y) { return (row(y)[x] & 0x00FFFFFF) == target; };

        std::vector<POINT> stack;
        stack.push_back({ x0, y0 });
        while (!stack.empty())
        {
            const POINT p = stack.back();
            stack.pop_back();
            if (!matches(p.x, p.y))
                continue; // 다른 구간을 칠하면서 이미 칠해짐

            // 현재 줄에서 왼쪽/오른쪽 끝 찾기
            int left = p.x;
            while (left > 0 && matches(left - 1, p.y))
                --left;
            int right = p.x;
            while (right < width - 1 && matches(right + 1, p.y))
                ++right;

            uint32_t* line = row(p.y);
            for (int x = left; x <= right; ++x)
                line[x] = fill;

            // 위아래 줄에서 칠할 구간의 시작점을 스택에 넣기
            for (int ny : { p.y - 1, p.y + 1 })
            {
                if (ny < 0 || ny >= height)
                    continue;
                bool inSpan = false;
                for (int x = left; x <= right; ++x)
                {
                    const bool m = matches(x, ny);
                    if (m && !inSpan)
                        stack.push_back({ x, ny });
                    inSpan = m;
                }
            }
        }
        return true;
    }
}

// ---------------------------------------------------------------------------
// 연필 / 지우개
// ---------------------------------------------------------------------------

void CCanvasWnd::DrawStrokeSegment(CDC& dc, CPoint from, CPoint to, COLORREF color) const
{
    const int width = CurrentWidth();
    if (width <= 1)
    {
        CPen pen(PS_SOLID, 1, color);
        CPen* pOld = dc.SelectObject(&pen);
        dc.MoveTo(from);
        dc.LineTo(to);
        dc.SelectObject(pOld);
        dc.SetPixelV(to, color); // LineTo는 끝점을 그리지 않음
        return;
    }

    if (from == to)
    {
        // 길이가 0인 선은 그려지지 않으므로 원으로 점을 찍습니다.
        CBrush brush(color);
        CBrush* pOldBrush = dc.SelectObject(&brush);
        CGdiObject* pOldPen = dc.SelectStockObject(NULL_PEN);
        const int left = to.x - width / 2;
        const int top = to.y - width / 2;
        dc.Ellipse(left, top, left + width + 1, top + width + 1); // NULL_PEN이면 오른쪽/아래 1px 덜 그려짐
        dc.SelectObject(pOldPen);
        dc.SelectObject(pOldBrush);
        return;
    }

    LOGBRUSH lb{ BS_SOLID, color, 0 };
    CPen pen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND | PS_JOIN_ROUND, width, &lb);
    CPen* pOld = dc.SelectObject(&pen);
    dc.MoveTo(from);
    dc.LineTo(to);
    dc.SelectObject(pOld);
}

CRect CCanvasWnd::EraserRect(CPoint center) const
{
    const int size = GetToolWidth(ToolType::Eraser, m_sizeLevel);
    const int left = center.x - size / 2;
    const int top = center.y - size / 2;
    return CRect(left, top, left + size, top + size);
}

CRect CCanvasWnd::EraseSegment(CDC& dc, CPoint from, CPoint to)
{
    // 두 점 사이를 1픽셀 간격으로 보간하며 사각형을 찍습니다.
    const int steps = std::max(std::abs(to.x - from.x), std::abs(to.y - from.y));
    const CRect bounds = ImageBounds();
    CRect dirty;
    dirty.SetRectEmpty();

    const bool replaceMode = (m_dragButton == DragButton::Right);
    const uint32_t target = ToPixel(m_color1);
    const uint32_t replacement = ToPixel(m_color2);
    if (replaceMode)
        GdiFlush();

    for (int i = 0; i <= steps; ++i)
    {
        CPoint p = from;
        if (steps > 0)
        {
            p.x = from.x + MulDiv(to.x - from.x, i, steps);
            p.y = from.y + MulDiv(to.y - from.y, i, steps);
        }

        CRect rc = EraserRect(p);
        if (!rc.IntersectRect(rc, bounds))
            continue;

        if (!replaceMode)
        {
            dc.FillSolidRect(rc, m_color2); // 왼쪽 버튼: 색 2로 칠함
        }
        else
        {
            // 오른쪽 버튼: 색 1인 픽셀만 색 2로 바꿈
            for (int y = rc.top; y < rc.bottom; ++y)
            {
                auto* row = static_cast<uint32_t*>(m_image.GetPixelAddress(0, y));
                for (int x = rc.left; x < rc.right; ++x)
                {
                    if ((row[x] & 0x00FFFFFF) == target)
                        row[x] = replacement;
                }
            }
        }
        dirty.UnionRect(dirty, rc);
    }
    return dirty;
}

void CCanvasWnd::BeginFreehand(DragButton button, CPoint imgPt)
{
    m_beforeDrag = CaptureSnapshot();
    BeginDrag(DragMode::Freehand, button, imgPt);

    if (m_tool == ToolType::Brush)
    {
        // 질감 브러시: 이미지 픽셀에 직접 찍음 (반투명 브러시는 m_beforeDrag를 바탕으로 섞음)
        GdiFlush();
        const CSize size = GetImageSize();
        m_brushStroke.Begin(m_brushKind, DragColor(), CurrentWidth(),
                            static_cast<uint32_t*>(m_image.GetPixelAddress(0, 0)), m_image.GetPitch() / 4,
                            size.cx, size.cy, m_beforeDrag.pixels.data(), ::GetTickCount());
        InvalidateImageRect(m_brushStroke.Dab(imgPt));
        if (m_brushStroke.NeedsTimer())
            SetTimer(kAirbrushTimer, 30, nullptr); // 에어브러시: 멈춰 있어도 계속 뿌림
        return;
    }

    // 누른 자리에 바로 점을 찍습니다.
    HDC hdc = m_image.GetDC();
    CDC* pDC = CDC::FromHandle(hdc);
    CRect dirty;
    if (m_tool == ToolType::Eraser)
    {
        dirty = EraseSegment(*pDC, imgPt, imgPt);
    }
    else
    {
        DrawStrokeSegment(*pDC, imgPt, imgPt, DragColor());
        dirty = CRect(imgPt, imgPt);
        dirty.InflateRect(CurrentWidth(), CurrentWidth());
    }
    m_image.ReleaseDC();
    InvalidateImageRect(dirty);
}

void CCanvasWnd::ContinueFreehand(CPoint imgPt)
{
    if (imgPt == m_dragLast)
        return;

    if (m_tool == ToolType::Brush)
    {
        GdiFlush();
        InvalidateImageRect(m_brushStroke.Segment(m_dragLast, imgPt));
        return;
    }

    HDC hdc = m_image.GetDC();
    CDC* pDC = CDC::FromHandle(hdc);
    CRect dirty;
    if (m_tool == ToolType::Eraser)
    {
        dirty = EraseSegment(*pDC, m_dragLast, imgPt);
    }
    else
    {
        DrawStrokeSegment(*pDC, m_dragLast, imgPt, DragColor());
        dirty = CRect(m_dragLast, imgPt);
        dirty.NormalizeRect();
        dirty.InflateRect(CurrentWidth(), CurrentWidth());
    }
    m_image.ReleaseDC();
    InvalidateImageRect(dirty);
}

// ---------------------------------------------------------------------------
// Shift 제약 (도형에서 사용)
// ---------------------------------------------------------------------------

CPoint CCanvasWnd::ConstrainPoint(CPoint start, CPoint end, bool shift, bool lineMode) const
{
    if (!shift)
        return end;

    const int dx = end.x - start.x;
    const int dy = end.y - start.y;
    const int adx = std::abs(dx);
    const int ady = std::abs(dy);
    const int sx = dx < 0 ? -1 : 1;
    const int sy = dy < 0 ? -1 : 1;

    if (lineMode)
    {
        // 0°, 45°, 90° 중 가까운 방향으로 고정 (tan 22.5° ≈ 0.414)
        if (ady * 1000 < adx * 414)
            return CPoint(end.x, start.y);
        if (adx * 1000 < ady * 414)
            return CPoint(start.x, end.y);
    }

    // 정사각형/정원 또는 45° 대각선
    const int d = std::max(adx, ady);
    return CPoint(start.x + sx * d, start.y + sy * d);
}

// ---------------------------------------------------------------------------
// 채우기 / 스포이트
// ---------------------------------------------------------------------------

void CCanvasWnd::DoFill(CPoint imgPt, COLORREF color)
{
    if (!IsInImage(imgPt))
        return;

    GdiFlush();
    const CSize size = GetImageSize();
    auto* base = static_cast<BYTE*>(m_image.GetPixelAddress(0, 0));
    const uint32_t fill = ToPixel(color);
    if ((static_cast<uint32_t*>(m_image.GetPixelAddress(imgPt.x, imgPt.y))[0] & 0x00FFFFFF) == fill)
        return; // 이미 같은 색: 기록을 남기지 않음

    ImageSnapshot before = CaptureSnapshot();
    ScanlineFill(base, m_image.GetPitch(), size.cx, size.cy, imgPt.x, imgPt.y, fill);
    m_history.Push(std::move(before));
    Invalidate(FALSE);
    MarkModified();
}

void CCanvasWnd::DoPick(CPoint imgPt, DragButton button)
{
    if (!IsInImage(imgPt))
        return;

    GdiFlush();
    const uint32_t pixel = static_cast<uint32_t*>(m_image.GetPixelAddress(imgPt.x, imgPt.y))[0];
    NotifyParent(button == DragButton::Right ? CN_PICK_COLOR2 : CN_PICK_COLOR1, static_cast<LPARAM>(ToColor(pixel)));

    // Windows 그림판처럼 색을 고르면 직전 도구로 돌아갑니다.
    ChangeToolInternal(m_toolBeforePicker);
}
