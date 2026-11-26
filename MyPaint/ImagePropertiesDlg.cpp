/**
 * @file ImagePropertiesDlg.cpp
 * @brief "이미지 속성" 대화 상자 구현
 */
#include "pch.h"
#include "MyPaint.h"
#include "ImagePropertiesDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
    constexpr int kMaxSide = 16000;
}

CImagePropertiesDlg::CImagePropertiesDlg(CSize size, const CString& fileInfo, CWnd* pParent)
    : CDialog(IDD_IMAGE_PROPERTIES, pParent), m_size(size), m_fileInfo(fileInfo)
{
}

BOOL CImagePropertiesDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    SetDlgItemText(IDC_IP_INFO, m_fileInfo);
    SetDlgItemInt(IDC_IP_WIDTH, static_cast<UINT>(m_size.cx));
    SetDlgItemInt(IDC_IP_HEIGHT, static_cast<UINT>(m_size.cy));
    GotoDlgCtrl(GetDlgItem(IDC_IP_WIDTH));
    return FALSE;
}

void CImagePropertiesDlg::OnOK()
{
    BOOL okW = FALSE, okH = FALSE;
    const int w = static_cast<int>(GetDlgItemInt(IDC_IP_WIDTH, &okW));
    const int h = static_cast<int>(GetDlgItemInt(IDC_IP_HEIGHT, &okH));
    if (!okW || !okH || w < 1 || h < 1 || w > kMaxSide || h > kMaxSide)
    {
        CString msg;
        msg.Format(_T("너비와 높이는 1부터 %d까지(픽셀) 입력하세요."), kMaxSide);
        MessageBox(msg, _T("이미지 속성"), MB_OK | MB_ICONWARNING);
        GotoDlgCtrl(GetDlgItem(okW && w >= 1 && w <= kMaxSide ? IDC_IP_HEIGHT : IDC_IP_WIDTH));
        return;
    }
    m_size = CSize(w, h);
    CDialog::OnOK();
}
