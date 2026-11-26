/**
 * @file MyPaintDlg.cpp
 * @brief 주 대화 상자 클래스 구현
 */
#include "pch.h"
#include "MyPaint.h"
#include "MyPaintDlg.h"
#include "ResizeSkewDlg.h"
#include "FullScreenWnd.h"
#include "ImagePropertiesDlg.h"
#include "FunctionGraphDlg.h"

#include <shlwapi.h>
#include <shobjidl.h>
#pragma comment(lib, "shlwapi.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
    constexpr int kDefaultWidth = 800;   ///< 새 그림 너비
    constexpr int kDefaultHeight = 600;  ///< 새 그림 높이
    constexpr int kMaxRecentFiles = ID_FILE_RECENT_LAST - ID_FILE_RECENT_FIRST + 1; ///< 최근 파일 수 (8)
    const TCHAR kRecentSection[] = _T("Recent Files");

    /** @brief 열기 대화 상자 필터 */
    const TCHAR kOpenFilter[] =
        _T("그림 파일 (*.png;*.bmp;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico)|*.png;*.bmp;*.jpg;*.jpeg;*.gif;*.tif;*.tiff;*.ico|")
        _T("모든 파일 (*.*)|*.*||");

    /** @brief 저장 대화 상자 필터 (순서는 kSaveExtensions와 같아야 함) */
    const TCHAR kSaveFilter[] =
        _T("PNG (*.png)|*.png|")
        _T("24비트 비트맵 (*.bmp)|*.bmp|")
        _T("JPEG (*.jpg;*.jpeg)|*.jpg;*.jpeg|")
        _T("GIF (*.gif)|*.gif|")
        _T("TIFF (*.tif;*.tiff)|*.tif;*.tiff||");

    /** @brief 저장 필터 순서별 확장자 */
    const LPCTSTR kSaveExtensions[] = { _T("png"), _T("bmp"), _T("jpg"), _T("gif"), _T("tif") };

    /** @brief 파일 경로의 확장자에 맞는 저장 필터 번호 (1부터) */
    DWORD FilterIndexForPath(const CString& path)
    {
        CString ext = path.Mid(path.ReverseFind(_T('.')) + 1);
        ext.MakeLower();
        if (ext == _T("jpeg")) ext = _T("jpg");
        if (ext == _T("tiff")) ext = _T("tif");
        for (DWORD i = 0; i < _countof(kSaveExtensions); ++i)
        {
            if (ext == kSaveExtensions[i])
                return i + 1;
        }
        return 1; // 기본은 PNG
    }

    // 자동화(테스트)와 접근성 도구가 현재 상태를 읽을 수 있도록 창 속성으로 노출하는 이름
    const TCHAR kPropTool[] = _T("MyPaint.Tool");
    const TCHAR kPropSizeLevel[] = _T("MyPaint.SizeLevel");
    const TCHAR kPropZoomLevel[] = _T("MyPaint.ZoomLevel");
}

BEGIN_MESSAGE_MAP(CMyPaintDlg, CDialog)
    ON_WM_CLOSE()
    ON_WM_DESTROY()
    ON_WM_SIZE()
    ON_WM_ERASEBKGND()
    ON_WM_GETMINMAXINFO()
    ON_WM_INITMENUPOPUP()
    ON_WM_DROPFILES()
    // 파일
    ON_COMMAND(ID_FILE_NEW_IMAGE, &CMyPaintDlg::OnFileNew)
    ON_COMMAND(ID_FILE_OPEN_IMAGE, &CMyPaintDlg::OnFileOpen)
    ON_COMMAND(ID_FILE_SAVE_IMAGE, &CMyPaintDlg::OnFileSave)
    ON_COMMAND(ID_FILE_SAVE_IMAGE_AS, &CMyPaintDlg::OnFileSaveAs)
    ON_COMMAND(ID_FILE_EXIT, &CMyPaintDlg::OnFileExit)
    ON_COMMAND(ID_FILE_PRINT_IMG, &CMyPaintDlg::OnFilePrint)
    ON_COMMAND(ID_FILE_PAGE_SETUP_IMG, &CMyPaintDlg::OnFilePageSetup)
    ON_COMMAND(ID_FILE_PRINT_PREVIEW_IMG, &CMyPaintDlg::OnFilePrintPreview)
    ON_COMMAND_RANGE(ID_FILE_WALLPAPER_FILL, ID_FILE_WALLPAPER_CENTER, &CMyPaintDlg::OnSetWallpaper)
    ON_COMMAND_RANGE(ID_FILE_RECENT_FIRST, ID_FILE_RECENT_LAST, &CMyPaintDlg::OnOpenRecent)
    // 편집
    ON_COMMAND(ID_EDIT_UNDO_ACTION, &CMyPaintDlg::OnEditUndo)
    ON_COMMAND(ID_EDIT_REDO_ACTION, &CMyPaintDlg::OnEditRedo)
    ON_COMMAND(ID_EDIT_CUT_SEL, &CMyPaintDlg::OnEditCut)
    ON_COMMAND(ID_EDIT_COPY_SEL, &CMyPaintDlg::OnEditCopy)
    ON_COMMAND(ID_EDIT_PASTE_IMG, &CMyPaintDlg::OnEditPaste)
    ON_COMMAND(ID_EDIT_DELETE_SEL, &CMyPaintDlg::OnEditDelete)
    ON_COMMAND(ID_EDIT_SELECT_ALL_IMG, &CMyPaintDlg::OnEditSelectAll)
    ON_COMMAND(ID_EDIT_INVERT_SEL, &CMyPaintDlg::OnEditInvertSelection)
    // 이미지
    ON_COMMAND(ID_IMAGE_CROP, &CMyPaintDlg::OnImageCrop)
    ON_COMMAND(ID_IMAGE_RESIZE_SKEW, &CMyPaintDlg::OnImageResizeSkew)
    ON_COMMAND_RANGE(ID_IMAGE_ROTATE_RIGHT, ID_IMAGE_FLIP_HORIZONTAL, &CMyPaintDlg::OnImageFlipRotate)
    ON_COMMAND(ID_IMAGE_PROPERTIES, &CMyPaintDlg::OnImageProperties)
    ON_COMMAND(ID_IMAGE_FUNCTION_GRAPH, &CMyPaintDlg::OnImageFunctionGraph)
    ON_COMMAND(ID_IMAGE_TRANSPARENT_SEL, &CMyPaintDlg::OnImageTransparentSel)
    // 보기
    ON_COMMAND(ID_VIEW_ZOOM_IN, &CMyPaintDlg::OnViewZoomIn)
    ON_COMMAND(ID_VIEW_ZOOM_OUT, &CMyPaintDlg::OnViewZoomOut)
    ON_COMMAND(ID_VIEW_ZOOM_100, &CMyPaintDlg::OnViewZoom100)
    ON_COMMAND(ID_VIEW_STATUSBAR, &CMyPaintDlg::OnViewStatusBar)
    ON_COMMAND(ID_VIEW_RULERS, &CMyPaintDlg::OnViewRulers)
    ON_COMMAND(ID_VIEW_GRID, &CMyPaintDlg::OnViewGrid)
    ON_COMMAND(ID_VIEW_FULLSCREEN, &CMyPaintDlg::OnViewFullScreen)
    ON_COMMAND(ID_HELP_SHORTCUTS, &CMyPaintDlg::OnHelpShortcuts)
    // 도구 막대
    ON_COMMAND_RANGE(IDC_TOOL_PENCIL, IDC_TOOL_MAGNIFIER, &CMyPaintDlg::OnToolCommand)
    ON_COMMAND_RANGE(ID_BRUSH_FIRST, ID_BRUSH_LAST, &CMyPaintDlg::OnBrushCommand)
    ON_COMMAND_RANGE(ID_SHAPE_FIRST, ID_SHAPE_LAST, &CMyPaintDlg::OnShapeCommand)
    ON_COMMAND_RANGE(ID_OUTLINE_FIRST, ID_OUTLINE_LAST, &CMyPaintDlg::OnOutlineCommand)
    ON_COMMAND_RANGE(ID_FILL_FIRST, ID_FILL_LAST, &CMyPaintDlg::OnFillCommand)
    ON_COMMAND_RANGE(ID_SIZE_LEVEL_FIRST, ID_SIZE_LEVEL_LAST, &CMyPaintDlg::OnSizeLevelCommand)
    ON_COMMAND(ID_SIZE_UP, &CMyPaintDlg::OnSizeUp)
    ON_COMMAND(ID_SIZE_DOWN, &CMyPaintDlg::OnSizeDown)
    ON_COMMAND_RANGE(ID_COLOR1_FIRST, ID_COLOR1_LAST, &CMyPaintDlg::OnColor1Command)
    ON_COMMAND_RANGE(ID_COLOR2_FIRST, ID_COLOR2_LAST, &CMyPaintDlg::OnColor2Command)
    ON_COMMAND_RANGE(ID_COLOR_EDIT1, ID_COLOR_EDIT2, &CMyPaintDlg::OnColorEdit)
    ON_COMMAND(ID_TEXT_FONT_CMD, &CMyPaintDlg::OnTextFont)
    ON_COMMAND_RANGE(ID_TEXT_BOLD_CMD, ID_TEXT_UNDERLINE_CMD, &CMyPaintDlg::OnTextStyle)
    ON_COMMAND(ID_TEXT_TRANSPARENT_CMD, &CMyPaintDlg::OnTextTransparent)
    ON_MESSAGE(WM_CANVAS_NOTIFY, &CMyPaintDlg::OnCanvasNotify)
    ON_MESSAGE(WM_STATUS_ZOOM_LEVEL, &CMyPaintDlg::OnStatusZoomLevel)
END_MESSAGE_MAP()

CMyPaintDlg::CMyPaintDlg(CWnd* pParent) : CDialog(IDD_MYPAINT_DIALOG, pParent)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
    for (COLORREF& c : m_customColors)
        c = RGB(255, 255, 255);
}

BOOL CMyPaintDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    SetIcon(m_hIcon, TRUE);  // 큰 아이콘
    SetIcon(m_hIcon, FALSE); // 작은 아이콘
    theme::UseDarkTitleBar(m_hWnd);

    m_hAccel = ::LoadAccelerators(AfxGetResourceHandle(), MAKEINTRESOURCE(IDR_ACCEL));

    m_topBar.Create(this, IDC_TOPBAR);
    m_status.Create(this, IDC_STATUSBAR);
    m_canvas.Create(this, IDC_CANVAS);

    m_canvas.SetShapeKind(ShapeKind::Line);
    m_canvas.SetShapeOptions(ShapePaint::Solid, ShapePaint::None);
    m_canvas.SetBrushKind(BrushKind::Normal);
    m_canvas.SetTool(ToolType::Pencil);
    ApplyColors();

    m_canvas.NewImage(kDefaultWidth, kDefaultHeight);

    // 처음 창 크기: 도구 막대가 다 보이는 너비 이상
    CRect rcWindow;
    GetWindowRect(&rcWindow);
    const int minWidth = m_topBar.MinimumWidth() + theme::Scale(40, m_hWnd);
    if (rcWindow.Width() < minWidth)
        SetWindowPos(nullptr, 0, 0, minWidth, rcWindow.Height(), SWP_NOMOVE | SWP_NOZORDER);

    LayoutControls();
    UpdateTitle();
    UpdateStatusImageSize();
    UpdateStatusZoom();
    SyncUi();

    DragAcceptFiles(TRUE); // 탐색기에서 그림 파일을 끌어다 놓으면 열기
    LoadRecentFiles();
    if (!m_startupFile.IsEmpty())
        OpenImageFile(m_startupFile);

    m_canvas.SetFocus();
    return FALSE; // 포커스를 직접 지정했음
}

BOOL CMyPaintDlg::PreTranslateMessage(MSG* pMsg)
{
    const bool forThisWindow = (pMsg->hwnd == m_hWnd || ::IsChild(m_hWnd, pMsg->hwnd));

    // 메뉴 막대를 직접 그리므로 Alt+F/E/I/V/H, F10으로 메뉴 열기도 직접 처리
    if (forThisWindow && pMsg->message == WM_SYSKEYDOWN)
    {
        static const TCHAR kMenuKeys[] = _T("FEIVH");
        const TCHAR* key = _tcschr(kMenuKeys, static_cast<TCHAR>(pMsg->wParam));
        if (key && pMsg->wParam >= 'A' && pMsg->wParam <= 'Z')
        {
            m_topBar.OpenMenu(static_cast<int>(key - kMenuKeys));
            return TRUE;
        }
        if (pMsg->wParam == VK_F10)
        {
            m_topBar.OpenMenu(0);
            return TRUE;
        }
    }

    // 대화 상자는 단축키 테이블을 자동으로 처리하지 않으므로 직접 변환합니다.
    // 단, 글상자에 입력 중일 때는 Ctrl+C/V/Z, Del 등을 편집 컨트롤이 쓰도록 넘기고,
    // 전체 화면 창처럼 이 대화 상자 밖의 창으로 가는 키는 건드리지 않습니다.
    if (m_hAccel && pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST
        && forThisWindow && !m_canvas.IsTextEditWindow(pMsg->hwnd)
        && ::TranslateAccelerator(m_hWnd, m_hAccel, pMsg))
    {
        return TRUE;
    }
    return CDialog::PreTranslateMessage(pMsg);
}

BOOL CMyPaintDlg::OnCommand(WPARAM wParam, LPARAM lParam)
{
    // 어떤 명령이든 (도구 막대, 메뉴, 단축키) 처리한 뒤 도구 막대 표시를 맞춤
    const BOOL handled = CDialog::OnCommand(wParam, lParam);
    if (m_topBar.GetSafeHwnd())
        SyncUi();
    return handled;
}

void CMyPaintDlg::OnOK()
{
    // Enter 키로 닫히지 않게 함
}

void CMyPaintDlg::OnCancel()
{
    // Esc 키로 닫히지 않게 함. 창 닫기는 OnClose에서 처리합니다.
    m_canvas.CancelOperation();
}

void CMyPaintDlg::OnClose()
{
    if (ConfirmDiscardChanges())
        EndDialog(IDCANCEL);
}

void CMyPaintDlg::OnDestroy()
{
    ::RemoveProp(m_hWnd, kPropTool);
    ::RemoveProp(m_hWnd, kPropSizeLevel);
    ::RemoveProp(m_hWnd, kPropZoomLevel);
    CDialog::OnDestroy();
}

// ---------------------------------------------------------------------------
// 배치
// ---------------------------------------------------------------------------

void CMyPaintDlg::LayoutControls()
{
    if (!m_canvas.GetSafeHwnd() || !m_topBar.GetSafeHwnd() || !m_status.GetSafeHwnd())
        return;

    CRect client;
    GetClientRect(&client);
    const int topH = m_topBar.PreferredHeight();
    const int statusH = m_statusVisible ? m_status.PreferredHeight() : 0;

    m_topBar.MoveWindow(0, 0, client.Width(), topH);
    m_status.ShowWindow(m_statusVisible ? SW_SHOW : SW_HIDE);
    if (m_statusVisible)
        m_status.MoveWindow(0, client.bottom - statusH, client.Width(), statusH);
    m_canvas.MoveWindow(0, topH, client.Width(), std::max<int>(0, client.Height() - topH - statusH));
}

void CMyPaintDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialog::OnSize(nType, cx, cy);
    if (nType != SIZE_MINIMIZED)
        LayoutControls();
}

BOOL CMyPaintDlg::OnEraseBkgnd(CDC* pDC)
{
    // 자식 창(도구 막대, 캔버스, 상태 표시줄)이 전부 덮으므로 어두운 색으로만 채움
    CRect rc;
    GetClientRect(&rc);
    pDC->FillSolidRect(rc, theme::kWindow);
    return TRUE;
}

void CMyPaintDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    CDialog::OnGetMinMaxInfo(lpMMI);
    if (!GetSafeHwnd() || !m_topBar.GetSafeHwnd())
        return;

    // 도구 막대의 모든 그룹이 보이는 너비, 그림을 볼 수 있는 최소 높이
    CRect rc(0, 0, m_topBar.MinimumWidth() + theme::Scale(12, m_hWnd),
             m_topBar.PreferredHeight() + m_status.PreferredHeight() + theme::Scale(200, m_hWnd));
    AdjustWindowRectEx(&rc, GetStyle(), FALSE, GetExStyle());
    lpMMI->ptMinTrackSize.x = rc.Width();
    lpMMI->ptMinTrackSize.y = rc.Height();
}

void CMyPaintDlg::OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu)
{
    CDialog::OnInitMenuPopup(pPopupMenu, nIndex, bSysMenu);
    if (bSysMenu)
        return;

    // 대화 상자에는 ON_UPDATE_COMMAND_UI가 동작하지 않으므로 직접 켜고 끕니다.
    auto enable = [&](UINT id, bool on) {
        pPopupMenu->EnableMenuItem(id, MF_BYCOMMAND | (on ? MF_ENABLED : MF_GRAYED));
    };
    const bool hasSel = m_canvas.HasSelection();
    enable(ID_EDIT_UNDO_ACTION, m_canvas.CanUndo());
    enable(ID_EDIT_REDO_ACTION, m_canvas.CanRedo());
    enable(ID_EDIT_CUT_SEL, hasSel);
    enable(ID_EDIT_COPY_SEL, hasSel);
    enable(ID_EDIT_DELETE_SEL, hasSel);
    enable(ID_EDIT_PASTE_IMG, CCanvasWnd::CanPaste());
    enable(ID_IMAGE_CROP, hasSel);
    enable(ID_EDIT_INVERT_SEL, hasSel);
    RebuildRecentMenu(pPopupMenu); // "최근 파일" 하위 메뉴일 때만 다시 채움
    auto check = [&](UINT id, bool on) {
        pPopupMenu->CheckMenuItem(id, MF_BYCOMMAND | (on ? MF_CHECKED : MF_UNCHECKED));
    };
    check(ID_VIEW_STATUSBAR, m_statusVisible);
    check(ID_VIEW_RULERS, m_canvas.GetShowRulers());
    check(ID_VIEW_GRID, m_canvas.GetShowGrid());
    check(ID_IMAGE_TRANSPARENT_SEL, m_canvas.IsTransparentSelection());
}

// ---------------------------------------------------------------------------
// 상태 표시
// ---------------------------------------------------------------------------

CString CMyPaintDlg::DisplayName() const
{
    if (m_filePath.IsEmpty())
        return _T("제목 없음");
    return m_filePath.Mid(m_filePath.ReverseFind(_T('\\')) + 1);
}

void CMyPaintDlg::UpdateTitle()
{
    CString title;
    title.Format(_T("%s%s - MyPaint"), m_canvas.IsModified() ? _T("*") : _T(""), DisplayName().GetString());
    SetWindowText(title);
}

void CMyPaintDlg::UpdateStatusImageSize()
{
    const CSize size = m_canvas.GetImageSize();
    CString text;
    text.Format(_T("%d × %dpx"), size.cx, size.cy);
    m_status.SetImageText(text);
}

void CMyPaintDlg::UpdateStatusZoom()
{
    const int level = CCanvasWnd::NearestZoomLevel(m_canvas.GetZoom());
    m_status.SetZoom(level, CCanvasWnd::ZoomLevelCount(), m_canvas.GetZoom());
    ::SetProp(m_hWnd, kPropZoomLevel, reinterpret_cast<HANDLE>(static_cast<INT_PTR>(level)));
}

void CMyPaintDlg::UpdateHint()
{
    CString hint;
    switch (m_canvas.GetTool())
    {
    case ToolType::Pencil:
        hint = _T("왼쪽 버튼: 색 1 · 오른쪽 버튼: 색 2 · Esc: 취소");
        break;
    case ToolType::Eraser:
        hint = _T("왼쪽: 색 2로 지우기 · 오른쪽: 색 1인 부분만 색 2로 바꾸기");
        break;
    case ToolType::Fill:
        hint = _T("왼쪽: 색 1로 채우기 · 오른쪽: 색 2로 채우기");
        break;
    case ToolType::Picker:
        hint = _T("왼쪽: 색 1로 지정 · 오른쪽: 색 2로 지정 (지정 후 직전 도구로 돌아감)");
        break;
    case ToolType::Text:
        hint = _T("드래그/클릭: 글상자 · 바깥 클릭: 확정 · Esc: 취소 · Ctrl+B/I/U: 굵게/기울임/밑줄");
        break;
    case ToolType::Select:
    case ToolType::FreeSelect:
        hint = _T("드래그: 선택 · 안쪽 드래그: 이동 (Ctrl: 복사) · 핸들: 크기 조정 · 방향키: 1px 이동 · Del: 삭제 · Esc: 확정");
        break;
    case ToolType::Magnifier:
        hint = _T("왼쪽 클릭: 확대 · 오른쪽 클릭: 축소 (점선은 확대될 영역)");
        break;
    case ToolType::Shape:
        switch (m_canvas.GetShapeKind())
        {
        case ShapeKind::Curve:
            hint = _T("선을 그은 뒤 두 번 클릭/드래그해서 휘기 · Enter: 휘기 생략 · 그린 뒤 핸들로 조정");
            break;
        case ShapeKind::Polygon:
            hint = _T("클릭/드래그로 꼭짓점 추가 · 시작점 클릭/더블클릭/Enter: 닫기 · Esc: 취소");
            break;
        default:
            hint = _T("드래그: 그리기 (Shift: 정비율) · 그린 뒤 핸들로 크기/위치 조정 · 바깥 클릭/Esc/Enter: 확정");
            break;
        }
        break;
    case ToolType::Brush:
        hint.Format(_T("%s · 왼쪽 버튼: 색 1 · 오른쪽 버튼: 색 2 · Ctrl+Plus/Minus: 굵기 · Esc: 취소"),
                    BrushName(m_canvas.GetBrushKind()));
        break;
    default:
        hint = _T("왼쪽 버튼: 색 1 · 오른쪽 버튼: 색 2 · Esc: 취소");
        break;
    }
    m_status.SetHint(hint);
}

void CMyPaintDlg::SyncUi()
{
    TopBarState state;
    state.tool = m_canvas.GetTool();
    state.brush = m_canvas.GetBrushKind();
    state.shape = m_canvas.GetShapeKind();
    state.outline = m_canvas.GetShapeOutline();
    state.fill = m_canvas.GetShapeFill();
    state.sizeLevel = m_canvas.GetSizeLevel();
    state.color1 = m_color1;
    state.color2 = m_color2;
    state.transparentSelection = m_canvas.IsTransparentSelection();
    state.hasSelection = m_canvas.HasSelection();
    state.canUndo = m_canvas.CanUndo();
    state.canRedo = m_canvas.CanRedo();

    LOGFONT lf;
    int pointSize = 0;
    m_canvas.GetTextFont(lf, pointSize);
    state.textBold = lf.lfWeight >= FW_BOLD;
    state.textItalic = lf.lfItalic != 0;
    state.textUnderline = lf.lfUnderline != 0;
    state.textTransparent = m_canvas.IsTextTransparent();
    state.fontLabel.Format(_T("%s, %dpt"), lf.lfFaceName, pointSize);
    m_topBar.SetState(state);

    UpdateHint();
    ::SetProp(m_hWnd, kPropTool, reinterpret_cast<HANDLE>(static_cast<INT_PTR>(state.tool)));
    ::SetProp(m_hWnd, kPropSizeLevel, reinterpret_cast<HANDLE>(static_cast<INT_PTR>(state.sizeLevel)));
}

void CMyPaintDlg::ApplyColors()
{
    m_canvas.SetColors(m_color1, m_color2);
    m_canvas.FocusTextEdit(); // 글상자 입력 중이었다면 계속 입력할 수 있게
}

LRESULT CMyPaintDlg::OnCanvasNotify(WPARAM wParam, LPARAM lParam)
{
    switch (wParam)
    {
    case CN_CURSOR:
    case CN_SELSIZE:
    {
        CString text;
        if (lParam != -1)
            text.Format(wParam == CN_CURSOR ? _T("%d, %dpx") : _T("%d × %dpx"), LOWORD(lParam), HIWORD(lParam));
        if (wParam == CN_CURSOR)
            m_status.SetCursorText(text);
        else
            m_status.SetSelectionText(text);
        break;
    }
    case CN_STATE:
        UpdateTitle();
        UpdateStatusImageSize();
        if (m_topBar.GetSafeHwnd())
            SyncUi(); // 실행 취소 가능 여부, 선택 영역 유무
        break;
    case CN_ZOOM:
        UpdateStatusZoom();
        break;
    case CN_TOOL:
        SyncUi();
        break;
    case CN_PICK_COLOR1:
        m_color1 = static_cast<COLORREF>(lParam);
        ApplyColors();
        SyncUi();
        break;
    case CN_PICK_COLOR2:
        m_color2 = static_cast<COLORREF>(lParam);
        ApplyColors();
        SyncUi();
        break;
    }
    return 0;
}

LRESULT CMyPaintDlg::OnStatusZoomLevel(WPARAM wParam, LPARAM /*lParam*/)
{
    m_canvas.SetZoom(CCanvasWnd::ZoomLevelAt(static_cast<int>(wParam)));
    return 0;
}

// ---------------------------------------------------------------------------
// 도구 막대
// ---------------------------------------------------------------------------

void CMyPaintDlg::OnToolCommand(UINT nID)
{
    static_assert(IDC_TOOL_MAGNIFIER - IDC_TOOL_PENCIL == static_cast<int>(ToolType::Magnifier),
                  "도구 명령 ID 순서와 ToolType 순서가 같아야 합니다.");
    m_canvas.SetTool(static_cast<ToolType>(nID - IDC_TOOL_PENCIL));
    m_canvas.SetFocus();
}

void CMyPaintDlg::OnBrushCommand(UINT nID)
{
    // 브러시를 고르면 브러시 도구로 바뀜
    m_canvas.SetBrushKind(static_cast<BrushKind>(nID - ID_BRUSH_FIRST));
    if (m_canvas.GetTool() != ToolType::Brush)
        m_canvas.SetTool(ToolType::Brush);
    m_canvas.SetFocus();
}

void CMyPaintDlg::OnShapeCommand(UINT nID)
{
    // 도형을 고르면 도형 도구로 바뀜 (편집 중이던 도형은 확정)
    m_canvas.SetShapeKind(static_cast<ShapeKind>(nID - ID_SHAPE_FIRST));
    if (m_canvas.GetTool() != ToolType::Shape)
        m_canvas.SetTool(ToolType::Shape);
    m_canvas.SetFocus();
}

void CMyPaintDlg::OnOutlineCommand(UINT nID)
{
    m_canvas.SetShapeOptions(static_cast<ShapePaint>(nID - ID_OUTLINE_FIRST), m_canvas.GetShapeFill());
}

void CMyPaintDlg::OnFillCommand(UINT nID)
{
    m_canvas.SetShapeOptions(m_canvas.GetShapeOutline(), static_cast<ShapePaint>(nID - ID_FILL_FIRST));
}

void CMyPaintDlg::OnSizeLevelCommand(UINT nID)
{
    m_canvas.SetSizeLevel(static_cast<int>(nID - ID_SIZE_LEVEL_FIRST));
}

void CMyPaintDlg::OnSizeUp()
{
    // Ctrl+Plus: 굵기 한 단계 올림 (굵기를 쓰는 도구만)
    if (CCanvasWnd::ToolUsesSize(m_canvas.GetTool()))
        m_canvas.SetSizeLevel(m_canvas.GetSizeLevel() + 1);
}

void CMyPaintDlg::OnSizeDown()
{
    if (CCanvasWnd::ToolUsesSize(m_canvas.GetTool()))
        m_canvas.SetSizeLevel(m_canvas.GetSizeLevel() - 1);
}

void CMyPaintDlg::OnColor1Command(UINT nID)
{
    m_color1 = m_topBar.GetPaletteColor(static_cast<int>(nID - ID_COLOR1_FIRST));
    ApplyColors();
}

void CMyPaintDlg::OnColor2Command(UINT nID)
{
    m_color2 = m_topBar.GetPaletteColor(static_cast<int>(nID - ID_COLOR2_FIRST));
    ApplyColors();
}

void CMyPaintDlg::OnColorEdit(UINT nID)
{
    COLORREF& color = (nID == ID_COLOR_EDIT2) ? m_color2 : m_color1;
    CColorDialog dlg(color, CC_FULLOPEN | CC_RGBINIT, this);
    dlg.m_cc.lpCustColors = m_customColors;
    if (dlg.DoModal() == IDOK)
    {
        color = dlg.GetColor();
        m_topBar.AddCustomColor(color); // 그림판처럼 팔레트의 사용자 지정 칸에도 넣음
        ApplyColors();
    }
    m_canvas.FocusTextEdit();
}

void CMyPaintDlg::OnTextFont()
{
    LOGFONT lf;
    int pointSize = 0;
    m_canvas.GetTextFont(lf, pointSize);

    // 글꼴 대화 상자는 화면 DPI 기준 높이를 받으므로 포인트 크기를 변환해서 넘깁니다.
    CClientDC dc(this);
    lf.lfHeight = -MulDiv(pointSize, dc.GetDeviceCaps(LOGPIXELSY), 72);

    CFontDialog dlg(&lf, CF_SCREENFONTS | CF_EFFECTS | CF_INITTOLOGFONTSTRUCT, nullptr, this);
    dlg.m_cf.rgbColors = m_color1;
    if (dlg.DoModal() == IDOK)
    {
        LOGFONT chosen;
        dlg.GetCurrentFont(&chosen);
        m_canvas.SetTextFont(chosen, std::max(1, dlg.GetSize() / 10));
        m_color1 = dlg.GetColor(); // 글꼴 대화 상자에서 고른 색은 색 1로
        ApplyColors();
    }
    m_canvas.FocusTextEdit();
}

void CMyPaintDlg::OnTextStyle(UINT nID)
{
    const char style = nID == ID_TEXT_BOLD_CMD ? 'B' : nID == ID_TEXT_ITALIC_CMD ? 'I' : 'U';
    m_canvas.ToggleTextStyle(style);
    m_canvas.FocusTextEdit();
}

void CMyPaintDlg::OnTextTransparent()
{
    m_canvas.SetTextTransparent(!m_canvas.IsTextTransparent());
    m_canvas.FocusTextEdit();
}

void CMyPaintDlg::OnEditInvertSelection()
{
    m_canvas.InvertSelection();
    m_canvas.SetFocus();
}

// ---------------------------------------------------------------------------
// 이미지
// ---------------------------------------------------------------------------

void CMyPaintDlg::OnImageProperties()
{
    const CString info = m_filePath.IsEmpty() ? CString(_T("저장하지 않은 그림")) : m_filePath;
    CImagePropertiesDlg dlg(m_canvas.GetImageSize(), info, this);
    if (dlg.DoModal() == IDOK)
        m_canvas.SetCanvasSize(dlg.m_size);
    m_canvas.SetFocus();
}

void CMyPaintDlg::OnImageFunctionGraph()
{
    m_canvas.FinishPendingWork();
    if (!m_graphUsed)
    {
        // 처음에는 캔버스보다 크지 않게, 선 굵기는 지금 도형 굵기로
        const CSize image = m_canvas.GetImageSize();
        m_graphOptions.size = CSize(std::clamp<int>(image.cx * 3 / 4, 200, 600), std::clamp<int>(image.cy * 3 / 4, 150, 450));
        m_graphOptions.lineWidth = std::max(2, CCanvasWnd::GetToolWidth(ToolType::Shape, m_canvas.GetSizeLevel()));
    }
    m_graphOptions.curveColor = m_color1; // 곡선 = 색 1, 배경 = 색 2 (그리기 도구와 같은 규칙)
    m_graphOptions.background = m_color2;

    CFunctionGraphDlg dlg(m_graphOptions, m_graphExpression, this);
    if (dlg.DoModal() == IDOK)
    {
        m_graphUsed = true;
        m_graphOptions = dlg.m_options;
        m_graphExpression = dlg.m_expression;
        m_canvas.InsertFloatingImage(dlg.m_result); // 떠 있는 선택 영역: 옮기기·크기 조정 후 확정
    }
    m_canvas.SetFocus();
}

void CMyPaintDlg::OnImageTransparentSel()
{
    m_canvas.SetTransparentSelection(!m_canvas.IsTransparentSelection());
}

void CMyPaintDlg::OnImageCrop()
{
    m_canvas.CropToSelection();
    m_canvas.SetFocus();
}

void CMyPaintDlg::OnImageResizeSkew()
{
    CResizeSkewDlg dlg(m_canvas.GetTransformTargetSize(), this);
    if (dlg.DoModal() == IDOK)
    {
        if (!m_canvas.ResizeAndSkew(dlg.m_newSize, dlg.m_skewX, dlg.m_skewY))
        {
            MessageBox(_T("결과 그림이 너무 큽니다. (한 변 최대 16000px)\n크기나 기울기를 줄여 주세요."),
                       _T("MyPaint"), MB_OK | MB_ICONWARNING);
        }
    }
    m_canvas.SetFocus();
}

void CMyPaintDlg::OnImageFlipRotate(UINT nID)
{
    static const FlipRotate kOps[] = { FlipRotate::RotateRight, FlipRotate::RotateLeft, FlipRotate::Rotate180,
                                       FlipRotate::FlipVertical, FlipRotate::FlipHorizontal };
    static_assert(ID_IMAGE_FLIP_HORIZONTAL - ID_IMAGE_ROTATE_RIGHT + 1 == _countof(kOps), "메뉴 ID 순서 확인");
    m_canvas.FlipRotateImage(kOps[nID - ID_IMAGE_ROTATE_RIGHT]);
    m_canvas.SetFocus();
}

// ---------------------------------------------------------------------------
// 편집 / 보기
// ---------------------------------------------------------------------------

void CMyPaintDlg::OnEditUndo()
{
    m_canvas.Undo();
}

void CMyPaintDlg::OnEditRedo()
{
    m_canvas.Redo();
}

void CMyPaintDlg::OnEditCut()
{
    m_canvas.CutSelection();
}

void CMyPaintDlg::OnEditCopy()
{
    m_canvas.CopySelection();
}

void CMyPaintDlg::OnEditPaste()
{
    m_canvas.Paste();
    m_canvas.SetFocus();
}

void CMyPaintDlg::OnEditDelete()
{
    m_canvas.DeleteSelection();
}

void CMyPaintDlg::OnEditSelectAll()
{
    m_canvas.SelectAll();
    m_canvas.SetFocus();
}

void CMyPaintDlg::OnViewZoomIn()
{
    m_canvas.ZoomIn();
}

void CMyPaintDlg::OnViewZoomOut()
{
    m_canvas.ZoomOut();
}

void CMyPaintDlg::OnViewZoom100()
{
    m_canvas.SetZoom(1.0);
}

void CMyPaintDlg::OnViewRulers()
{
    m_canvas.SetShowRulers(!m_canvas.GetShowRulers());
}

void CMyPaintDlg::OnViewGrid()
{
    m_canvas.SetShowGrid(!m_canvas.GetShowGrid());
}

void CMyPaintDlg::OnViewFullScreen()
{
    // 편집 중인 도형·떠 있는 선택 영역까지 화면에 보이는 그대로 (확정하지는 않음)
    CImage image;
    if (m_canvas.GetCompositeImage(image))
        CFullScreenWnd::Show(this, image);
}

void CMyPaintDlg::OnHelpShortcuts()
{
    MessageBox(
        _T("[파일]\n")
        _T("Ctrl+N 새로 만들기 · Ctrl+O 열기 · Ctrl+S 저장 · F12 다른 이름으로 저장 · Ctrl+P 인쇄\n\n")
        _T("[편집]\n")
        _T("Ctrl+Z 실행 취소 · Ctrl+Y 다시 실행\n")
        _T("Ctrl+X 잘라내기 · Ctrl+C 복사 · Ctrl+V 붙여넣기 · Del 선택 영역 삭제 · Ctrl+A 모두 선택\n")
        _T("Ctrl+Shift+I 선택 영역 반전\n\n")
        _T("[이미지]\n")
        _T("Ctrl+Shift+X 자르기 · Ctrl+W 크기 조정 및 기울이기 · Ctrl+E 이미지 속성\n")
        _T("Ctrl+Shift+G 함수 그래프 넣기 (n차 함수를 입력하면 그래프를 그려 줌)\n\n")
        _T("[보기]\n")
        _T("Ctrl+PgUp 확대 · Ctrl+PgDn 축소 · Ctrl+0 100% · Ctrl+휠 마우스 위치 기준 확대/축소\n")
        _T("Ctrl+R 눈금자 · Ctrl+G 격자선 · F11 전체 화면 (아무 키나 누르면 돌아옴)\n")
        _T("휠 세로 스크롤 · Shift+휠 가로 스크롤 · 돋보기: 왼쪽 클릭 확대, 오른쪽 클릭 축소\n\n")
        _T("[그리기]\n")
        _T("Ctrl+Plus / Ctrl+Minus 굵기 · Shift 정사각형/정원/45° · Esc 취소 또는 확정\n")
        _T("방향키 선택 영역 1px 이동 · Ctrl+드래그 선택 영역 복사 · 선택 영역 핸들로 크기 조정\n")
        _T("다각형: 시작점 클릭/더블클릭/Enter로 닫기 · 곡선: Enter로 휘기 생략\n\n")
        _T("[텍스트 입력 중]\n")
        _T("Ctrl+B 굵게 · Ctrl+I 기울임 · Ctrl+U 밑줄 · Esc 취소 · 바깥 클릭 확정"),
        _T("단축키 목록"), MB_OK | MB_ICONINFORMATION);
}

void CMyPaintDlg::OnViewStatusBar()
{
    m_statusVisible = !m_statusVisible;
    LayoutControls();
}

// ---------------------------------------------------------------------------
// 파일
// ---------------------------------------------------------------------------

bool CMyPaintDlg::ConfirmDiscardChanges()
{
    m_canvas.FinishPendingWork(); // 입력 중인 글, 떠 있는 선택 영역도 변경 내용에 포함
    if (!m_canvas.IsModified())
        return true;

    CString msg;
    msg.Format(_T("변경 내용을 %s에 저장하시겠습니까?"), DisplayName().GetString());
    switch (MessageBox(msg, _T("MyPaint"), MB_YESNOCANCEL | MB_ICONWARNING))
    {
    case IDYES:
        return m_filePath.IsEmpty() ? SaveAsWithDialog() : SaveTo(m_filePath);
    case IDNO:
        return true;
    default:
        return false;
    }
}

bool CMyPaintDlg::SaveTo(const CString& path)
{
    const HRESULT hr = m_canvas.SaveToFile(path);
    if (FAILED(hr))
    {
        CString msg;
        msg.Format(_T("파일을 저장하지 못했습니다.\n%s\n\n지원 형식: PNG, BMP, JPEG, GIF, TIFF (오류 코드 0x%08X)"),
                   path.GetString(), static_cast<unsigned>(hr));
        MessageBox(msg, _T("MyPaint"), MB_OK | MB_ICONERROR);
        return false;
    }
    m_filePath = path;
    m_canvas.SetModified(false); // CN_STATE 알림으로 제목도 갱신됨
    AddRecentFile(path);
    return true;
}

bool CMyPaintDlg::SaveAsWithDialog()
{
    CString initialName = DisplayName();
    const int dot = initialName.ReverseFind(_T('.'));
    if (dot > 0)
        initialName = initialName.Left(dot);

    CFileDialog dlg(FALSE, _T("png"), initialName,
                    OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY, kSaveFilter, this);
    dlg.m_ofn.nFilterIndex = m_filePath.IsEmpty() ? 1 : FilterIndexForPath(m_filePath);
    if (dlg.DoModal() != IDOK)
        return false;
    return SaveTo(dlg.GetPathName());
}

void CMyPaintDlg::OnFileNew()
{
    if (!ConfirmDiscardChanges())
        return;
    m_filePath.Empty();
    m_canvas.NewImage(kDefaultWidth, kDefaultHeight);
    UpdateTitle();
}

void CMyPaintDlg::OnFileOpen()
{
    if (!ConfirmDiscardChanges())
        return;

    CFileDialog dlg(TRUE, nullptr, nullptr, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY, kOpenFilter, this);
    if (dlg.DoModal() != IDOK)
        return;

    OpenImageFile(dlg.GetPathName());
}

bool CMyPaintDlg::OpenImageFile(const CString& path)
{
    const HRESULT hr = m_canvas.LoadFromFile(path);
    if (FAILED(hr))
    {
        CString msg;
        msg.Format(_T("파일을 열 수 없습니다.\n%s\n\n(오류 코드 0x%08X)"), path.GetString(), static_cast<unsigned>(hr));
        MessageBox(msg, _T("MyPaint"), MB_OK | MB_ICONERROR);
        return false;
    }
    m_filePath = path;
    AddRecentFile(path);
    UpdateTitle();
    return true;
}

void CMyPaintDlg::OnDropFiles(HDROP hDropInfo)
{
    // 여러 개를 끌어다 놓으면 첫 번째 파일만 엶
    CString path;
    const UINT length = ::DragQueryFile(hDropInfo, 0, nullptr, 0);
    if (length > 0)
    {
        ::DragQueryFile(hDropInfo, 0, path.GetBuffer(length + 1), length + 1);
        path.ReleaseBuffer();
    }
    ::DragFinish(hDropInfo);

    if (path.IsEmpty())
        return;
    SetForegroundWindow();
    if (ConfirmDiscardChanges())
        OpenImageFile(path);
}

// ---------------------------------------------------------------------------
// 최근 파일
// ---------------------------------------------------------------------------

void CMyPaintDlg::LoadRecentFiles()
{
    m_recentFiles.clear();
    for (int i = 0; i < kMaxRecentFiles; ++i)
    {
        CString key;
        key.Format(_T("File%d"), i + 1);
        const CString path = AfxGetApp()->GetProfileString(kRecentSection, key);
        if (!path.IsEmpty())
            m_recentFiles.push_back(path);
    }
}

void CMyPaintDlg::SaveRecentFiles() const
{
    for (int i = 0; i < kMaxRecentFiles; ++i)
    {
        CString key;
        key.Format(_T("File%d"), i + 1);
        AfxGetApp()->WriteProfileString(kRecentSection, key, i < static_cast<int>(m_recentFiles.size()) ? m_recentFiles[i].GetString() : _T(""));
    }
}

void CMyPaintDlg::AddRecentFile(const CString& path)
{
    // 이미 있으면 맨 앞으로, 넘치면 오래된 것부터 버림
    m_recentFiles.erase(std::remove_if(m_recentFiles.begin(), m_recentFiles.end(),
                                       [&](const CString& p) { return p.CompareNoCase(path) == 0; }),
                        m_recentFiles.end());
    m_recentFiles.insert(m_recentFiles.begin(), path);
    if (static_cast<int>(m_recentFiles.size()) > kMaxRecentFiles)
        m_recentFiles.resize(kMaxRecentFiles);
    SaveRecentFiles();
}

void CMyPaintDlg::RebuildRecentMenu(CMenu* pMenu) const
{
    // "최근 파일" 하위 메뉴인지: 첫 항목이 (없음) 또는 최근 파일 ID
    if (!pMenu || pMenu->GetMenuItemCount() == 0)
        return;
    const UINT firstId = pMenu->GetMenuItemID(0);
    if (firstId != ID_FILE_RECENT_NONE && (firstId < ID_FILE_RECENT_FIRST || firstId > ID_FILE_RECENT_LAST))
        return;

    while (pMenu->GetMenuItemCount() > 0)
        pMenu->DeleteMenu(0, MF_BYPOSITION);
    if (m_recentFiles.empty())
    {
        pMenu->AppendMenu(MF_STRING | MF_GRAYED, ID_FILE_RECENT_NONE, _T("(없음)"));
        return;
    }
    for (size_t i = 0; i < m_recentFiles.size(); ++i)
    {
        // 긴 경로는 가운데를 ...로 줄이고, 메뉴에서 &가 밑줄로 바뀌지 않게 &&로
        TCHAR compact[MAX_PATH] = {};
        ::PathCompactPathEx(compact, m_recentFiles[i], 60, 0);
        CString name(compact);
        name.Replace(_T("&"), _T("&&"));
        CString text;
        text.Format(_T("&%d %s"), static_cast<int>(i) + 1, name.GetString());
        pMenu->AppendMenu(MF_STRING, ID_FILE_RECENT_FIRST + static_cast<UINT>(i), text);
    }
}

void CMyPaintDlg::OnOpenRecent(UINT nID)
{
    const size_t index = nID - ID_FILE_RECENT_FIRST;
    if (index >= m_recentFiles.size())
        return;
    const CString path = m_recentFiles[index];
    if (!ConfirmDiscardChanges())
        return;
    if (!OpenImageFile(path))
    {
        // 없어졌거나 열 수 없는 파일은 목록에서 뺌
        m_recentFiles.erase(m_recentFiles.begin() + static_cast<std::ptrdiff_t>(std::min(index, m_recentFiles.size() - 1)));
        SaveRecentFiles();
    }
}

// ---------------------------------------------------------------------------
// 바탕 화면 배경
// ---------------------------------------------------------------------------

void CMyPaintDlg::OnSetWallpaper(UINT nID)
{
    // 저장된 파일만 배경으로 쓸 수 있으므로, 저장하지 않았거나 바뀌었으면 먼저 저장
    m_canvas.FinishPendingWork();
    if (m_filePath.IsEmpty() || m_canvas.IsModified())
    {
        if (MessageBox(_T("바탕 화면 배경으로 설정하려면 먼저 그림을 저장해야 합니다.\n저장할까요?"),
                       _T("MyPaint"), MB_YESNO | MB_ICONQUESTION) != IDYES)
            return;
        if (!(m_filePath.IsEmpty() ? SaveAsWithDialog() : SaveTo(m_filePath)))
            return;
    }

    const DESKTOP_WALLPAPER_POSITION position = nID == ID_FILE_WALLPAPER_TILE ? DWPOS_TILE
                                              : nID == ID_FILE_WALLPAPER_CENTER ? DWPOS_CENTER : DWPOS_FILL;
    const HRESULT hrInit = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    HRESULT hr = E_FAIL;
    {
        CComPtr<IDesktopWallpaper> wallpaper;
        hr = wallpaper.CoCreateInstance(CLSID_DesktopWallpaper);
        if (SUCCEEDED(hr))
            hr = wallpaper->SetPosition(position);
        if (SUCCEEDED(hr))
            hr = wallpaper->SetWallpaper(nullptr, m_filePath); // nullptr: 모든 모니터
    }
    if (SUCCEEDED(hrInit))
        ::CoUninitialize();

    if (FAILED(hr))
    {
        CString msg;
        msg.Format(_T("바탕 화면 배경으로 설정하지 못했습니다.\n%s\n\n(오류 코드 0x%08X)"),
                   m_filePath.GetString(), static_cast<unsigned>(hr));
        MessageBox(msg, _T("MyPaint"), MB_OK | MB_ICONERROR);
    }
}

void CMyPaintDlg::OnFileSave()
{
    m_canvas.FinishPendingWork();
    if (m_filePath.IsEmpty())
        SaveAsWithDialog();
    else
        SaveTo(m_filePath);
}

void CMyPaintDlg::OnFileSaveAs()
{
    m_canvas.FinishPendingWork();
    SaveAsWithDialog();
}

void CMyPaintDlg::OnFileExit()
{
    PostMessage(WM_CLOSE);
}

void CMyPaintDlg::OnFilePrint()
{
    m_canvas.FinishPendingWork();
    CString error;
    if (!PrintImage(this, m_canvas.GetImage(), DisplayName(), m_printSettings, error) && !error.IsEmpty())
        MessageBox(error, _T("인쇄"), MB_OK | MB_ICONERROR);
}

void CMyPaintDlg::OnFilePrintPreview()
{
    m_canvas.FinishPendingWork();
    if (ShowPrintPreview(this, m_canvas.GetImage(), m_printSettings))
        OnFilePrint(); // 미리 보기에서 [인쇄]
}

void CMyPaintDlg::OnFilePageSetup()
{
    ShowPageSetup(this, m_printSettings);
}
