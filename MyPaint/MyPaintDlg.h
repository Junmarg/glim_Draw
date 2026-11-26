#pragma once

/**
 * @file MyPaintDlg.h
 * @brief 주 대화 상자 클래스 선언
 */

#include "CanvasWnd.h"
#include "FunctionGraph.h"
#include "Printing.h"
#include "StatusStrip.h"
#include "TopBar.h"

/**
 * @brief 주 대화 상자. 위쪽 도구 막대, 캔버스, 아래쪽 상태 표시줄을 배치하고
 *        파일 입출력과 각 부분 사이의 연결을 담당합니다.
 *
 * 도구 막대·메뉴·단축키는 모두 WM_COMMAND(명령 ID)로 들어오며, 명령을 처리한 뒤에는
 * OnCommand에서 도구 막대 표시(선택된 도구, 색 등)를 한 번에 맞춥니다.
 */
class CMyPaintDlg : public CDialog
{
public:
    /**
     * @brief 생성자
     * @param pParent 부모 창 (없으면 nullptr)
     */
    explicit CMyPaintDlg(CWnd* pParent = nullptr);

    /** @brief 시작할 때 열 파일 (명령줄 인자) */
    void SetStartupFile(const CString& path) { m_startupFile = path; }

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_MYPAINT_DIALOG };
#endif

protected:
    BOOL OnInitDialog() override;
    BOOL PreTranslateMessage(MSG* pMsg) override;
    BOOL OnCommand(WPARAM wParam, LPARAM lParam) override;

    /** @brief Enter 키로 대화 상자가 닫히지 않도록 아무것도 하지 않습니다. */
    void OnOK() override;
    /** @brief Esc 키로 대화 상자가 닫히지 않도록, 진행 중인 작업만 취소합니다. */
    void OnCancel() override;

    afx_msg void OnClose();
    afx_msg void OnDestroy();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
    afx_msg void OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu);
    afx_msg void OnDropFiles(HDROP hDropInfo);

    // 파일
    afx_msg void OnFileNew();
    afx_msg void OnFileOpen();
    afx_msg void OnFileSave();
    afx_msg void OnFileSaveAs();
    afx_msg void OnFileExit();
    afx_msg void OnFilePrint();
    afx_msg void OnFilePageSetup();
    afx_msg void OnFilePrintPreview();
    afx_msg void OnSetWallpaper(UINT nID);
    afx_msg void OnOpenRecent(UINT nID);

    // 편집
    afx_msg void OnEditUndo();
    afx_msg void OnEditRedo();
    afx_msg void OnEditCut();
    afx_msg void OnEditCopy();
    afx_msg void OnEditPaste();
    afx_msg void OnEditDelete();
    afx_msg void OnEditSelectAll();
    afx_msg void OnEditInvertSelection();

    // 이미지
    afx_msg void OnImageCrop();
    afx_msg void OnImageResizeSkew();
    afx_msg void OnImageFlipRotate(UINT nID);
    afx_msg void OnImageProperties();
    afx_msg void OnImageFunctionGraph();
    afx_msg void OnImageTransparentSel();

    // 보기
    afx_msg void OnViewZoomIn();
    afx_msg void OnViewZoomOut();
    afx_msg void OnViewZoom100();
    afx_msg void OnViewStatusBar();
    afx_msg void OnViewRulers();
    afx_msg void OnViewGrid();
    afx_msg void OnViewFullScreen();
    afx_msg void OnHelpShortcuts();

    // 도구 막대
    afx_msg void OnToolCommand(UINT nID);
    afx_msg void OnBrushCommand(UINT nID);
    afx_msg void OnShapeCommand(UINT nID);
    afx_msg void OnOutlineCommand(UINT nID);
    afx_msg void OnFillCommand(UINT nID);
    afx_msg void OnSizeLevelCommand(UINT nID);
    afx_msg void OnSizeUp();
    afx_msg void OnSizeDown();
    afx_msg void OnColor1Command(UINT nID);
    afx_msg void OnColor2Command(UINT nID);
    afx_msg void OnColorEdit(UINT nID);
    afx_msg void OnTextFont();
    afx_msg void OnTextStyle(UINT nID);
    afx_msg void OnTextTransparent();

    afx_msg LRESULT OnCanvasNotify(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnStatusZoomLevel(WPARAM wParam, LPARAM lParam);

    DECLARE_MESSAGE_MAP()

private:
    void LayoutControls();
    void UpdateTitle();
    void UpdateStatusImageSize();
    void UpdateStatusZoom();
    void UpdateHint();
    /** @brief 도구 막대·상태 표시줄을 캔버스 상태에 맞춤 */
    void SyncUi();
    /** @brief 색 1/색 2를 캔버스에 반영 */
    void ApplyColors();

    /** @brief 파일 열기 (메뉴, 최근 파일, 끌어다 놓기, 명령줄 공통). 성공하면 true */
    bool OpenImageFile(const CString& path);
    void LoadRecentFiles();
    void SaveRecentFiles() const;
    void AddRecentFile(const CString& path);
    void RebuildRecentMenu(CMenu* pMenu) const;

    /**
     * @brief 저장하지 않은 변경 내용이 있으면 저장할지 묻습니다.
     * @return 계속 진행해도 되면 true (저장함/저장 안 함), 취소하면 false
     */
    bool ConfirmDiscardChanges();

    /** @brief 지정한 경로에 저장합니다. 성공하면 true */
    bool SaveTo(const CString& path);

    /** @brief 다른 이름으로 저장 대화 상자를 띄워 저장합니다. 성공하면 true */
    bool SaveAsWithDialog();

    /** @brief 제목 표시줄에 쓸 파일 이름 */
    CString DisplayName() const;

    HICON m_hIcon = nullptr;     ///< 창 아이콘
    HACCEL m_hAccel = nullptr;   ///< 단축키 테이블

    CTopBar m_topBar;            ///< 위쪽 도구 막대 (메뉴 줄 포함)
    CCanvasWnd m_canvas;         ///< 캔버스
    CStatusStrip m_status;       ///< 아래쪽 상태 표시줄
    bool m_statusVisible = true; ///< 상태 표시줄 표시 여부

    COLORREF m_color1 = RGB(0, 0, 0);       ///< 색 1 (왼쪽 버튼)
    COLORREF m_color2 = RGB(255, 255, 255); ///< 색 2 (오른쪽 버튼, 배경)
    COLORREF m_customColors[16];            ///< 색 편집 대화 상자의 사용자 지정 색

    GraphOptions m_graphOptions;        ///< 함수 그래프 설정 (다음에 열 때 그대로)
    CString m_graphExpression = _T("x^3 - 3x + 1"); ///< 마지막 함수 그래프 수식
    bool m_graphUsed = false;           ///< 함수 그래프를 연 적이 있는지 (처음엔 크기를 캔버스에 맞춤)
    CPrintSettings m_printSettings;     ///< 프린터·여백 설정 (다음 인쇄까지 기억)
    std::vector<CString> m_recentFiles; ///< 최근 파일 (최신이 앞)
    CString m_startupFile;              ///< 시작할 때 열 파일
    CString m_filePath;                 ///< 현재 파일 경로 (새 그림이면 빈 문자열)
};
