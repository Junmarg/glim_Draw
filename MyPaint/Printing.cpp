/**
 * @file Printing.cpp
 * @brief 페이지 설정, 인쇄 미리 보기, 이미지 인쇄 구현
 */
#include "pch.h"
#include "MyPaint.h"
#include "Printing.h"

#include <cmath>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// ---------------------------------------------------------------------------
// 설정 / 용지 정보 / 배치 계산
// ---------------------------------------------------------------------------

CPrintSettings::~CPrintSettings()
{
    if (m_hDevMode)
        ::GlobalFree(m_hDevMode);
    if (m_hDevNames)
        ::GlobalFree(m_hDevNames);
}

namespace
{
    PageMetrics MetricsFromDC(HDC hdc)
    {
        PageMetrics m;
        m.dpi = CSize(::GetDeviceCaps(hdc, LOGPIXELSX), ::GetDeviceCaps(hdc, LOGPIXELSY));
        m.paper = CSize(::GetDeviceCaps(hdc, PHYSICALWIDTH), ::GetDeviceCaps(hdc, PHYSICALHEIGHT));
        m.offset = CPoint(::GetDeviceCaps(hdc, PHYSICALOFFSETX), ::GetDeviceCaps(hdc, PHYSICALOFFSETY));
        m.printable = CSize(::GetDeviceCaps(hdc, HORZRES), ::GetDeviceCaps(hdc, VERTRES));
        if (m.paper.cx <= 0 || m.paper.cy <= 0) // 일부 가상 프린터는 용지 크기를 주지 않음
        {
            m.paper = m.printable;
            m.offset = CPoint(0, 0);
        }
        return m;
    }

    /** @brief 설정의 프린터(없으면 기본 프린터)로 정보용 DC를 만듦 */
    HDC CreatePrinterDC(CPrintSettings& settings)
    {
        if (!settings.m_hDevNames)
        {
            CPrintDialog dlg(FALSE);
            if (dlg.GetDefaults())
            {
                settings.m_hDevMode = dlg.m_pd.hDevMode;
                settings.m_hDevNames = dlg.m_pd.hDevNames;
            }
            if (dlg.m_pd.hDC)
                ::DeleteDC(dlg.m_pd.hDC);
            dlg.m_pd.hDC = nullptr;
            dlg.m_pd.hDevMode = nullptr;
            dlg.m_pd.hDevNames = nullptr;
            if (!settings.m_hDevNames)
                return nullptr;
        }

        HDC hdc = nullptr;
        auto* names = static_cast<DEVNAMES*>(::GlobalLock(settings.m_hDevNames));
        auto* mode = settings.m_hDevMode ? static_cast<DEVMODE*>(::GlobalLock(settings.m_hDevMode)) : nullptr;
        if (names)
        {
            const auto* base = reinterpret_cast<LPCTSTR>(names);
            hdc = ::CreateDC(base + names->wDriverOffset, base + names->wDeviceOffset, nullptr, mode);
        }
        if (mode)
            ::GlobalUnlock(settings.m_hDevMode);
        if (names)
            ::GlobalUnlock(settings.m_hDevNames);
        return hdc;
    }

    /** @brief 프린터 DC에 모든 페이지를 인쇄 */
    bool RenderPages(HDC hdc, CImage& image, const CPrintSettings& settings, LPCTSTR docName, CString& error)
    {
        const PageMetrics metrics = MetricsFromDC(hdc);
        const PrintLayout layout = ComputePrintLayout(metrics, CSize(image.GetWidth(), image.GetHeight()), settings);

        DOCINFO di{ sizeof(DOCINFO) };
        di.lpszDocName = docName;
        if (::StartDoc(hdc, &di) <= 0)
        {
            error = _T("인쇄 작업을 시작하지 못했습니다.");
            return false;
        }

        // 32비트 위→아래 DIB를 그대로 넘김 (높이 음수 = 위→아래)
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = image.GetWidth();
        bmi.bmiHeader.biHeight = -image.GetHeight();
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        GdiFlush();

        for (int page = 0; page < layout.PageCount(); ++page)
        {
            if (::StartPage(hdc) <= 0)
            {
                ::AbortDoc(hdc);
                error = _T("페이지를 시작하지 못했습니다.");
                return false;
            }
            // 이 페이지에 들어가는 부분만: 여백 안쪽으로 자르고, 그림 전체를 페이지 위치에 맞춰 그림
            const int saved = ::SaveDC(hdc);
            ::IntersectClipRect(hdc, layout.area.left, layout.area.top, layout.area.right, layout.area.bottom);
            const CRect dest = layout.ImageRectOnPage(page);
            ::SetStretchBltMode(hdc, HALFTONE);
            ::SetBrushOrgEx(hdc, 0, 0, nullptr);
            const int lines = ::StretchDIBits(hdc, dest.left, dest.top, dest.Width(), dest.Height(),
                                              0, 0, image.GetWidth(), image.GetHeight(),
                                              image.GetPixelAddress(0, 0), &bmi, DIB_RGB_COLORS, SRCCOPY);
            ::RestoreDC(hdc, saved);
            if (::EndPage(hdc) <= 0 || lines == GDI_ERROR)
            {
                ::AbortDoc(hdc);
                error = _T("이미지를 프린터로 보내지 못했습니다.");
                return false;
            }
        }
        if (::EndDoc(hdc) <= 0)
        {
            error = _T("인쇄 작업을 마치지 못했습니다.");
            return false;
        }
        return true;
    }

    /** @brief 용지·여백·프린터 (Windows 공용 페이지 설정 대화 상자) */
    bool ShowPaperSetup(CWnd* pParent, CPrintSettings& settings)
    {
        CPageSetupDialog dlg(PSD_MARGINS | PSD_INHUNDREDTHSOFMILLIMETERS, pParent);
        dlg.m_psd.hDevMode = settings.m_hDevMode;
        dlg.m_psd.hDevNames = settings.m_hDevNames;
        dlg.m_psd.rtMargin = settings.m_margins;

        const INT_PTR result = dlg.DoModal();

        // 대화 상자가 핸들을 다시 할당했을 수 있으므로 돌려받고, 대화 상자가 해제하지 않도록 비움
        settings.m_hDevMode = dlg.m_psd.hDevMode;
        settings.m_hDevNames = dlg.m_psd.hDevNames;
        dlg.m_psd.hDevMode = nullptr;
        dlg.m_psd.hDevNames = nullptr;

        if (result != IDOK)
            return false;
        settings.m_margins = dlg.m_psd.rtMargin;
        return true;
    }
}

CRect PrintLayout::ImageRectOnPage(int page) const
{
    const int col = page % cols;
    const int row = page / cols;
    return CRect(CPoint(area.left + origin.x - col * area.Width(), area.top + origin.y - row * area.Height()), printed);
}

bool QueryPageMetrics(CPrintSettings& settings, PageMetrics& metrics)
{
    HDC hdc = CreatePrinterDC(settings);
    if (!hdc)
    {
        metrics = PageMetrics(); // 프린터가 없으면 A4 600 DPI로 가정
        return false;
    }
    metrics = MetricsFromDC(hdc);
    ::DeleteDC(hdc);
    return true;
}

PrintLayout ComputePrintLayout(const PageMetrics& m, CSize imageSize, const CPrintSettings& s)
{
    PrintLayout layout;

    // 여백(1/100 mm)을 장치 픽셀로 바꾸고, 인쇄 가능 영역 좌표로 옮김
    CRect area(MulDiv(s.m_margins.left, m.dpi.cx, 2540) - m.offset.x,
               MulDiv(s.m_margins.top, m.dpi.cy, 2540) - m.offset.y,
               m.paper.cx - MulDiv(s.m_margins.right, m.dpi.cx, 2540) - m.offset.x,
               m.paper.cy - MulDiv(s.m_margins.bottom, m.dpi.cy, 2540) - m.offset.y);
    area.IntersectRect(area, CRect(CPoint(0, 0), m.printable));
    if (area.IsRectEmpty())
        area = CRect(CPoint(0, 0), m.printable); // 여백이 너무 크면 인쇄 가능 영역 전체
    layout.area = area;

    // 원래 크기 = 이미지 픽셀을 96 DPI(화면)로 보고 프린터 해상도로 환산
    const double naturalW = std::max(1, static_cast<int>(imageSize.cx)) * m.dpi.cx / 96.0;
    const double naturalH = std::max(1, static_cast<int>(imageSize.cy)) * m.dpi.cy / 96.0;
    double scale = s.m_scalePercent / 100.0;
    if (s.m_fitToPages)
    {
        // 가로 n장 × 세로 m장 안에 비율을 유지하며 최대한 크게 (늘리거나 줄임)
        scale = std::min(area.Width() * std::max(1, s.m_pagesWide) / naturalW,
                         area.Height() * std::max(1, s.m_pagesTall) / naturalH);
    }
    layout.printed = CSize(std::max(1, static_cast<int>(std::lround(naturalW * scale))),
                           std::max(1, static_cast<int>(std::lround(naturalH * scale))));

    // 페이지 수: 그림이 넘치면 여러 장에 나눠서
    layout.cols = std::max(1, static_cast<int>(std::ceil(static_cast<double>(layout.printed.cx) / area.Width() - 1e-9)));
    layout.rows = std::max(1, static_cast<int>(std::ceil(static_cast<double>(layout.printed.cy) / area.Height() - 1e-9)));

    // 가운데 맞춤: 페이지들을 이어 붙인 판의 가운데에
    layout.origin = CPoint(s.m_centerHorz ? (layout.cols * area.Width() - layout.printed.cx) / 2 : 0,
                           s.m_centerVert ? (layout.rows * area.Height() - layout.printed.cy) / 2 : 0);
    return layout;
}

// ---------------------------------------------------------------------------
// 페이지 설정 대화 상자
// ---------------------------------------------------------------------------

namespace
{
    class CPageOptionsDlg : public CDialog
    {
    public:
        CPageOptionsDlg(CPrintSettings& settings, CWnd* pParent) : CDialog(IDD_PAGE_OPTIONS, pParent), m_settings(settings) {}

    protected:
        BOOL OnInitDialog() override
        {
            CDialog::OnInitDialog();
            CheckRadioButton(IDC_PS_ADJUST, IDC_PS_FIT, m_settings.m_fitToPages ? IDC_PS_FIT : IDC_PS_ADJUST);
            SetDlgItemInt(IDC_PS_PERCENT, static_cast<UINT>(m_settings.m_scalePercent));
            SetDlgItemInt(IDC_PS_PAGES_W, static_cast<UINT>(m_settings.m_pagesWide));
            SetDlgItemInt(IDC_PS_PAGES_H, static_cast<UINT>(m_settings.m_pagesTall));
            CheckDlgButton(IDC_PS_CENTER_H, m_settings.m_centerHorz ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(IDC_PS_CENTER_V, m_settings.m_centerVert ? BST_CHECKED : BST_UNCHECKED);
            UpdateEnable();
            return TRUE;
        }

        void OnOK() override
        {
            const bool fit = IsDlgButtonChecked(IDC_PS_FIT) == BST_CHECKED;
            BOOL okP = FALSE, okW = FALSE, okH = FALSE;
            const int percent = static_cast<int>(GetDlgItemInt(IDC_PS_PERCENT, &okP));
            const int wide = static_cast<int>(GetDlgItemInt(IDC_PS_PAGES_W, &okW));
            const int tall = static_cast<int>(GetDlgItemInt(IDC_PS_PAGES_H, &okH));
            if (!fit && (!okP || percent < 1 || percent > 1000))
            {
                MessageBox(_T("배율은 1부터 1000까지(%) 입력하세요."), _T("페이지 설정"), MB_OK | MB_ICONWARNING);
                GotoDlgCtrl(GetDlgItem(IDC_PS_PERCENT));
                return;
            }
            if (fit && (!okW || !okH || wide < 1 || tall < 1 || wide > 10 || tall > 10))
            {
                MessageBox(_T("페이지 수는 1부터 10까지 입력하세요."), _T("페이지 설정"), MB_OK | MB_ICONWARNING);
                GotoDlgCtrl(GetDlgItem(okW && wide >= 1 && wide <= 10 ? IDC_PS_PAGES_H : IDC_PS_PAGES_W));
                return;
            }
            m_settings.m_fitToPages = fit;
            if (!fit)
                m_settings.m_scalePercent = percent;
            else
            {
                m_settings.m_pagesWide = wide;
                m_settings.m_pagesTall = tall;
            }
            m_settings.m_centerHorz = IsDlgButtonChecked(IDC_PS_CENTER_H) == BST_CHECKED;
            m_settings.m_centerVert = IsDlgButtonChecked(IDC_PS_CENTER_V) == BST_CHECKED;
            CDialog::OnOK();
        }

        afx_msg void OnModeChanged() { UpdateEnable(); }
        afx_msg void OnPaper() { ShowPaperSetup(this, m_settings); } // 용지·여백·프린터는 바로 적용
        DECLARE_MESSAGE_MAP()

    private:
        void UpdateEnable()
        {
            const bool fit = IsDlgButtonChecked(IDC_PS_FIT) == BST_CHECKED;
            GetDlgItem(IDC_PS_PERCENT)->EnableWindow(!fit);
            GetDlgItem(IDC_PS_PAGES_W)->EnableWindow(fit);
            GetDlgItem(IDC_PS_PAGES_H)->EnableWindow(fit);
        }

        CPrintSettings& m_settings;
    };

    BEGIN_MESSAGE_MAP(CPageOptionsDlg, CDialog)
        ON_BN_CLICKED(IDC_PS_ADJUST, &CPageOptionsDlg::OnModeChanged)
        ON_BN_CLICKED(IDC_PS_FIT, &CPageOptionsDlg::OnModeChanged)
        ON_BN_CLICKED(IDC_PS_PAPER, &CPageOptionsDlg::OnPaper)
    END_MESSAGE_MAP()
}

bool ShowPageSetup(CWnd* pParent, CPrintSettings& settings)
{
    CPageOptionsDlg dlg(settings, pParent);
    return dlg.DoModal() == IDOK;
}

// ---------------------------------------------------------------------------
// 인쇄 미리 보기
// ---------------------------------------------------------------------------

namespace
{
    class CPrintPreviewDlg : public CDialog
    {
    public:
        CPrintPreviewDlg(CImage& image, CPrintSettings& settings, CWnd* pParent)
            : CDialog(IDD_PRINT_PREVIEW, pParent), m_image(image), m_settings(settings) {}

    protected:
        BOOL OnInitDialog() override
        {
            CDialog::OnInitDialog();
            // 자리표시 컨트롤의 위치에 직접 그림
            CWnd* area = GetDlgItem(IDC_PV_AREA);
            area->GetWindowRect(&m_areaRect);
            ScreenToClient(&m_areaRect);
            area->ShowWindow(SW_HIDE);
            Recompute();
            return TRUE;
        }

        afx_msg void OnPaint()
        {
            CPaintDC dc(this);
            CDC mem;
            mem.CreateCompatibleDC(&dc);
            CBitmap bmp;
            bmp.CreateCompatibleBitmap(&dc, m_areaRect.Width(), m_areaRect.Height());
            CBitmap* pOld = mem.SelectObject(&bmp);
            const CRect area(0, 0, m_areaRect.Width(), m_areaRect.Height());
            mem.FillSolidRect(area, RGB(150, 150, 150));

            // 용지를 미리 보기 영역에 맞춰 축소
            const double s = std::min((area.Width() - 24.0) / m_metrics.paper.cx, (area.Height() - 24.0) / m_metrics.paper.cy);
            const CSize paperSize(static_cast<int>(m_metrics.paper.cx * s), static_cast<int>(m_metrics.paper.cy * s));
            const CRect paper(CPoint((area.Width() - paperSize.cx) / 2, (area.Height() - paperSize.cy) / 2), paperSize);
            auto toPreview = [&](const CRect& r) {  // 인쇄 가능 영역 기준 장치 좌표 → 미리 보기 좌표
                return CRect(paper.left + static_cast<int>((m_metrics.offset.x + r.left) * s),
                             paper.top + static_cast<int>((m_metrics.offset.y + r.top) * s),
                             paper.left + static_cast<int>((m_metrics.offset.x + r.right) * s),
                             paper.top + static_cast<int>((m_metrics.offset.y + r.bottom) * s));
            };

            CRect shadow = paper;
            shadow.OffsetRect(4, 4);
            mem.FillSolidRect(shadow, RGB(90, 90, 90));
            mem.FillSolidRect(paper, RGB(255, 255, 255));

            // 이 페이지에 들어가는 그림 부분 (여백 안쪽으로 잘라서)
            const CRect margin = toPreview(m_layout.area);
            const int saved = mem.SaveDC();
            mem.IntersectClipRect(margin);
            mem.SetStretchBltMode(HALFTONE);
            mem.SetBrushOrg(0, 0);
            m_image.StretchBlt(mem.GetSafeHdc(), toPreview(m_layout.ImageRectOnPage(m_page)),
                               CRect(0, 0, m_image.GetWidth(), m_image.GetHeight()), SRCCOPY);
            mem.RestoreDC(saved);

            // 여백 표시 (점선)
            CPen pen(PS_DOT, 1, RGB(160, 160, 160));
            CPen* pOldPen = mem.SelectObject(&pen);
            CGdiObject* pOldBrush = mem.SelectStockObject(NULL_BRUSH);
            mem.Rectangle(margin);
            mem.SelectObject(pOldBrush);
            mem.SelectObject(pOldPen);

            dc.BitBlt(m_areaRect.left, m_areaRect.top, area.Width(), area.Height(), &mem, 0, 0, SRCCOPY);
            mem.SelectObject(pOld);
        }

        afx_msg void OnPrev() { if (m_page > 0) { --m_page; UpdatePageUi(); } }
        afx_msg void OnNext() { if (m_page + 1 < m_layout.PageCount()) { ++m_page; UpdatePageUi(); } }
        afx_msg void OnSetup()
        {
            if (ShowPageSetup(this, m_settings))
                Recompute();
        }
        DECLARE_MESSAGE_MAP()

    private:
        void Recompute()
        {
            const bool hasPrinter = QueryPageMetrics(m_settings, m_metrics);
            m_layout = ComputePrintLayout(m_metrics, CSize(m_image.GetWidth(), m_image.GetHeight()), m_settings);
            m_page = std::min(m_page, m_layout.PageCount() - 1);
            SetDlgItemText(IDC_PV_PRINTER, hasPrinter ? _T("") : _T("프린터가 없어 A4 용지로 가정했습니다."));
            UpdatePageUi();
        }

        void UpdatePageUi()
        {
            CString text;
            text.Format(_T("%d / %d 페이지"), m_page + 1, m_layout.PageCount());
            SetDlgItemText(IDC_PV_PAGE, text);
            GetDlgItem(IDC_PV_PREV)->EnableWindow(m_page > 0);
            GetDlgItem(IDC_PV_NEXT)->EnableWindow(m_page + 1 < m_layout.PageCount());
            InvalidateRect(m_areaRect, FALSE);
        }

        CImage& m_image;
        CPrintSettings& m_settings;
        PageMetrics m_metrics;
        PrintLayout m_layout;
        CRect m_areaRect;
        int m_page = 0;
    };

    BEGIN_MESSAGE_MAP(CPrintPreviewDlg, CDialog)
        ON_WM_PAINT()
        ON_BN_CLICKED(IDC_PV_PREV, &CPrintPreviewDlg::OnPrev)
        ON_BN_CLICKED(IDC_PV_NEXT, &CPrintPreviewDlg::OnNext)
        ON_BN_CLICKED(IDC_PV_SETUP, &CPrintPreviewDlg::OnSetup)
    END_MESSAGE_MAP()
}

bool ShowPrintPreview(CWnd* pParent, CImage& image, CPrintSettings& settings)
{
    if (image.IsNull())
        return false;
    CPrintPreviewDlg dlg(image, settings, pParent);
    return dlg.DoModal() == IDOK; // [인쇄]
}

// ---------------------------------------------------------------------------
// 인쇄
// ---------------------------------------------------------------------------

bool PrintImage(CWnd* pParent, CImage& image, LPCTSTR docName, CPrintSettings& settings, CString& error)
{
    error.Empty();
    if (image.IsNull())
        return false;

    CPrintDialog dlg(FALSE, PD_ALLPAGES | PD_USEDEVMODECOPIESANDCOLLATE | PD_NOPAGENUMS | PD_NOSELECTION
                          | PD_HIDEPRINTTOFILE | PD_RETURNDC, pParent);
    dlg.m_pd.hDevMode = settings.m_hDevMode;
    dlg.m_pd.hDevNames = settings.m_hDevNames;

    const INT_PTR result = dlg.DoModal();

    settings.m_hDevMode = dlg.m_pd.hDevMode;
    settings.m_hDevNames = dlg.m_pd.hDevNames;
    dlg.m_pd.hDevMode = nullptr;
    dlg.m_pd.hDevNames = nullptr;
    HDC hdc = dlg.m_pd.hDC;
    dlg.m_pd.hDC = nullptr;

    if (result != IDOK)
    {
        if (hdc)
            ::DeleteDC(hdc);
        return false;
    }
    if (!hdc)
    {
        error = _T("프린터를 열지 못했습니다.");
        return false;
    }

    const bool ok = RenderPages(hdc, image, settings, docName, error);
    ::DeleteDC(hdc);
    return ok;
}
