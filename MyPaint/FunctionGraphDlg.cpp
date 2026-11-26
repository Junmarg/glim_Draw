/**
 * @file FunctionGraphDlg.cpp
 * @brief "함수 그래프" 대화 상자 구현
 */
#include "pch.h"
#include "MyPaint.h"
#include "FunctionGraphDlg.h"

#include <cmath>
#include <gdiplus.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
    constexpr UINT_PTR kPreviewTimer = 1;
    constexpr UINT kPreviewDelay = 150; // ms: 입력이 멈추면 다시 그림
    constexpr double kMaxRange = 1e9;

    const LPCTSTR kExamples[] = {
        _T("x^2 - 4"),
        _T("x^3 - 3x + 1"),
        _T("(x - 1)(x + 2)^2"),
        _T("0.5x^4 - 2x^2 + 1"),
        _T("x^5 - 5x^3 + 4x"),
        _T("-2x^2 + 8x - 3"),
        _T("x(x - 2)(x + 3)/4"),
    };

    /** @brief 투명 부분을 보여 주는 바둑판 무늬 */
    void FillChecker(CDC& dc, const CRect& rc, int cell)
    {
        dc.FillSolidRect(rc, RGB(255, 255, 255));
        for (int y = rc.top; y < rc.bottom; y += cell)
            for (int x = rc.left + (((y - rc.top) / cell) % 2) * cell; x < rc.right; x += cell * 2)
                dc.FillSolidRect(x, y, std::min<int>(cell, rc.right - x), std::min<int>(cell, rc.bottom - y), RGB(222, 222, 222));
    }

    /** @brief 목록을 "a, b, c"로, 너무 많으면 뒤를 "…"로 */
    CString JoinLimited(const std::vector<CString>& items, size_t limit)
    {
        CString s;
        for (size_t i = 0; i < items.size() && i < limit; ++i)
        {
            if (i)
                s += _T(", ");
            s += items[i];
        }
        if (items.size() > limit)
            s += _T(", …");
        return s;
    }
}

BEGIN_MESSAGE_MAP(CFunctionGraphDlg, CDialog)
    ON_CONTROL_RANGE(EN_CHANGE, IDC_FG_XMIN, IDC_FG_LINE, &CFunctionGraphDlg::OnInputChanged)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_FG_AUTOY, IDC_FG_TRANSPARENT, &CFunctionGraphDlg::OnInputChanged)
    ON_CBN_EDITCHANGE(IDC_FG_EXPR, &CFunctionGraphDlg::OnExpressionChanged)
    ON_CBN_SELCHANGE(IDC_FG_EXPR, &CFunctionGraphDlg::OnExpressionChanged)
    ON_WM_TIMER()
    ON_WM_DRAWITEM()
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

CFunctionGraphDlg::CFunctionGraphDlg(const GraphOptions& options, const CString& expression, CWnd* pParent)
    : CDialog(IDD_FUNCTION_GRAPH, pParent), m_options(options), m_expression(expression)
{
}

BOOL CFunctionGraphDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    m_updating = true;
    auto* combo = static_cast<CComboBox*>(GetDlgItem(IDC_FG_EXPR));
    for (LPCTSTR example : kExamples)
        combo->AddString(example);
    combo->SetWindowText(m_expression);

    SetNumber(IDC_FG_XMIN, m_options.xMin);
    SetNumber(IDC_FG_XMAX, m_options.xMax);
    SetNumber(IDC_FG_YMIN, m_options.yMin);
    SetNumber(IDC_FG_YMAX, m_options.yMax);
    SetDlgItemInt(IDC_FG_WIDTH, static_cast<UINT>(m_options.size.cx));
    SetDlgItemInt(IDC_FG_HEIGHT, static_cast<UINT>(m_options.size.cy));
    SetDlgItemInt(IDC_FG_LINE, static_cast<UINT>(m_options.lineWidth));
    CheckDlgButton(IDC_FG_AUTOY, m_options.autoY ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_FG_AXES, m_options.showAxes ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_FG_GRID, m_options.showGrid ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_FG_LABELS, m_options.showLabels ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_FG_POINTS, m_options.showPoints ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_FG_FORMULA, m_options.showFormula ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(IDC_FG_TRANSPARENT, m_options.transparent ? BST_CHECKED : BST_UNCHECKED);
    m_updating = false;

    UpdatePreview();
    GotoDlgCtrl(combo);
    return FALSE;
}

// ---------------------------------------------------------------------------
// 입력 읽기
// ---------------------------------------------------------------------------

bool CFunctionGraphDlg::ReadNumber(int id, double& value) const
{
    CString text;
    GetDlgItemText(id, text);
    text.Trim();
    text.Replace(static_cast<wchar_t>(0x2212), L'-'); // − → -
    if (text.IsEmpty())
        return false;
    LPTSTR end = nullptr;
    value = _tcstod(text, &end);
    return end && *end == 0 && std::isfinite(value) && std::fabs(value) <= kMaxRange;
}

void CFunctionGraphDlg::SetNumber(int id, double value)
{
    SetDlgItemText(id, FormatNumber(value, 4));
}

bool CFunctionGraphDlg::ReadInputs(Polynomial& f, GraphOptions& o, CString& error, int& badControl)
{
    CString expr;
    GetDlgItemText(IDC_FG_EXPR, expr);
    if (!Polynomial::Parse(expr, f, error))
    {
        badControl = IDC_FG_EXPR;
        return false;
    }

    o.autoY = IsDlgButtonChecked(IDC_FG_AUTOY) == BST_CHECKED;
    o.showAxes = IsDlgButtonChecked(IDC_FG_AXES) == BST_CHECKED;
    o.showGrid = IsDlgButtonChecked(IDC_FG_GRID) == BST_CHECKED;
    o.showLabels = IsDlgButtonChecked(IDC_FG_LABELS) == BST_CHECKED;
    o.showPoints = IsDlgButtonChecked(IDC_FG_POINTS) == BST_CHECKED;
    o.showFormula = IsDlgButtonChecked(IDC_FG_FORMULA) == BST_CHECKED;
    o.transparent = IsDlgButtonChecked(IDC_FG_TRANSPARENT) == BST_CHECKED;

    if (!ReadNumber(IDC_FG_XMIN, o.xMin) || !ReadNumber(IDC_FG_XMAX, o.xMax) || !(o.xMax > o.xMin))
    {
        error = _T("x 범위를 확인하세요 (최소 < 최대인 숫자)");
        badControl = ReadNumber(IDC_FG_XMIN, o.xMin) ? IDC_FG_XMAX : IDC_FG_XMIN;
        return false;
    }
    if (!o.autoY && (!ReadNumber(IDC_FG_YMIN, o.yMin) || !ReadNumber(IDC_FG_YMAX, o.yMax) || !(o.yMax > o.yMin)))
    {
        error = _T("y 범위를 확인하세요 (최소 < 최대인 숫자)");
        badControl = ReadNumber(IDC_FG_YMIN, o.yMin) ? IDC_FG_YMAX : IDC_FG_YMIN;
        return false;
    }

    BOOL okW = FALSE, okH = FALSE, okL = FALSE;
    const int w = static_cast<int>(GetDlgItemInt(IDC_FG_WIDTH, &okW, FALSE));
    const int h = static_cast<int>(GetDlgItemInt(IDC_FG_HEIGHT, &okH, FALSE));
    const int line = static_cast<int>(GetDlgItemInt(IDC_FG_LINE, &okL, FALSE));
    if (!okW || !okH || w < 50 || h < 50 || w > graph::kMaxSide || h > graph::kMaxSide)
    {
        error.Format(_T("그림 크기는 50 ~ %d 픽셀로 입력하세요"), graph::kMaxSide);
        badControl = (okW && w >= 50 && w <= graph::kMaxSide) ? IDC_FG_HEIGHT : IDC_FG_WIDTH;
        return false;
    }
    if (!okL || line < 1 || line > 30)
    {
        error = _T("선 굵기는 1 ~ 30 픽셀로 입력하세요");
        badControl = IDC_FG_LINE;
        return false;
    }
    o.size = CSize(w, h);
    o.lineWidth = line;
    return true;
}

// ---------------------------------------------------------------------------
// 미리 보기
// ---------------------------------------------------------------------------

void CFunctionGraphDlg::OnInputChanged(UINT nID)
{
    if (m_updating)
        return;
    if (nID == IDC_FG_AUTOY)
    {
        const BOOL manual = IsDlgButtonChecked(IDC_FG_AUTOY) != BST_CHECKED;
        GetDlgItem(IDC_FG_YMIN)->EnableWindow(manual);
        GetDlgItem(IDC_FG_YMAX)->EnableWindow(manual);
    }
    ScheduleUpdate();
}

void CFunctionGraphDlg::OnExpressionChanged()
{
    if (!m_updating)
        ScheduleUpdate(); // 목록에서 고르면 편집 칸 글자는 알림 뒤에 바뀌므로 타이머에서 읽음
}

void CFunctionGraphDlg::ScheduleUpdate()
{
    SetTimer(kPreviewTimer, kPreviewDelay, nullptr);
}

void CFunctionGraphDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == kPreviewTimer)
    {
        KillTimer(kPreviewTimer);
        UpdatePreview();
        return;
    }
    CDialog::OnTimer(nIDEvent);
}

void CFunctionGraphDlg::UpdatePreview()
{
    const BOOL manual = IsDlgButtonChecked(IDC_FG_AUTOY) != BST_CHECKED;
    GetDlgItem(IDC_FG_YMIN)->EnableWindow(manual);
    GetDlgItem(IDC_FG_YMAX)->EnableWindow(manual);

    Polynomial f;
    GraphOptions o = m_options;
    CString error;
    int bad = 0;
    if (!ReadInputs(f, o, error, bad))
    {
        m_hasError = true;
        SetDlgItemText(IDC_FG_INFO, _T("⚠ ") + error);
        SetDlgItemText(IDC_FG_DETAILS, _T(""));
        m_preview.Destroy();
        GetDlgItem(IDC_FG_PREVIEW)->Invalidate(FALSE);
        return;
    }

    m_hasError = false;
    CString info;
    if (f.IsConstant())
        info.Format(_T("상수 함수: f(x) = %s"), (LPCTSTR)f.ToString());
    else
        info.Format(_T("%d차 함수: f(x) = %s"), f.Degree(), (LPCTSTR)f.ToString());
    SetDlgItemText(IDC_FG_INFO, info);

    if (graph::Render(f, o, m_preview))
    {
        m_previewTransparent = o.transparent;
        if (o.autoY)
        {
            // 자동으로 정한 y 범위를 칸에 보여 줌 (끄면 이 값에서 시작해 고칠 수 있음)
            m_updating = true;
            SetNumber(IDC_FG_YMIN, o.yMin);
            SetNumber(IDC_FG_YMAX, o.yMax);
            m_updating = false;
        }
    }
    UpdateDetails(f, o);
    GetDlgItem(IDC_FG_PREVIEW)->Invalidate(FALSE);
}

void CFunctionGraphDlg::UpdateDetails(const Polynomial& f, const GraphOptions& o)
{
    CString text;
    if (f.IsConstant())
    {
        text.Format(_T("y절편: (0, %s)"), (LPCTSTR)FormatNumber(f.Eval(0.0), 4, true));
        SetDlgItemText(IDC_FG_DETAILS, text);
        return;
    }

    std::vector<CString> roots, maxima, minima;
    for (double r : f.RealRoots(o.xMin, o.xMax))
        roots.push_back(_T("x = ") + FormatNumber(r, 4, true));
    for (const auto& e : f.Extrema(o.xMin, o.xMax))
    {
        const CString p = _T("(") + FormatNumber(e.x, 4, true) + _T(", ") + FormatNumber(e.y, 4, true) + _T(")");
        (e.maximum ? maxima : minima).push_back(p);
    }
    const CString none = _T("없음");
    CString range;
    range.Format(_T("x 범위 %s ~ %s 안에서"), (LPCTSTR)FormatNumber(o.xMin, 4, true), (LPCTSTR)FormatNumber(o.xMax, 4, true));
    text = range + _T("\r\n");
    text += _T("실근(x절편): ") + (roots.empty() ? none : JoinLimited(roots, 6)) + _T("\r\n");
    text += _T("극대: ") + (maxima.empty() ? none : JoinLimited(maxima, 4)) + _T("\r\n");
    text += _T("극소: ") + (minima.empty() ? none : JoinLimited(minima, 4)) + _T("\r\n");
    text += _T("y절편: (0, ") + FormatNumber(f.Eval(0.0), 4, true) + _T(")");
    SetDlgItemText(IDC_FG_DETAILS, text);
}

void CFunctionGraphDlg::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
    if (nIDCtl != IDC_FG_PREVIEW)
    {
        CDialog::OnDrawItem(nIDCtl, lpDrawItemStruct);
        return;
    }

    const CRect rc(lpDrawItemStruct->rcItem);
    CDC* target = CDC::FromHandle(lpDrawItemStruct->hDC);
    CDC dc;
    dc.CreateCompatibleDC(target);
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(target, rc.Width(), rc.Height());
    CBitmap* oldBmp = dc.SelectObject(&bmp);

    const CRect area(0, 0, rc.Width(), rc.Height());
    dc.FillSolidRect(area, RGB(96, 96, 96));

    if (!m_preview.IsNull())
    {
        // 가로세로 비율을 지켜 가운데에 맞춤
        const double scale = std::min({ (area.Width() - 8.0) / m_preview.GetWidth(),
                                        (area.Height() - 8.0) / m_preview.GetHeight(), 1.0 });
        const int w = std::max(1, static_cast<int>(m_preview.GetWidth() * scale));
        const int h = std::max(1, static_cast<int>(m_preview.GetHeight() * scale));
        const CRect fit(CPoint((area.Width() - w) / 2, (area.Height() - h) / 2), CSize(w, h));
        if (m_previewTransparent)
            FillChecker(dc, fit, 6);

        Gdiplus::Bitmap image(m_preview.GetWidth(), m_preview.GetHeight(), m_preview.GetPitch(),
                              PixelFormat32bppPARGB, static_cast<BYTE*>(m_preview.GetBits()));
        Gdiplus::Graphics g(dc.GetSafeHdc());
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        g.DrawImage(&image, Gdiplus::Rect(fit.left, fit.top, fit.Width(), fit.Height()));
    }
    else
    {
        dc.SetBkMode(TRANSPARENT);
        dc.SetTextColor(RGB(230, 230, 230));
        CFont* oldFont = dc.SelectObject(GetFont());
        CRect textRc = area;
        dc.DrawText(_T("수식을 고치면 여기에 그래프가 나타납니다"), textRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        dc.SelectObject(oldFont);
    }

    target->BitBlt(rc.left, rc.top, rc.Width(), rc.Height(), &dc, 0, 0, SRCCOPY);
    dc.SelectObject(oldBmp);
}

HBRUSH CFunctionGraphDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    HBRUSH brush = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
    if (pWnd->GetDlgCtrlID() == IDC_FG_INFO && m_hasError)
        pDC->SetTextColor(RGB(200, 0, 0));
    return brush;
}

void CFunctionGraphDlg::OnOK()
{
    KillTimer(kPreviewTimer);
    Polynomial f;
    GraphOptions o = m_options;
    CString error;
    int bad = 0;
    if (!ReadInputs(f, o, error, bad))
    {
        MessageBox(error, _T("함수 그래프"), MB_OK | MB_ICONWARNING);
        GotoDlgCtrl(GetDlgItem(bad));
        return;
    }
    if (!graph::Render(f, o, m_result))
    {
        MessageBox(_T("그래프를 그리지 못했습니다."), _T("함수 그래프"), MB_OK | MB_ICONERROR);
        return;
    }
    m_options = o;
    GetDlgItemText(IDC_FG_EXPR, m_expression);
    CDialog::OnOK();
}
