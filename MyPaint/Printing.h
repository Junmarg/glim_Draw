#pragma once

/**
 * @file Printing.h
 * @brief 페이지 설정, 인쇄 미리 보기, 이미지 인쇄
 *
 * 인쇄와 미리 보기는 같은 배치 계산(ComputePrintLayout)을 쓰므로, 미리 보기에서 본 그대로 인쇄됩니다.
 */

/**
 * @brief 인쇄 설정. 프린터와 용지·방향(DEVMODE), 여백, 크기 조정, 가운데 맞춤을 다음 인쇄까지 기억합니다.
 */
class CPrintSettings
{
public:
    CPrintSettings() = default;
    ~CPrintSettings();
    CPrintSettings(const CPrintSettings&) = delete;
    CPrintSettings& operator=(const CPrintSettings&) = delete;

    HGLOBAL m_hDevMode = nullptr;   ///< 프린터 설정 (공용 대화 상자가 할당, 이 개체가 해제)
    HGLOBAL m_hDevNames = nullptr;  ///< 프린터 이름
    CRect m_margins{ 1000, 1000, 1000, 1000 }; ///< 여백 (1/100 mm, 기본 10mm)

    bool m_fitToPages = false;      ///< false: 배율로 조정, true: 가로 n × 세로 m 페이지에 맞춤
    int m_scalePercent = 100;       ///< 배율 (원래 크기 = 화면 크기 기준 %)
    int m_pagesWide = 1;            ///< 맞춤: 가로 페이지 수
    int m_pagesTall = 1;            ///< 맞춤: 세로 페이지 수
    bool m_centerHorz = false;      ///< 가로 가운데 맞춤
    bool m_centerVert = false;      ///< 세로 가운데 맞춤
};

/**
 * @brief 용지와 인쇄 가능 영역 (프린터 장치 픽셀)
 */
struct PageMetrics
{
    CSize dpi{ 600, 600 };
    CSize paper{ 4961, 7016 };      ///< 용지 전체 (기본: A4 600 DPI)
    CPoint offset{ 0, 0 };          ///< 용지 가장자리 → 인쇄 가능 영역 시작
    CSize printable{ 4961, 7016 };  ///< 인쇄 가능 영역
};

/**
 * @brief 그림을 페이지에 어떻게 놓을지 (인쇄 가능 영역 기준 장치 픽셀)
 */
struct PrintLayout
{
    CRect area;           ///< 여백 안쪽 (그림이 들어갈 곳)
    CSize printed;        ///< 인쇄될 그림 크기
    int cols = 1;         ///< 가로 페이지 수
    int rows = 1;         ///< 세로 페이지 수
    CPoint origin;        ///< 전체 페이지를 이어 붙인 판에서 그림의 왼쪽 위

    int PageCount() const { return cols * rows; }
    /** @brief page번째 페이지에서 그림 전체가 놓이는 사각형 (페이지 밖으로 넘치는 부분은 잘라서 인쇄) */
    CRect ImageRectOnPage(int page) const;
};

/**
 * @brief 선택된(없으면 기본) 프린터의 용지 정보를 읽습니다.
 * @return 프린터 정보를 읽었으면 true, 프린터가 없어서 A4 600 DPI로 가정했으면 false
 */
bool QueryPageMetrics(CPrintSettings& settings, PageMetrics& metrics);

/** @brief 배치 계산: 여백, 배율 또는 n×m 맞춤, 가운데 맞춤, 페이지 수 */
PrintLayout ComputePrintLayout(const PageMetrics& metrics, CSize imageSize, const CPrintSettings& settings);

/**
 * @brief 페이지 설정 대화 상자 (크기 조정, 가운데 맞춤, [용지·여백·프린터] 버튼)
 * @return 확인을 눌렀으면 true
 */
bool ShowPageSetup(CWnd* pParent, CPrintSettings& settings);

/**
 * @brief 인쇄 미리 보기
 * @return 미리 보기에서 [인쇄]를 눌렀으면 true
 */
bool ShowPrintPreview(CWnd* pParent, CImage& image, CPrintSettings& settings);

/**
 * @brief 인쇄 대화 상자를 띄우고 이미지를 인쇄합니다 (페이지 설정의 배치대로, 필요하면 여러 장).
 * @param error [out] 실패했을 때의 설명 (취소하면 비어 있음)
 * @return 인쇄했으면 true
 */
bool PrintImage(CWnd* pParent, CImage& image, LPCTSTR docName, CPrintSettings& settings, CString& error);
