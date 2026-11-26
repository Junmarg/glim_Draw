/**
 * @file FullScreenWnd.cpp
 * @brief 전체 화면 보기 구현
 */
#include "pch.h"
#include "FullScreenWnd.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
    constexpr COLORREF kBackground = RGB(40, 40, 40);
}

BEGIN_MESSAGE_MAP(CFullScreenWnd, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDOWN()
    ON_WM_RBUTTONDOWN()
    ON_WM_KEYDOWN()
    ON_WM_SYSKEYDOWN()
    ON_WM_KILLFOCUS()
END_MESSAGE_MAP()

void CFullScreenWnd::Show(CWnd* pOwner, CImage& image)
{
    if (image.IsNull())
        return;

    // 주 창이 있는 모니터 전체 크기
    MONITORINFO mi{ sizeof(MONITORINFO) };
    ::GetMonitorInfo(::MonitorFromWindow(pOwner->GetSafeHwnd(), MONITOR_DEFAULTTONEAREST), &mi);

    auto* wnd = new CFullScreenWnd;
    wnd->m_image.Attach(image.Detach(), CImage::DIBOR_TOPDOWN);

    const CString cls = AfxRegisterWndClass(0, ::LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr);
    if (!wnd->CreateEx(0, cls, _T("MyPaint - 전체 화면"), WS_POPUP | WS_VISIBLE, CRect(mi.rcMonitor),
                       pOwner, 0))
    {
        delete wnd; // 창이 만들어지지 않았으면 PostNcDestroy가 불리지 않음
        return;
    }
    wnd->SetForegroundWindow();
    wnd->SetFocus();
}

BOOL CFullScreenWnd::OnEraseBkgnd(CDC* /*pDC*/)
{
    return TRUE;
}

void CFullScreenWnd::OnPaint()
{
    CPaintDC dc(this);
    CRect client;
    GetClientRect(&client);

    CDC mem;
    mem.CreateCompatibleDC(&dc);
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(&dc, client.Width(), client.Height());
    CBitmap* pOld = mem.SelectObject(&bmp);
    mem.FillSolidRect(client, kBackground);

    // 화면보다 크면 비율을 유지하며 줄이고, 작으면 실제 크기로 가운데에
    const double w = m_image.GetWidth();
    const double h = m_image.GetHeight();
    const double scale = std::min({ 1.0, client.Width() / w, client.Height() / h });
    const int dw = std::max(1, static_cast<int>(w * scale));
    const int dh = std::max(1, static_cast<int>(h * scale));
    const CRect dest(CPoint((client.Width() - dw) / 2, (client.Height() - dh) / 2), CSize(dw, dh));
    mem.SetStretchBltMode(scale < 1.0 ? HALFTONE : COLORONCOLOR);
    mem.SetBrushOrg(0, 0);
    m_image.StretchBlt(mem.GetSafeHdc(), dest, CRect(0, 0, m_image.GetWidth(), m_image.GetHeight()), SRCCOPY);

    dc.BitBlt(0, 0, client.Width(), client.Height(), &mem, 0, 0, SRCCOPY);
    mem.SelectObject(pOld);
}

void CFullScreenWnd::Close()
{
    if (m_closing)
        return;
    m_closing = true;
    CWnd* owner = GetOwner();
    DestroyWindow();
    if (owner)
        owner->SetForegroundWindow();
}

void CFullScreenWnd::OnLButtonDown(UINT /*nFlags*/, CPoint /*point*/)
{
    Close();
}

void CFullScreenWnd::OnRButtonDown(UINT /*nFlags*/, CPoint /*point*/)
{
    Close();
}

void CFullScreenWnd::OnKeyDown(UINT /*nChar*/, UINT /*nRepCnt*/, UINT /*nFlags*/)
{
    Close(); // Esc, F11 등 아무 키
}

void CFullScreenWnd::OnSysKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (nChar == VK_F4)
    {
        Close(); // Alt+F4: 프로그램이 아니라 전체 화면만 닫음
        return;
    }
    CWnd::OnSysKeyDown(nChar, nRepCnt, nFlags);
}

void CFullScreenWnd::OnKillFocus(CWnd* pNewWnd)
{
    CWnd::OnKillFocus(pNewWnd);
    // Alt+Tab 등으로 다른 창으로 가면 닫음 (보이지 않는 전체 화면 창이 남지 않도록)
    PostMessage(WM_CLOSE);
}

void CFullScreenWnd::PostNcDestroy()
{
    delete this;
}
