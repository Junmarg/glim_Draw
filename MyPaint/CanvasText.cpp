/**
 * @file CanvasText.cpp
 * @brief 캔버스 텍스트 도구
 *
 * 글상자는 캔버스 위에 띄운 여러 줄 편집 컨트롤(CTextEdit)입니다.
 * 입력하는 동안에는 이미지에 아무것도 그리지 않고, 확정할 때 같은 글꼴로
 * DrawText를 이미지에 한 번 그립니다. 편집 컨트롤의 글꼴은 확대 배율만큼 키워서
 * 화면에서 확정 결과와 비슷한 크기로 보이게 합니다.
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
    constexpr UINT kTextFormat = DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX | DT_EXPANDTABS;
}

// ---------------------------------------------------------------------------
// CTextEdit
// ---------------------------------------------------------------------------

BEGIN_MESSAGE_MAP(CTextEdit, CEdit)
    ON_WM_GETDLGCODE()
    ON_WM_KEYDOWN()
    ON_WM_CHAR()
END_MESSAGE_MAP()

UINT CTextEdit::OnGetDlgCode()
{
    // Enter, Tab, Esc를 대화 상자가 가로채지 않고 이 컨트롤로 보내도록
    return CEdit::OnGetDlgCode() | DLGC_WANTALLKEYS;
}

void CTextEdit::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    auto* canvas = static_cast<CCanvasWnd*>(GetParent());
    if (nChar == VK_ESCAPE)
    {
        if (canvas)
            canvas->CancelOperation(); // 글상자 취소
        return;
    }
    // Ctrl+B / Ctrl+I / Ctrl+U: 굵게 / 기울임 / 밑줄 (그림판과 같음)
    if (GetKeyState(VK_CONTROL) < 0 && (nChar == 'B' || nChar == 'I' || nChar == 'U'))
    {
        if (canvas)
            canvas->ToggleTextStyle(static_cast<char>(nChar));
        return;
    }
    CEdit::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CTextEdit::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (nChar == VK_ESCAPE)
        return; // 편집 컨트롤이 Esc 문자에 경고음을 내지 않도록
    if (GetKeyState(VK_CONTROL) < 0 && (nChar == 2 || nChar == 9 || nChar == 21))
        return; // Ctrl+B/I/U가 만드는 문자(Ctrl+I는 탭 문자)가 입력되지 않도록
    if (nChar == 1) // Ctrl+A: 여러 줄 편집 컨트롤은 기본으로 지원하지 않음
    {
        SetSel(0, -1);
        return;
    }
    CEdit::OnChar(nChar, nRepCnt, nFlags);
}

// ---------------------------------------------------------------------------
// 글꼴
// ---------------------------------------------------------------------------

int CCanvasWnd::TextPixelHeight() const
{
    return std::max(1, MulDiv(m_textPointSize, 96, 72)); // 96 DPI 기준 (그림판과 같음)
}

void CCanvasWnd::MakeTextFont(CFont& font, double scale) const
{
    LOGFONT lf = m_textLogFont;
    lf.lfHeight = -std::max(1, static_cast<int>(std::lround(TextPixelHeight() * scale)));
    lf.lfWidth = 0;
    lf.lfQuality = ANTIALIASED_QUALITY; // ClearType은 그림에 색 번짐을 남기므로 회색조 안티앨리어싱
    font.CreateFontIndirect(&lf);
}

void CCanvasWnd::SetTextFont(const LOGFONT& lf, int pointSize)
{
    m_textLogFont = lf;
    m_textPointSize = std::clamp(pointSize, 1, 500);
    if (m_textActive)
    {
        UpdateTextEditPlacement();
        OnTextChanged(); // 글자가 커져 넘치면 글상자를 늘림
    }
}

void CCanvasWnd::GetTextFont(LOGFONT& lf, int& pointSize) const
{
    lf = m_textLogFont;
    pointSize = m_textPointSize;
}

void CCanvasWnd::ToggleTextStyle(char style)
{
    switch (style)
    {
    case 'B': m_textLogFont.lfWeight = (m_textLogFont.lfWeight >= FW_BOLD) ? FW_NORMAL : FW_BOLD; break;
    case 'I': m_textLogFont.lfItalic = !m_textLogFont.lfItalic; break;
    case 'U': m_textLogFont.lfUnderline = !m_textLogFont.lfUnderline; break;
    default: return;
    }
    if (m_textActive)
    {
        UpdateTextEditPlacement();
        OnTextChanged();
    }
}

void CCanvasWnd::FocusTextEdit()
{
    if (m_textActive)
        m_textEdit.SetFocus();
}

// ---------------------------------------------------------------------------
// 글상자
// ---------------------------------------------------------------------------

void CCanvasWnd::StartText(CRect rect)
{
    rect.IntersectRect(rect, ImageBounds()); // 글상자는 이미지 안에만
    if (rect.IsRectEmpty())
        return;

    m_textRect = rect;
    m_textMinHeight = rect.Height();
    m_textActive = true;

    if (!m_textEdit.GetSafeHwnd())
    {
        m_textEdit.Create(WS_CHILD | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | ES_NOHIDESEL,
                          CRect(0, 0, 1, 1), this, kTextEditId);
    }
    m_textEdit.SetWindowText(_T(""));
    UpdateTextEditPlacement();
    m_textEdit.ShowWindow(SW_SHOW);
    m_textEdit.SetFocus();
    Invalidate(FALSE);
    NotifySelectionSize();
    NotifyParent(CN_STATE);
}

void CCanvasWnd::UpdateTextEditPlacement()
{
    if (!m_textActive || !m_textEdit.GetSafeHwnd())
        return;

    // 글꼴을 먼저 바꾸고 옛 글꼴을 지움 (편집 컨트롤이 지워진 글꼴을 잡고 있지 않도록)
    CFont font;
    MakeTextFont(font, m_zoom);
    m_textEdit.SetFont(&font, FALSE);
    m_textEditFont.DeleteObject();
    m_textEditFont.Attach(font.Detach());

    m_textEdit.SetMargins(0, 0);
    m_textEdit.MoveWindow(ImageToClient(m_textRect));
    m_textEdit.Invalidate();
}

void CCanvasWnd::OnTextChanged()
{
    if (!m_textActive)
        return;

    // 입력한 글이 글상자보다 길어지면 아래로 늘립니다. (확정할 때 잘리지 않도록)
    CString text;
    m_textEdit.GetWindowText(text);
    if (text.IsEmpty() || text.Right(1) == _T("\n"))
        text += _T(" "); // 마지막 빈 줄도 높이에 포함

    CFont font;
    MakeTextFont(font, 1.0);
    CClientDC screen(this);
    CDC dc;
    dc.CreateCompatibleDC(&screen);
    CFont* pOld = dc.SelectObject(&font);
    CRect calc(0, 0, m_textRect.Width(), 0);
    dc.DrawText(text, calc, kTextFormat | DT_CALCRECT);
    dc.SelectObject(pOld);

    int height = std::max(m_textMinHeight, static_cast<int>(calc.Height()) + 2);
    height = std::min<int>(height, GetImageSize().cy - m_textRect.top);
    if (height != m_textRect.Height())
    {
        m_textRect.bottom = m_textRect.top + height;
        UpdateTextEditPlacement();
        Invalidate(FALSE);
        NotifySelectionSize();
    }
}

HBRUSH CCanvasWnd::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    if (pWnd && pWnd->GetSafeHwnd() == m_textEdit.GetSafeHwnd())
    {
        // 입력 중에는 색 1 글자, 색 2 바탕으로 보여 줍니다.
        pDC->SetTextColor(m_color1);
        pDC->SetBkColor(m_color2);
        return static_cast<HBRUSH>(m_textBrush.GetSafeHandle());
    }
    return CWnd::OnCtlColor(pDC, pWnd, nCtlColor);
}

void CCanvasWnd::HideTextEdit()
{
    m_textActive = false;
    if (m_textEdit.GetSafeHwnd())
    {
        const bool hadFocus = (GetFocus() == &m_textEdit);
        m_textEdit.ShowWindow(SW_HIDE);
        if (hadFocus)
            SetFocus();
    }
    Invalidate(FALSE);
    NotifySelectionSize();
    NotifyParent(CN_STATE);
}

void CCanvasWnd::CommitText()
{
    if (!m_textActive)
        return;

    CString text;
    m_textEdit.GetWindowText(text);
    HideTextEdit();
    if (text.IsEmpty())
        return;

    ImageSnapshot before = CaptureSnapshot();

    HDC hdc = m_image.GetDC();
    CDC* pDC = CDC::FromHandle(hdc);
    // CImage::GetDC는 여러 CImage가 함께 쓰는 DC를 빌려주고, ReleaseDC할 때 DC 상태를 되돌리지 않습니다.
    // 바꾼 상태(원점, 채우기 모드 등)가 다른 이미지 그리기에 남지 않도록 SaveDC/RestoreDC로 감쌉니다.
    const int saved = pDC->SaveDC();
    if (!m_textTransparent)
        pDC->FillSolidRect(m_textRect, m_color2);

    CFont font;
    MakeTextFont(font, 1.0);
    CFont* pOld = pDC->SelectObject(&font);
    pDC->SetBkMode(TRANSPARENT);
    pDC->SetTextColor(m_color1);
    CRect rc = m_textRect;
    pDC->DrawText(text, rc, kTextFormat);
    pDC->SelectObject(pOld);
    pDC->RestoreDC(saved);
    m_image.ReleaseDC();

    m_history.Push(std::move(before));
    Invalidate(FALSE);
    MarkModified();
}

void CCanvasWnd::CancelText()
{
    if (m_textActive)
        HideTextEdit();
}
