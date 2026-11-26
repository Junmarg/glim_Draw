/**
 * @file ResizeSkewDlg.cpp
 * @brief "크기 조정 및 기울이기" 대화 상자 구현
 */
#include "pch.h"
#include "MyPaint.h"
#include "ResizeSkewDlg.h"

#include <cmath>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
    constexpr int kMaxPercent = 1000;
    constexpr int kMaxPixels = 16000;
    constexpr int kMaxSkew = 89;
}

BEGIN_MESSAGE_MAP(CResizeSkewDlg, CDialog)
    ON_BN_CLICKED(IDC_RS_PERCENT, &CResizeSkewDlg::OnUnitChanged)
    ON_BN_CLICKED(IDC_RS_PIXELS, &CResizeSkewDlg::OnUnitChanged)
    ON_EN_CHANGE(IDC_RS_WIDTH, &CResizeSkewDlg::OnWidthChanged)
    ON_EN_CHANGE(IDC_RS_HEIGHT, &CResizeSkewDlg::OnHeightChanged)
END_MESSAGE_MAP()

CResizeSkewDlg::CResizeSkewDlg(CSize original, CWnd* pParent)
    : CDialog(IDD_RESIZE_SKEW, pParent), m_newSize(original), m_original(original)
{
}

BOOL CResizeSkewDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    CString info;
    info.Format(_T("현재 크기: %d × %dpx"), m_original.cx, m_original.cy);
    SetDlgItemText(IDC_RS_INFO, info);

    m_updating = true;
    CheckRadioButton(IDC_RS_PERCENT, IDC_RS_PIXELS, IDC_RS_PERCENT);
    m_percentMode = true;
    SetDlgItemInt(IDC_RS_WIDTH, 100);
    SetDlgItemInt(IDC_RS_HEIGHT, 100);
    CheckDlgButton(IDC_RS_KEEP_ASPECT, BST_CHECKED);
    SetDlgItemInt(IDC_RS_SKEW_X, 0, TRUE);
    SetDlgItemInt(IDC_RS_SKEW_Y, 0, TRUE);
    m_updating = false;

    GotoDlgCtrl(GetDlgItem(IDC_RS_WIDTH));
    return FALSE;
}

bool CResizeSkewDlg::IsPercent() const
{
    return IsDlgButtonChecked(IDC_RS_PERCENT) == BST_CHECKED;
}

void CResizeSkewDlg::OnUnitChanged()
{
    const bool toPercent = IsPercent();
    if (toPercent == m_percentMode)
        return;

    // 지금 보이는 값을 새 단위로 바꿔서 다시 표시
    BOOL okW = FALSE, okH = FALSE;
    const int w = static_cast<int>(GetDlgItemInt(IDC_RS_WIDTH, &okW));
    const int h = static_cast<int>(GetDlgItemInt(IDC_RS_HEIGHT, &okH));
    m_updating = true;
    if (toPercent)
    {
        if (okW) SetDlgItemInt(IDC_RS_WIDTH, static_cast<UINT>(std::lround(w * 100.0 / m_original.cx)));
        if (okH) SetDlgItemInt(IDC_RS_HEIGHT, static_cast<UINT>(std::lround(h * 100.0 / m_original.cy)));
    }
    else
    {
        if (okW) SetDlgItemInt(IDC_RS_WIDTH, static_cast<UINT>(std::lround(w * m_original.cx / 100.0)));
        if (okH) SetDlgItemInt(IDC_RS_HEIGHT, static_cast<UINT>(std::lround(h * m_original.cy / 100.0)));
    }
    m_updating = false;
    m_percentMode = toPercent;
}

void CResizeSkewDlg::SyncAspect(bool widthChanged)
{
    if (m_updating || IsDlgButtonChecked(IDC_RS_KEEP_ASPECT) != BST_CHECKED)
        return;

    BOOL ok = FALSE;
    const int value = static_cast<int>(GetDlgItemInt(widthChanged ? IDC_RS_WIDTH : IDC_RS_HEIGHT, &ok));
    if (!ok)
        return;

    int other = value; // 백분율이면 같은 값
    if (!m_percentMode)
    {
        other = widthChanged ? static_cast<int>(std::lround(value * static_cast<double>(m_original.cy) / m_original.cx))
                             : static_cast<int>(std::lround(value * static_cast<double>(m_original.cx) / m_original.cy));
    }
    m_updating = true;
    SetDlgItemInt(widthChanged ? IDC_RS_HEIGHT : IDC_RS_WIDTH, static_cast<UINT>(std::max(other, 1)));
    m_updating = false;
}

void CResizeSkewDlg::OnWidthChanged()
{
    SyncAspect(true);
}

void CResizeSkewDlg::OnHeightChanged()
{
    SyncAspect(false);
}

void CResizeSkewDlg::OnOK()
{
    BOOL okW = FALSE, okH = FALSE, okSx = FALSE, okSy = FALSE;
    const int w = static_cast<int>(GetDlgItemInt(IDC_RS_WIDTH, &okW));
    const int h = static_cast<int>(GetDlgItemInt(IDC_RS_HEIGHT, &okH));
    const int sx = static_cast<int>(GetDlgItemInt(IDC_RS_SKEW_X, &okSx, TRUE));
    const int sy = static_cast<int>(GetDlgItemInt(IDC_RS_SKEW_Y, &okSy, TRUE));

    const int maxSize = m_percentMode ? kMaxPercent : kMaxPixels;
    if (!okW || !okH || w < 1 || h < 1 || w > maxSize || h > maxSize)
    {
        CString msg;
        msg.Format(_T("크기는 1부터 %d까지 입력하세요."), maxSize);
        MessageBox(msg, _T("크기 조정 및 기울이기"), MB_OK | MB_ICONWARNING);
        GotoDlgCtrl(GetDlgItem(okW && w >= 1 && w <= maxSize ? IDC_RS_HEIGHT : IDC_RS_WIDTH));
        return;
    }
    if (!okSx || !okSy || std::abs(sx) > kMaxSkew || std::abs(sy) > kMaxSkew)
    {
        CString msg;
        msg.Format(_T("기울기는 -%d부터 %d까지(도) 입력하세요."), kMaxSkew, kMaxSkew);
        MessageBox(msg, _T("크기 조정 및 기울이기"), MB_OK | MB_ICONWARNING);
        GotoDlgCtrl(GetDlgItem(okSx && std::abs(sx) <= kMaxSkew ? IDC_RS_SKEW_Y : IDC_RS_SKEW_X));
        return;
    }

    if (m_percentMode)
    {
        m_newSize.cx = std::max(1, static_cast<int>(std::lround(m_original.cx * w / 100.0)));
        m_newSize.cy = std::max(1, static_cast<int>(std::lround(m_original.cy * h / 100.0)));
    }
    else
    {
        m_newSize = CSize(w, h);
    }
    m_skewX = sx;
    m_skewY = sy;
    CDialog::OnOK();
}
