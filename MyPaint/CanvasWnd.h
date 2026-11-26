#pragma once

/**
 * @file CanvasWnd.h
 * @brief 그림을 그리는 캔버스 창 선언
 *
 * 구현은 기능별로 나뉘어 있습니다.
 * - CanvasWnd.cpp       : 이미지 관리, 좌표/확대/스크롤, 화면 출력, 마우스·키보드 분배, 캔버스 크기 조절
 * - CanvasTools.cpp     : 연필, 지우개, 채우기, 스포이트
 * - CanvasShape.cpp     : 도형 (그리기, 곡선/다각형 단계, 확정 전 편집)
 * - CanvasSelection.cpp : 사각형/자유형 선택, 이동/복사, 클립보드
 * - CanvasTransform.cpp : 회전, 대칭, 크기 조정, 기울이기, 자르기
 * - CanvasText.cpp      : 텍스트 도구
 * - CanvasView.cpp      : 눈금자, 격자, 전체 화면용 합성 이미지
 */

#include "BrushEngine.h"
#include "ImageHistory.h"
#include "RasterOps.h"
#include "Shapes.h"

/**
 * @brief 그리기 도구 종류 (도구 버튼 ID 순서와 같아야 함)
 */
enum class ToolType
{
    Pencil,      ///< 연필 (자유선)
    Eraser,      ///< 지우개
    Fill,        ///< 채우기
    Picker,      ///< 스포이트 (색 선택)
    Text,        ///< 텍스트
    Select,      ///< 사각형 선택
    FreeSelect,  ///< 자유형 선택
    Shape,       ///< 도형 (종류는 SetShapeKind)
    Brush,       ///< 질감 브러시 (종류는 SetBrushKind)
    Magnifier,   ///< 돋보기: 왼쪽 클릭 확대, 오른쪽 클릭 축소
};

/** @brief 캔버스가 부모 창에 보내는 알림 메시지. wParam은 CanvasNotify 값 */
constexpr UINT WM_CANVAS_NOTIFY = WM_APP + 1;

/**
 * @brief WM_CANVAS_NOTIFY의 wParam
 */
enum CanvasNotify : WPARAM
{
    CN_CURSOR = 1,      ///< 커서 위치. lParam = MAKELPARAM(x, y), 이미지 밖이면 -1
    CN_STATE = 2,       ///< 이미지 크기, 수정 여부, 실행 취소 기록, 선택 상태 중 하나가 바뀜
    CN_SELSIZE = 3,     ///< 선택/도형/새 캔버스 크기. lParam = MAKELPARAM(cx, cy), 없으면 -1
    CN_ZOOM = 4,        ///< 확대 배율이 바뀜
    CN_TOOL = 5,        ///< 캔버스가 스스로 도구를 바꿈 (스포이트 후 복귀, 붙여넣기 후 선택 도구)
    CN_PICK_COLOR1 = 6, ///< 스포이트로 색 1을 고름. lParam = COLORREF
    CN_PICK_COLOR2 = 7, ///< 스포이트로 색 2를 고름. lParam = COLORREF
};

/** @brief 굵기 단계 수 */
constexpr int kSizeLevelCount = 4;

/**
 * @brief 텍스트 도구의 입력 상자. Esc를 캔버스에 전달하고, 대화 상자가 키를 가로채지 않게 합니다.
 */
class CTextEdit : public CEdit
{
protected:
    afx_msg UINT OnGetDlgCode();
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
    DECLARE_MESSAGE_MAP()
};

/**
 * @brief 캔버스 창. 이미지 보관, 그리기 도구, 확대/스크롤, 선택 영역, 실행 취소를 담당합니다.
 *
 * 화면 출력은 메모리 DC에 이미지와 미리 보기를 합성한 뒤 한 번에 복사하는
 * 더블 버퍼링 방식입니다.
 */
class CCanvasWnd : public CWnd
{
public:
    CCanvasWnd();

    /**
     * @brief 캔버스 창을 만듭니다.
     * @param pParent 부모 창
     * @param nID 컨트롤 ID
     * @return 성공하면 TRUE
     */
    BOOL Create(CWnd* pParent, UINT nID);

    // ----- 이미지 -----

    /** @brief 흰색 새 이미지를 만듭니다. 실행 취소 기록은 지워집니다. */
    bool NewImage(int width, int height);
    /** @brief 파일에서 이미지를 읽습니다. 투명 영역은 흰색 위에 합성됩니다. */
    HRESULT LoadFromFile(LPCTSTR path);
    /** @brief 이미지를 파일로 저장합니다. 형식은 확장자로 정해집니다. */
    HRESULT SaveToFile(LPCTSTR path);
    /** @brief 이미지 크기 */
    CSize GetImageSize() const;
    /** @brief 확정된 이미지 (인쇄용). 부르기 전에 FinishPendingWork로 진행 중인 작업을 확정하세요. */
    CImage& GetImage() { return m_image; }

    // ----- 도구 -----

    /** @brief 현재 도구를 바꿉니다. 진행 중인 텍스트·도형·선택 영역은 확정됩니다. */
    void SetTool(ToolType tool);
    /** @brief 현재 도구 */
    ToolType GetTool() const { return m_tool; }
    /** @brief 굵기 설정을 쓰는 도구인지 여부 */
    static bool ToolUsesSize(ToolType tool);
    /** @brief 굵기 단계 (0 ~ kSizeLevelCount-1) 지정. 편집 중인 도형에도 적용됩니다. */
    void SetSizeLevel(int level);
    /** @brief 굵기 단계 */
    int GetSizeLevel() const { return m_sizeLevel; }
    /** @brief 도구별 굵기 단계에 해당하는 픽셀 값 */
    static int GetToolWidth(ToolType tool, int level);
    /** @brief 그리기 색 지정. color1은 왼쪽 버튼, color2는 오른쪽 버튼·지우개·배경·채우기 색 */
    void SetColors(COLORREF color1, COLORREF color2);

    // ----- 도형 -----

    /** @brief 그릴 도형 종류. 편집 중인 도형은 먼저 확정됩니다. */
    void SetShapeKind(ShapeKind kind);
    /** @brief 그릴 도형 종류 */
    ShapeKind GetShapeKind() const { return m_shapeKind; }
    /** @brief 윤곽선/채우기 방식. 편집 중인 도형에도 적용됩니다. */
    void SetShapeOptions(ShapePaint outline, ShapePaint fill);
    /** @brief 윤곽선 방식 */
    ShapePaint GetShapeOutline() const { return m_shapeOutline; }
    /** @brief 채우기 방식 */
    ShapePaint GetShapeFill() const { return m_shapeFill; }
    /** @brief 도형을 그리는 중이거나 편집 중인지 */
    bool IsShapeActive() const { return m_shapeStage != ShapeStage::None; }

    // ----- 브러시 -----

    /** @brief 브러시 종류 */
    void SetBrushKind(BrushKind kind) { m_brushKind = kind; }
    /** @brief 브러시 종류 */
    BrushKind GetBrushKind() const { return m_brushKind; }

    // ----- 텍스트 -----

    /**
     * @brief 텍스트 글꼴 지정. 입력 중인 글상자에도 바로 적용됩니다.
     * @param lf 글꼴 이름과 굵게/기울임/밑줄/취소선 (높이는 무시)
     * @param pointSize 글자 크기 (포인트, 96 DPI 기준으로 픽셀 변환)
     */
    void SetTextFont(const LOGFONT& lf, int pointSize);
    /** @brief 텍스트 글꼴 */
    void GetTextFont(LOGFONT& lf, int& pointSize) const;
    /** @brief 텍스트 배경 투명 여부 지정 (불투명이면 글상자를 색 2로 채움) */
    void SetTextTransparent(bool transparent) { m_textTransparent = transparent; }
    /** @brief 텍스트 배경 투명 여부 */
    bool IsTextTransparent() const { return m_textTransparent; }
    /** @brief 글상자에 입력 중인지 여부 */
    bool IsTextEditing() const { return m_textActive; }
    /** @brief hWnd가 글상자 입력 창인지 여부 */
    bool IsTextEditWindow(HWND hWnd) const { return hWnd && hWnd == m_textEdit.GetSafeHwnd(); }
    /** @brief 입력 중인 글상자에 포커스를 돌려줍니다. */
    void FocusTextEdit();
    /** @brief 텍스트 굵게('B')/기울임('I')/밑줄('U') 켜고 끄기 (글상자 입력 중 Ctrl+B/I/U) */
    void ToggleTextStyle(char style);

    // ----- 선택 영역과 클립보드 -----

    /** @brief 선택 영역이 있는지 여부 */
    bool HasSelection() const { return m_hasSelection; }
    /** @brief 이미지 전체를 선택합니다. */
    void SelectAll();
    /** @brief 선택 영역 반전: 지금 선택하지 않은 부분을 선택합니다. */
    void InvertSelection();
    /** @brief 선택 영역을 지웁니다 (색 2로 채움). */
    void DeleteSelection();
    /** @brief 선택 영역을 클립보드에 복사합니다. */
    bool CopySelection();
    /** @brief 선택 영역을 잘라냅니다. */
    bool CutSelection();
    /** @brief 클립보드 그림을 떠 있는 선택 영역으로 붙여넣습니다. */
    bool Paste();
    /**
     * @brief 그림을 떠 있는 선택 영역으로 넣습니다 (보이는 영역 가운데). 함수 그래프 넣기용
     * @param image 32비트, 미리 곱한 알파 (알파 0인 곳은 투명하게 얹힘)
     */
    bool InsertFloatingImage(const CImage& image);
    /** @brief 클립보드에 붙여넣을 그림이 있는지 여부 */
    static bool CanPaste();
    /** @brief 투명 선택: 떠 있는 그림에서 색 2와 같은 픽셀을 투명하게 */
    void SetTransparentSelection(bool transparent);
    /** @brief 투명 선택 여부 */
    bool IsTransparentSelection() const { return m_transparentSelection; }

    // ----- 이미지 변환 (선택 영역이 있으면 선택 영역에, 없으면 이미지 전체에) -----

    /** @brief 회전/대칭 */
    void FlipRotateImage(FlipRotate op);
    /** @brief 크기 조정·기울이기 대상의 현재 크기 */
    CSize GetTransformTargetSize() const;
    /**
     * @brief 크기 조정 후 기울이기
     * @param newSize 새 크기 (픽셀)
     * @param skewX 가로 기울기 (도)
     * @param skewY 세로 기울기 (도)
     * @return 결과가 너무 커서(한 변 16000px 초과) 하지 않았으면 false
     */
    bool ResizeAndSkew(CSize newSize, int skewX, int skewY);
    /** @brief 이미지를 선택 영역으로 자릅니다. */
    void CropToSelection();
    /** @brief 캔버스 크기 변경 (이미지 속성). 늘어난 부분은 색 2 */
    void SetCanvasSize(CSize size);

    // ----- 확대/축소 -----

    /** @brief 확대 배율 (1.0 = 100%) */
    double GetZoom() const { return m_zoom; }
    /** @brief 확대 배율 지정. 화면 가운데를 기준으로 확대합니다. */
    void SetZoom(double zoom);
    /** @brief 한 단계 확대 */
    void ZoomIn();
    /** @brief 한 단계 축소 */
    void ZoomOut();
    /** @brief 확대 단계 수 */
    static int ZoomLevelCount();
    /** @brief i번째 확대 단계 (0 = 12.5%) */
    static double ZoomLevelAt(int index);
    /** @brief 배율에 가장 가까운 단계 번호 */
    static int NearestZoomLevel(double zoom);

    // ----- 보기 -----

    /** @brief 눈금자 표시 */
    void SetShowRulers(bool show);
    bool GetShowRulers() const { return m_showRulers; }
    /** @brief 격자선 표시 */
    void SetShowGrid(bool show);
    bool GetShowGrid() const { return m_showGrid; }
    /**
     * @brief 화면에 보이는 그대로의 그림 (편집 중인 도형, 떠 있는 선택 영역 포함). 전체 화면 보기용
     * @return 성공하면 true
     */
    bool GetCompositeImage(CImage& out);

    // ----- 실행 취소 -----

    bool CanUndo() const;
    bool CanRedo() const;
    void Undo();
    void Redo();

    /** @brief 마지막 저장 이후 수정되었는지 여부 */
    bool IsModified() const { return m_modified; }
    /** @brief 수정 여부 지정 (저장 후 false로) */
    void SetModified(bool modified);

    /** @brief Esc: 드래그·그리는 중인 도형·글상자는 취소, 편집 중인 도형·선택 영역은 확정 */
    void CancelOperation();
    /** @brief 진행 중인 작업을 마무리합니다: 드래그 취소, 글상자·도형·선택 영역 확정 */
    void FinishPendingWork();

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnMouseHWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
    afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnRButtonDblClk(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnMouseLeave();
    afx_msg void OnCaptureChanged(CWnd* pWnd);
    afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
    afx_msg UINT OnGetDlgCode();
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    afx_msg void OnTextChanged();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    DECLARE_MESSAGE_MAP()

private:
    /** @brief 드래그 중인 마우스 버튼 */
    enum class DragButton { None, Left, Right };

    /** @brief 드래그 중인 작업 */
    enum class DragMode
    {
        None,
        Freehand,      ///< 연필/지우개
        ShapeDraw,     ///< 도형 그리기 (처음 드래그, 곡선 휘기, 다각형 꼭짓점)
        ShapeMove,     ///< 편집 중인 도형 옮기기
        ShapeHandle,   ///< 편집 중인 도형 크기 핸들
        SelectRect,    ///< 사각형 선택 영역 지정
        FreeSelect,    ///< 자유형 선택 영역 지정
        MoveSelection, ///< 선택 영역 이동
        SelectionHandle, ///< 선택 영역 크기 핸들
        TextRect,      ///< 글상자 영역 지정
        ResizeCanvas,  ///< 캔버스 크기 핸들
    };

    /** @brief 도형 진행 단계 */
    enum class ShapeStage
    {
        None,
        Drawing,     ///< 처음 드래그 중
        CurveBend1,  ///< 곡선: 첫 번째 휘기를 기다리는 중
        CurveBend2,  ///< 곡선: 두 번째 휘기를 기다리는 중
        Polygon,     ///< 다각형: 꼭짓점을 추가하는 중
        Editing,     ///< 확정 전 편집 (핸들로 크기/위치 조정, 색·굵기 변경)
    };

    /** @brief 캔버스 크기 핸들 */
    enum Handle { HandleNone = 0, HandleRight, HandleBottom, HandleCorner };

    // --- 좌표 변환 (CanvasWnd.cpp) ---
    CPoint ImageOrigin() const;                   ///< 이미지 (0,0)의 클라이언트 좌표
    int Scale(int v) const;                       ///< 이미지 길이 → 화면 길이
    CPoint ClientToImage(CPoint pt) const;        ///< 클라이언트 → 이미지 좌표 (내림)
    CPoint ImageToClient(CPoint pt) const;        ///< 이미지 → 클라이언트 좌표
    CRect ImageToClient(const CRect& rc) const;   ///< 이미지 → 클라이언트 사각형
    CRect ImageBounds() const { return CRect(CPoint(0, 0), GetImageSize()); }
    bool IsInImage(CPoint img) const { return ImageBounds().PtInRect(img) != FALSE; }

    // --- 스크롤/확대 (CanvasWnd.cpp) ---
    CSize ContentSize() const;
    void UpdateScrollBars();
    void ScrollTo(int x, int y);
    void HandleScroll(int bar, UINT code);
    void SetZoomAt(double zoom, CPoint anchorClient);

    // --- 화면 출력 (CanvasWnd.cpp) ---
    void DrawImageScaled(CDC& dc, CImage& image, const CRect& imageRect, const CRect& clip, bool alpha) const;
    void DrawOverlays(CDC& dc) const;
    void DrawHandleBox(CDC& dc, const CRect& rc) const;
    void InvalidateImageRect(CRect rcImage);

    // --- 캔버스 크기 (CanvasWnd.cpp) ---
    CRect HandleRect(Handle handle) const;
    Handle HitTestHandle(CPoint clientPt) const;
    CSize ResizeTargetSize(CPoint clientPt) const;
    void ResizeCanvasTo(CSize size, bool recordHistory);

    // --- 마우스 분배 (CanvasWnd.cpp) ---
    void OnButtonDown(DragButton button, UINT nFlags, CPoint point);
    void OnButtonUp(DragButton button, UINT nFlags, CPoint point);
    void BeginDrag(DragMode mode, DragButton button, CPoint imgPt);
    void EndDrag(CPoint imgPt, UINT nFlags);
    void CancelDrag();
    void ChangeToolInternal(ToolType tool);
    void FinishPendingWorkExceptSelection();

    // --- 그리기 도구 (CanvasTools.cpp) ---
    void BeginFreehand(DragButton button, CPoint imgPt);
    void ContinueFreehand(CPoint imgPt);
    void DrawStrokeSegment(CDC& dc, CPoint from, CPoint to, COLORREF color) const;
    CRect EraseSegment(CDC& dc, CPoint from, CPoint to);
    CRect EraserRect(CPoint center) const;
    void DoFill(CPoint imgPt, COLORREF color);
    void DoPick(CPoint imgPt, DragButton button);
    CPoint ConstrainPoint(CPoint start, CPoint end, bool shift, bool lineMode) const;

    // --- 도형 (CanvasShape.cpp) ---
    ShapeStyle CurrentShapeStyle() const;
    ShapePaint EffectiveOutline() const;
    ShapePaint EffectiveFill() const;
    int TextureWidth() const;
    void RenderShape(CImage& target, const uint32_t* before);
    void TextureFillShape(CImage& target, const uint32_t* before, ShapePaint paint, COLORREF color);
    void TextureOutlineShape(CImage& target, const uint32_t* before, ShapePaint paint, COLORREF color);
    void ShapeButtonDown(DragButton button, UINT nFlags, CPoint imgPt, CPoint clientPt);
    void ShapeDragMove(CPoint imgPt, bool shift);
    void ShapeDragEnd(CPoint imgPt, bool shift);
    void ShapeDragCancel();
    void FinishPolygon();
    void RefreshShapePreview();
    void CommitShape();
    void CancelShape();
    std::vector<CRect> ShapeHandleRects() const;  ///< 편집 핸들 (클라이언트 좌표)
    int HitTestShapeHandle(CPoint clientPt) const;
    bool HitTestShapeBody(CPoint clientPt) const;
    void ApplyShapeHandleDrag(CPoint imgPt, bool shift);

    // --- 선택 영역 (CanvasSelection.cpp) ---
    std::vector<CRect> SelectionHandleRects() const;  ///< 선택 영역 크기 핸들 (클라이언트 좌표)
    int HitTestSelectionHandle(CPoint clientPt) const;
    void BeginSelectionResize(int handle, CPoint imgPt);
    void UpdateSelectionResize(CPoint imgPt);
    void CancelSelectionResize();
    CRect SelectionFromDrag(CPoint a, CPoint b) const;
    void FinishFreeSelect();
    bool BuildSelectionMask(std::vector<uint8_t>& mask) const;
    bool BuildSelectionImage(CImage& out) const;
    void BeginMoveSelection(CPoint imgPt, bool copy);
    void LiftSelection(bool copy);
    void StampFloating();
    void DropSelection();
    void ClearSelectionState();
    void MoveSelectionBy(int dx, int dy);
    bool WriteClipboard(CImage& source);
    bool ReadClipboard(CImage& out);
    bool InsertFloating(CImage& source, bool center); ///< 붙여넣기·그림 넣기 공통 (source는 CreateDib32로 만든 것)

    void RebuildFloatImage();

    // --- 보기 (CanvasView.cpp) ---
    int RulerSize() const;                    ///< 눈금자 두께 (꺼져 있으면 0)
    bool IsInRuler(CPoint clientPt) const;
    void DrawRulers(CDC& dc) const;
    void DrawGrid(CDC& dc, const CRect& client) const;

    // --- 변환 도우미 (CanvasTransform.cpp) ---
    static PixelBuffer ToBuffer(const CImage& image);
    static bool FromBuffer(CImage& image, const PixelBuffer& buffer);

    // --- 텍스트 (CanvasText.cpp) ---
    int TextPixelHeight() const;
    void MakeTextFont(CFont& font, double scale) const;
    void StartText(CRect rect);
    void UpdateTextEditPlacement();
    void CommitText();
    void CancelText();
    void HideTextEdit();

    // --- 이미지/기록 (CanvasWnd.cpp) ---
    static bool CreateDib32(CImage& image, int width, int height, COLORREF fill);
    static void ReplaceImage(CImage& target, CImage& source);
    ImageSnapshot CaptureSnapshot() const;
    void RestoreSnapshot(const ImageSnapshot& snapshot);
    void ResetAfterImageChange();
    void MarkModified();
    bool EnsurePreview();

    void NotifyParent(CanvasNotify code, LPARAM lParam = 0);
    void NotifyCursor(CPoint clientPt);
    void NotifySelectionSize();

    int CurrentWidth() const { return GetToolWidth(m_tool, m_sizeLevel); }
    COLORREF DragColor() const { return m_dragButton == DragButton::Right ? m_color2 : m_color1; }

    // 이미지
    CImage m_image;              ///< 확정된 이미지 (32비트, 위→아래 DIB)
    CImage m_preview;            ///< 도형 미리 보기 이미지 (m_image + 도형)
    CImageHistory m_history;     ///< 실행 취소 기록
    ImageSnapshot m_beforeDrag;  ///< 자유선 드래그 시작 전 이미지 (확정 시 기록, 취소 시 복원)
    bool m_modified = false;

    // 도구
    ToolType m_tool = ToolType::Pencil;
    ToolType m_toolBeforePicker = ToolType::Pencil; ///< 스포이트 사용 후 돌아갈 도구
    int m_sizeLevel = 0;
    COLORREF m_color1 = RGB(0, 0, 0);
    COLORREF m_color2 = RGB(255, 255, 255);

    // 드래그
    DragMode m_dragMode = DragMode::None;
    DragButton m_dragButton = DragButton::None;
    CPoint m_dragStart;          ///< 드래그 시작점 (이미지 좌표)
    CPoint m_dragLast;           ///< 직전 마우스 위치 (이미지 좌표, 제약 전)
    Handle m_resizeHandle = HandleNone;
    CSize m_resizeSize;          ///< 크기 핸들로 지정 중인 새 캔버스 크기
    CRect m_textDragRect;        ///< 글상자 영역 지정 중인 사각형

    // 도형
    ShapeKind m_shapeKind = ShapeKind::Line;
    ShapePaint m_shapeOutline = ShapePaint::Solid;
    ShapePaint m_shapeFill = ShapePaint::None;
    unsigned m_shapeSeed = 1;    ///< 질감(유화 붓털 등) 난수 시드: 도형마다 고정해서 미리 보기가 떨리지 않게
    ShapeStage m_shapeStage = ShapeStage::None;
    ShapeData m_shape;           ///< 그리는 중/편집 중인 도형
    ShapeData m_shapeAtDragStart;///< 편집 드래그 시작 시의 도형 (취소 시 복원, 핸들 계산 기준)
    bool m_shapeSwapped = false; ///< 오른쪽 버튼으로 그림: 윤곽선 = 색 2, 채우기 = 색 1
    int m_shapeHandle = -1;      ///< 끌고 있는 편집 핸들
    CRect m_shapeDrawnArea;      ///< m_preview에 도형을 그린 영역 (다음에 지울 곳)

    // 선택 영역
    bool m_hasSelection = false;
    CRect m_selRect;             ///< 선택 영역 (이미지 좌표). 떠 있으면 떠 있는 그림의 위치
    bool m_selFreeform = false;  ///< 윤곽선(고리)으로 정해지는 선택인지 (자유형, 반전)
    /**
     * @brief 선택 윤곽선 고리들 (m_selRect 왼쪽 위 기준 상대 좌표)
     * 1개: 자유형 선택 (안쪽 전부). 2개 이상: 반전 선택 (교대 채우기 = 바깥 테두리 안, 원래 윤곽 밖)
     */
    std::vector<std::vector<CPoint>> m_selRings;
    int m_selHandle = -1;                       ///< 끌고 있는 선택 영역 핸들
    CRect m_selDragStartRect;                   ///< 핸들 드래그 시작 시 선택 영역
    PixelBuffer m_selDragSource;                ///< 핸들 드래그 시작 시 떠 있는 그림 원본
    std::vector<std::vector<CPoint>> m_selDragRings; ///< 핸들 드래그 시작 시 윤곽선
    std::vector<CPoint> m_lasso; ///< 자유형 선택 드래그 중인 경로 (이미지 좌표)
    bool m_floating = false;     ///< 선택 영역의 그림을 떼어 낸 상태인지
    CImage m_floatSource;        ///< 떠 있는 그림 원본 (미리 곱한 알파: 선택 밖은 투명)
    CImage m_floatImage;         ///< 화면 표시·찍기용 = 원본 + 투명 선택 적용
    bool m_transparentSelection = false;
    CPoint m_moveGrab;           ///< 이동 시 잡은 점과 선택 영역 왼쪽 위의 차이

    // 텍스트
    CTextEdit m_textEdit;        ///< 글상자 입력 창 (캔버스의 자식)
    CFont m_textEditFont;        ///< 입력 창용 글꼴 (확대 배율 적용)
    CBrush m_textBrush;          ///< 입력 창 배경 브러시 (색 2)
    bool m_textActive = false;
    CRect m_textRect;            ///< 글상자 (이미지 좌표)
    int m_textMinHeight = 0;     ///< 사용자가 지정한 글상자 높이
    LOGFONT m_textLogFont{};
    int m_textPointSize = 12;
    bool m_textTransparent = false;

    // 브러시
    BrushKind m_brushKind = BrushKind::Normal;
    CBrushStroke m_brushStroke;

    // 화면
    bool m_showRulers = false;
    bool m_showGrid = false;
    double m_zoom = 1.0;
    CPoint m_scroll;             ///< 스크롤 위치 (화면 픽셀)
    bool m_updatingScroll = false;
    bool m_trackingMouse = false;
    bool m_hoverValid = false;   ///< 지우개 미리 보기용 마우스 위치가 유효한지
    CPoint m_hoverPt;            ///< 마우스 위치 (이미지 좌표)
    LPARAM m_lastCursorNotify = -2;
    LPARAM m_lastSelSizeNotify = -2;
};
