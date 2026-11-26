/**
 * @file PickerPopup.cpp
 * @brief 어두운 드롭다운 구현
 */
#include "pch.h"
#include "PickerPopup.h"
#include "Theme.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CPickerPopup, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
    ON_WM_LBUTTONDOWN()
    ON_WM_RBUTTONDOWN()
    ON_WM_CAPTURECHANGED()
    ON_WM_MOUSEACTIVATE()
END_MESSAGE_MAP()

int CPickerPopup::Show(CWnd* pOwner, const CRect& anchor, const Spec& spec)
{
    if (spec.count <= 0 || !spec.draw)
        return -1;

    const int padding = theme::Scale(4, pOwner->GetSafeHwnd());
    const int columns = std::max(1, spec.columns);
    const int rows = (spec.count + columns - 1) / columns;
    const CSize size(columns * spec.itemSize.cx + padding * 2, rows * spec.itemSize.cy + padding * 2);

    // 기준 사각형 아래에, 모니터 밖으로 나가면 위로/안쪽으로
    MONITORINFO mi{ sizeof(MONITORINFO) };
    ::GetMonitorInfo(::MonitorFromRect(&anchor, MONITOR_DEFAULTTONEAREST), &mi);
    const CRect work(mi.rcWork);
    CPoint pos(anchor.left, anchor.bottom + 2);
    if (pos.y + size.cy > work.bottom)
        pos.y = anchor.top - size.cy - 2;
    pos.x = std::clamp<int>(pos.x, work.left, std::max<int>(work.left, work.right - size.cx));
    pos.y = std::max<int>(pos.y, work.top);

    CPickerPopup popup(spec, padding);
    const CString cls = AfxRegisterWndClass(CS_DROPSHADOW, ::LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr);
    // 활성화하지 않는 창: 주 창의 제목 표시줄이 흐려지지 않음 (메뉴와 같은 동작)
    if (!popup.CreateEx(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE, cls, nullptr, WS_POPUP,
                        CRect(pos, size), pOwner, 0))
        return -1;
    theme::UseRoundCorners(popup.m_hWnd);
    popup.ShowWindow(SW_SHOWNOACTIVATE);
    popup.SetCapture();

    // 자체 메시지 루프: 키보드는 여기서 직접 처리, 나머지는 평소대로 전달
    MSG msg{};
    while (!popup.m_done && ::GetMessage(&msg, nullptr, 0, 0))
    {
        if (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN)
        {
            switch (msg.wParam)
            {
            case VK_ESCAPE:                     popup.Finish(-1); break;
            case VK_RETURN: case VK_SPACE:      if (popup.IsSelectable(popup.m_hot)) popup.Finish(popup.m_hot); break;
            case VK_LEFT:                       popup.MoveHot(-1, 0); break;
            case VK_RIGHT:                      popup.MoveHot(1, 0); break;
            case VK_UP:                         popup.MoveHot(0, -1); break;
            case VK_DOWN:                       popup.MoveHot(0, 1); break;
            case VK_MENU: case VK_F10:          popup.Finish(-1); break;
            default: break;
            }
            continue;
        }
        if (msg.message == WM_CHAR || msg.message == WM_SYSCHAR || msg.message == WM_KEYUP)
            continue; // 뒤의 주 창으로 키가 가지 않도록
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
        if (msg.message == WM_QUIT)
            break;
    }
    if (msg.message == WM_QUIT)
        ::PostQuitMessage(static_cast<int>(msg.wParam));

    const int result = popup.m_result;
    if (::GetCapture() == popup.m_hWnd)
        ::ReleaseCapture();
    popup.DestroyWindow();
    return result;
}

CRect CPickerPopup::ItemRect(int index) const
{
    const int columns = std::max(1, m_spec.columns);
    const int col = index % columns;
    const int row = index / columns;
    return CRect(CPoint(m_padding + col * m_spec.itemSize.cx, m_padding + row * m_spec.itemSize.cy), m_spec.itemSize);
}

int CPickerPopup::HitTest(CPoint pt) const
{
    for (int i = 0; i < m_spec.count; ++i)
        if (ItemRect(i).PtInRect(pt))
            return i;
    return -1;
}

bool CPickerPopup::IsSelectable(int index) const
{
    if (index < 0 || index >= m_spec.count)
        return false;
    return !m_spec.selectable || m_spec.selectable(index);
}

void CPickerPopup::MoveHot(int dx, int dy)
{
    const int columns = std::max(1, m_spec.columns);
    int index = m_hot < 0 ? 0 : m_hot;
    // 고를 수 없는 항목(구분선)은 건너뜀
    for (int tries = 0; tries < m_spec.count; ++tries)
    {
        index += dx + dy * columns;
        if (index < 0 || index >= m_spec.count)
            return;
        if (IsSelectable(index))
            break;
    }
    m_hot = index;
    Invalidate(FALSE);
}

void CPickerPopup::Finish(int result)
{
    m_result = result;
    m_done = true;
}

BOOL CPickerPopup::OnEraseBkgnd(CDC* /*pDC*/)
{
    return TRUE;
}

void CPickerPopup::OnPaint()
{
    CPaintDC dc(this);
    CRect client;
    GetClientRect(&client);
    CDC mem;
    mem.CreateCompatibleDC(&dc);
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(&dc, client.Width(), client.Height());
    CBitmap* pOld = mem.SelectObject(&bmp);

    mem.FillSolidRect(client, theme::kSurface);
    for (int i = 0; i < m_spec.count; ++i)
        m_spec.draw(mem, ItemRect(i), i, i == m_hot && IsSelectable(i));
    CBrush border(theme::kBorder);
    mem.FrameRect(client, &border);

    dc.BitBlt(0, 0, client.Width(), client.Height(), &mem, 0, 0, SRCCOPY);
    mem.SelectObject(pOld);
}

void CPickerPopup::OnMouseMove(UINT /*nFlags*/, CPoint point)
{
    const int hit = HitTest(point);
    if (hit != m_hot && (hit < 0 || IsSelectable(hit)))
    {
        m_hot = hit;
        Invalidate(FALSE);
    }
}

void CPickerPopup::OnLButtonUp(UINT /*nFlags*/, CPoint point)
{
    const int hit = HitTest(point);
    if (IsSelectable(hit))
        Finish(hit);
}

void CPickerPopup::OnLButtonDown(UINT /*nFlags*/, CPoint point)
{
    CloseIfOutside(point);
}

void CPickerPopup::OnRButtonDown(UINT /*nFlags*/, CPoint point)
{
    CloseIfOutside(point);
}

void CPickerPopup::CloseIfOutside(CPoint point)
{
    CRect client;
    GetClientRect(&client);
    if (!client.PtInRect(point))
        Finish(-1); // 바깥을 누르면 닫힘
}

void CPickerPopup::OnCaptureChanged(CWnd* pWnd)
{
    if (pWnd != this)
        Finish(-1); // Alt+Tab 등으로 마우스를 빼앗기면 닫힘
    CWnd::OnCaptureChanged(pWnd);
}

int CPickerPopup::OnMouseActivate(CWnd* /*pDesktopWnd*/, UINT /*nHitTest*/, UINT /*message*/)
{
    return MA_NOACTIVATE;
}
