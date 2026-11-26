/**
 * @file StatusStrip.cpp
 * @brief 어두운 상태 표시줄 구현
 */
#include "pch.h"
#include "MyPaint.h"
#include "StatusStrip.h"
#include "Theme.h"

#include <cmath>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CStatusStrip, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
    ON_WM_CAPTURECHANGED()
END_MESSAGE_MAP()

BOOL CStatusStrip::Create(CWnd* pParent, UINT nID)
{
    const CString cls = AfxRegisterWndClass(0, ::LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr);
    if (!CWnd::CreateEx(0, cls, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, CRect(0, 0, 10, 10), pParent, nID))
        return FALSE;
    m_dpi = static_cast<int>(GetDpiForWindow(m_hWnd));
    theme::CreateUiFont(m_font, Px(12));
    return TRUE;
}

int CStatusStrip::PreferredHeight() const
{
    return Px(28);
}

void CStatusStrip::SetField(CString& field, const CString& text)
{
    if (field == text)
        return;
    field = text;
    Invalidate(FALSE);
}

void CStatusStrip::SetZoom(int level, int levelCount, double zoom)
{
    m_level = level;
    m_levelCount = std::max(2, levelCount);
    m_zoom = zoom;
    Invalidate(FALSE);
}

void CStatusStrip::Layout()
{
    CRect rc;
    GetClientRect(&rc);
    const int h = rc.Height();
    int x = Px(8);
    m_rcCursor = CRect(x, 0, x + Px(130), h);         x = m_rcCursor.right;
    m_rcSelection = CRect(x, 0, x + Px(120), h);      x = m_rcSelection.right;
    m_rcImage = CRect(x, 0, x + Px(120), h);          x = m_rcImage.right;

    // 오른쪽부터: [배율] [+] [슬라이더] [−]
    int r = rc.right - Px(14);
    m_rcPercent = CRect(r - Px(48), 0, r, h);         r = m_rcPercent.left - Px(4);
    m_rcZoomIn = CRect(r - Px(24), 0, r, h);          r = m_rcZoomIn.left;
    m_rcTrack = CRect(r - Px(120), 0, r, h);          r = m_rcTrack.left;
    m_rcZoomOut = CRect(r - Px(24), 0, r, h);         r = m_rcZoomOut.left - Px(8);
    m_rcHint = CRect(x + Px(8), 0, std::max<int>(x + Px(8), r), h);
}

void CStatusStrip::OnSize(UINT nType, int cx, int cy)
{
    CWnd::OnSize(nType, cx, cy);
    Layout();
    Invalidate(FALSE);
}

int CStatusStrip::ThumbX() const
{
    const int pad = Px(8);
    const int span = m_rcTrack.Width() - pad * 2;
    return m_rcTrack.left + pad + MulDiv(span, m_level, m_levelCount - 1);
}

int CStatusStrip::LevelFromX(int x) const
{
    const int pad = Px(8);
    const int span = std::max(1, m_rcTrack.Width() - pad * 2);
    const int level = static_cast<int>(std::lround(static_cast<double>(x - m_rcTrack.left - pad) * (m_levelCount - 1) / span));
    return std::clamp(level, 0, m_levelCount - 1);
}

BOOL CStatusStrip::OnEraseBkgnd(CDC* /*pDC*/)
{
    return TRUE;
}

void CStatusStrip::OnPaint()
{
    CPaintDC dc(this);
    CRect client;
    GetClientRect(&client);
    CDC mem;
    mem.CreateCompatibleDC(&dc);
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(&dc, client.Width(), client.Height());
    CBitmap* pOldBmp = mem.SelectObject(&bmp);
    mem.FillSolidRect(client, theme::kWindow);
    mem.FillSolidRect(0, 0, client.Width(), 1, theme::kBorder); // 위쪽 구분선

    CFont* pOldFont = mem.SelectObject(&m_font);
    mem.SetBkMode(TRANSPARENT);
    const int icon = Px(14);

    // 아이콘 + 글자 칸
    auto field = [&](const CRect& rc, const CString& text, auto drawIcon) {
        if (text.IsEmpty())
            return;
        CRect ri(rc.left, rc.top, rc.left + icon, rc.bottom);
        drawIcon(ri);
        CRect rt = rc;
        rt.left += icon + Px(6);
        mem.SetTextColor(theme::kTextDim);
        mem.DrawText(text, rt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    };
    field(m_rcCursor, m_cursor, [&](const CRect& r) { theme::DrawGlyph(mem, theme::kGlyphPointer, r, theme::kTextDim, Px(12)); });
    field(m_rcSelection, m_selection, [&](const CRect& r) {
        CRect ri = r; ri.DeflateRect(0, (r.Height() - icon) / 2);
        theme::DrawIcon(mem, theme::DrawnIcon::RectSelect, ri, theme::kTextDim); });
    field(m_rcImage, m_image, [&](const CRect& r) { theme::DrawGlyph(mem, theme::kGlyphPicture, r, theme::kTextDim, Px(12)); });
    if (!m_hint.IsEmpty())
    {
        mem.SetTextColor(theme::kTextDim);
        CRect rt = m_rcHint;
        mem.DrawText(m_hint, rt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }

    // 확대/축소: [−] 슬라이더 [+] 배율
    auto button = [&](const CRect& rc, wchar_t glyph, bool hot) {
        if (hot)
        {
            CRect rh = rc;
            rh.DeflateRect(0, Px(3));
            theme::FillRound(mem, rh, Px(4), theme::kHover);
        }
        theme::DrawGlyph(mem, glyph, rc, theme::kText, Px(11));
    };
    button(m_rcZoomOut, 0xE738 /* 빼기 */, m_hot == HotZoomOut);
    button(m_rcZoomIn, 0xE710 /* 더하기 */, m_hot == HotZoomIn);

    const int pad = Px(8);
    const int cy = client.Height() / 2;
    mem.FillSolidRect(m_rcTrack.left + pad, cy - 1, ThumbX() - (m_rcTrack.left + pad), 2, theme::kAccent);   // 채워진 부분
    mem.FillSolidRect(ThumbX(), cy - 1, m_rcTrack.right - pad - ThumbX(), 2, theme::kTextOff);            // 나머지
    const int r = Px(m_hot == HotSlider || m_dragging ? 7 : 6);
    theme::FillCircle(mem, CRect(ThumbX() - r, cy - r, ThumbX() + r, cy + r), theme::kAccent, theme::kWindow);

    CString percent;
    percent.Format(_T("%g%%"), m_zoom * 100.0);
    mem.SetTextColor(theme::kText);
    CRect rp = m_rcPercent;
    mem.DrawText(percent, rp, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    mem.SelectObject(pOldFont);
    dc.BitBlt(0, 0, client.Width(), client.Height(), &mem, 0, 0, SRCCOPY);
    mem.SelectObject(pOldBmp);
}

void CStatusStrip::OnLButtonDown(UINT /*nFlags*/, CPoint point)
{
    if (m_rcZoomOut.PtInRect(point))
        GetParent()->SendMessage(WM_COMMAND, ID_VIEW_ZOOM_OUT);
    else if (m_rcZoomIn.PtInRect(point))
        GetParent()->SendMessage(WM_COMMAND, ID_VIEW_ZOOM_IN);
    else if (m_rcTrack.PtInRect(point))
    {
        m_dragging = true;
        SetCapture();
        const int level = LevelFromX(point.x);
        if (level != m_level)
            GetParent()->SendMessage(WM_STATUS_ZOOM_LEVEL, static_cast<WPARAM>(level));
    }
}

void CStatusStrip::OnMouseMove(UINT /*nFlags*/, CPoint point)
{
    if (!m_tracking)
    {
        TRACKMOUSEEVENT tme{ sizeof(TRACKMOUSEEVENT), TME_LEAVE, m_hWnd, 0 };
        m_tracking = TrackMouseEvent(&tme) != FALSE;
    }
    if (m_dragging)
    {
        const int level = LevelFromX(point.x);
        if (level != m_level)
            GetParent()->SendMessage(WM_STATUS_ZOOM_LEVEL, static_cast<WPARAM>(level));
        return;
    }
    const Hot hot = m_rcZoomOut.PtInRect(point) ? HotZoomOut : m_rcZoomIn.PtInRect(point) ? HotZoomIn
                  : m_rcTrack.PtInRect(point) ? HotSlider : HotNone;
    if (hot != m_hot)
    {
        m_hot = hot;
        Invalidate(FALSE);
    }
}

void CStatusStrip::OnLButtonUp(UINT /*nFlags*/, CPoint /*point*/)
{
    if (m_dragging)
    {
        m_dragging = false;
        ReleaseCapture();
        Invalidate(FALSE);
    }
}

void CStatusStrip::OnMouseLeave()
{
    m_tracking = false;
    if (m_hot != HotNone)
    {
        m_hot = HotNone;
        Invalidate(FALSE);
    }
}

void CStatusStrip::OnCaptureChanged(CWnd* pWnd)
{
    if (pWnd != this)
        m_dragging = false;
    CWnd::OnCaptureChanged(pWnd);
}
