/**
 * @file CanvasWnd.cpp
 * @brief 캔버스 핵심: 이미지 관리, 좌표/확대/스크롤, 화면 출력, 마우스·키보드 분배, 캔버스 크기 조절
 */
#include "pch.h"
#include "CanvasWnd.h"
#include "CanvasInternal.h"
#include "Theme.h"

#include <cmath>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

using namespace canvas;

BEGIN_MESSAGE_MAP(CCanvasWnd, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
    ON_WM_HSCROLL()
    ON_WM_VSCROLL()
    ON_WM_MOUSEWHEEL()
    ON_WM_MOUSEHWHEEL()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_RBUTTONDOWN()
    ON_WM_RBUTTONUP()
    ON_WM_RBUTTONDBLCLK()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
    ON_WM_CAPTURECHANGED()
    ON_WM_SETCURSOR()
    ON_WM_GETDLGCODE()
    ON_WM_KEYDOWN()
    ON_WM_KEYUP()
    ON_WM_CTLCOLOR()
    ON_EN_CHANGE(kTextEditId, &CCanvasWnd::OnTextChanged)
    ON_WM_TIMER()
END_MESSAGE_MAP()

CCanvasWnd::CCanvasWnd()
{
    m_textLogFont.lfWeight = FW_NORMAL;
    m_textLogFont.lfCharSet = DEFAULT_CHARSET;
    m_textLogFont.lfQuality = ANTIALIASED_QUALITY;
    _tcscpy_s(m_textLogFont.lfFaceName, _T("맑은 고딕"));
    m_textBrush.CreateSolidBrush(m_color2);
}

BOOL CCanvasWnd::Create(CWnd* pParent, UINT nID)
{
    // CS_DBLCLKS: 다각형을 더블클릭으로 닫기 위해 (다른 도구는 더블클릭을 일반 클릭으로 처리)
    const CString cls = AfxRegisterWndClass(CS_DBLCLKS, ::LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr);
    // WS_CLIPCHILDREN: 화면을 통째로 다시 그릴 때 글상자 입력 창을 덮어쓰지 않도록
    if (!CWnd::CreateEx(0, cls, nullptr,
        WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL | WS_TABSTOP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        CRect(0, 0, 10, 10), pParent, nID))
        return FALSE;
    theme::UseDarkScrollbars(m_hWnd); // 어두운 테마 스크롤바
    return TRUE;
}

// ---------------------------------------------------------------------------
// 이미지
// ---------------------------------------------------------------------------

bool CCanvasWnd::CreateDib32(CImage& image, int width, int height, COLORREF fill)
{
    image.Destroy();
    CImage temp;
    // 높이를 음수로 주면 위에서 아래 순서(top-down) DIB가 만들어집니다.
    if (width <= 0 || height <= 0 || !temp.Create(width, -height, 32))
        return false;

    HDC hdc = temp.GetDC();
    CDC::FromHandle(hdc)->FillSolidRect(0, 0, width, height, fill);
    temp.ReleaseDC();

    // 방향을 명시해야 합니다. 기본값(DIBOR_DEFAULT)은 GetObject가 돌려주는 높이 부호로 판단하는데,
    // DIB 섹션은 양수로 돌려주므로 위→아래 이미지를 아래→위로 오해해 GetPixelAddress가 상하 반전됩니다.
    image.Attach(temp.Detach(), CImage::DIBOR_TOPDOWN);
    return true;
}

void CCanvasWnd::ReplaceImage(CImage& target, CImage& source)
{
    target.Destroy();
    target.Attach(source.Detach(), CImage::DIBOR_TOPDOWN); // CreateDib32로 만든 이미지만 넘어옴
}

bool CCanvasWnd::NewImage(int width, int height)
{
    FinishPendingWork();
    CImage image;
    if (!CreateDib32(image, width, height, RGB(255, 255, 255)))
        return false;
    ReplaceImage(m_image, image);
    ResetAfterImageChange();
    return true;
}

HRESULT CCanvasWnd::LoadFromFile(LPCTSTR path)
{
    FinishPendingWork();

    CImage src;
    HRESULT hr = src.Load(path);
    if (FAILED(hr))
        return hr;

    CImage dst;
    if (!CreateDib32(dst, src.GetWidth(), src.GetHeight(), RGB(255, 255, 255)))
        return E_OUTOFMEMORY;

    // 투명한 부분이 검게 보이지 않도록 흰 바탕 위에 합성합니다.
    HDC hdc = dst.GetDC();
    src.Draw(hdc, 0, 0);
    dst.ReleaseDC();

    ReplaceImage(m_image, dst);
    ResetAfterImageChange();
    return S_OK;
}

HRESULT CCanvasWnd::SaveToFile(LPCTSTR path)
{
    if (m_image.IsNull())
        return E_FAIL;
    FinishPendingWork();

    // 어떤 형식으로든 문제없이 저장되도록 24비트로 변환해서 저장합니다.
    const CSize size = GetImageSize();
    CImage out;
    if (!out.Create(size.cx, size.cy, 24))
        return E_OUTOFMEMORY;
    HDC hdc = out.GetDC();
    m_image.BitBlt(hdc, 0, 0);
    out.ReleaseDC();

    return out.Save(path); // 형식은 확장자로 결정
}

CSize CCanvasWnd::GetImageSize() const
{
    if (m_image.IsNull())
        return CSize(0, 0);
    return CSize(m_image.GetWidth(), m_image.GetHeight());
}

ImageSnapshot CCanvasWnd::CaptureSnapshot() const
{
    ImageSnapshot snap;
    if (m_image.IsNull())
        return snap;

    GdiFlush(); // GDI로 그린 내용이 픽셀 메모리에 반영되도록
    snap.width = m_image.GetWidth();
    snap.height = m_image.GetHeight();
    snap.pixels.resize(static_cast<size_t>(snap.width) * snap.height);
    for (int y = 0; y < snap.height; ++y)
    {
        memcpy(&snap.pixels[static_cast<size_t>(y) * snap.width],
               m_image.GetPixelAddress(0, y), static_cast<size_t>(snap.width) * 4);
    }
    return snap;
}

void CCanvasWnd::RestoreSnapshot(const ImageSnapshot& snap)
{
    if (snap.IsEmpty())
        return;
    if (GetImageSize() != CSize(snap.width, snap.height))
    {
        CImage image;
        if (!CreateDib32(image, snap.width, snap.height, RGB(255, 255, 255)))
            return;
        ReplaceImage(m_image, image);
    }

    GdiFlush();
    for (int y = 0; y < snap.height; ++y)
    {
        memcpy(m_image.GetPixelAddress(0, y),
               &snap.pixels[static_cast<size_t>(y) * snap.width], static_cast<size_t>(snap.width) * 4);
    }
}

void CCanvasWnd::ResetAfterImageChange()
{
    m_history.Clear();
    m_modified = false;
    m_scroll = CPoint(0, 0);
    if (GetSafeHwnd())
    {
        UpdateScrollBars();
        Invalidate(FALSE);
    }
    NotifyParent(CN_STATE);
    NotifySelectionSize();
}

void CCanvasWnd::MarkModified()
{
    m_modified = true;
    NotifyParent(CN_STATE);
}

bool CCanvasWnd::EnsurePreview()
{
    // 미리 보기 이미지 = 확정 이미지의 사본. 도형은 여기에만 그렸다가 확정할 때 m_image에 그림
    const CSize size = GetImageSize();
    if (m_preview.IsNull() || m_preview.GetWidth() != size.cx || m_preview.GetHeight() != size.cy)
    {
        if (!CreateDib32(m_preview, size.cx, size.cy, RGB(255, 255, 255)))
            return false;
    }
    HDC hdc = m_preview.GetDC();
    m_image.BitBlt(hdc, 0, 0);
    m_preview.ReleaseDC();
    m_shapeDrawnArea.SetRectEmpty();
    return true;
}

// ---------------------------------------------------------------------------
// 설정
// ---------------------------------------------------------------------------

void CCanvasWnd::SetTool(ToolType tool)
{
    FinishPendingWork();
    if (tool == ToolType::Picker && m_tool != ToolType::Picker)
        m_toolBeforePicker = m_tool;
    m_tool = tool;
    Invalidate(FALSE);
}

void CCanvasWnd::ChangeToolInternal(ToolType tool)
{
    SetTool(tool);
    NotifyParent(CN_TOOL);
}

bool CCanvasWnd::ToolUsesSize(ToolType tool)
{
    return tool == ToolType::Pencil || tool == ToolType::Eraser || tool == ToolType::Shape || tool == ToolType::Brush;
}

void CCanvasWnd::SetSizeLevel(int level)
{
    m_sizeLevel = std::clamp(level, 0, kSizeLevelCount - 1);
    RefreshShapePreview(); // 편집 중인 도형에도 바로 반영
    Invalidate(FALSE);
}

int CCanvasWnd::GetToolWidth(ToolType tool, int level)
{
    static constexpr int kPencil[kSizeLevelCount] = { 1, 2, 3, 4 };
    static constexpr int kEraser[kSizeLevelCount] = { 4, 6, 8, 10 };
    static constexpr int kShape[kSizeLevelCount] = { 1, 3, 5, 8 };
    static constexpr int kBrush[kSizeLevelCount] = { 4, 8, 14, 20 };

    level = std::clamp(level, 0, kSizeLevelCount - 1);
    switch (tool)
    {
    case ToolType::Pencil: return kPencil[level];
    case ToolType::Eraser: return kEraser[level];
    case ToolType::Brush:  return kBrush[level];
    default:               return kShape[level];
    }
}

void CCanvasWnd::SetColors(COLORREF color1, COLORREF color2)
{
    m_color1 = color1;
    m_color2 = color2;
    m_textBrush.DeleteObject();
    m_textBrush.CreateSolidBrush(m_color2);
    if (m_textActive)
        m_textEdit.Invalidate();
    RefreshShapePreview(); // 편집 중인 도형에도 바로 반영
    if (m_floating && m_transparentSelection)
    {
        RebuildFloatImage(); // 투명하게 할 색(색 2)이 바뀜
        Invalidate(FALSE);
    }
}

// ---------------------------------------------------------------------------
// 실행 취소 / 다시 실행
// ---------------------------------------------------------------------------

bool CCanvasWnd::CanUndo() const
{
    // 떠 있는 선택 영역·글상자·도형은 확정하면 기록이 생기므로 실행 취소할 수 있음
    return m_history.CanUndo() || m_floating || m_textActive || IsShapeActive();
}

bool CCanvasWnd::CanRedo() const
{
    return m_history.CanRedo();
}

void CCanvasWnd::Undo()
{
    FinishPendingWork();
    ImageSnapshot current = CaptureSnapshot();
    if (!m_history.Undo(current))
        return;
    RestoreSnapshot(current);
    UpdateScrollBars();
    Invalidate(FALSE);
    MarkModified();
}

void CCanvasWnd::Redo()
{
    FinishPendingWork();
    ImageSnapshot current = CaptureSnapshot();
    if (!m_history.Redo(current))
        return;
    RestoreSnapshot(current);
    UpdateScrollBars();
    Invalidate(FALSE);
    MarkModified();
}

void CCanvasWnd::SetModified(bool modified)
{
    m_modified = modified;
    NotifyParent(CN_STATE);
}

void CCanvasWnd::CancelOperation()
{
    if (m_dragMode != DragMode::None)
        CancelDrag();
    else if (m_textActive)
        CancelText();
    else if (m_shapeStage == ShapeStage::Editing)
        CommitShape();      // 다 그린 도형은 확정
    else if (IsShapeActive())
        CancelShape();      // 그리는 중인 곡선/다각형은 취소
    else
        DropSelection();
}

void CCanvasWnd::FinishPendingWorkExceptSelection()
{
    if (m_dragMode != DragMode::None)
        CancelDrag();
    if (m_textActive)
        CommitText();
    if (IsShapeActive())
        CommitShape();
}

void CCanvasWnd::FinishPendingWork()
{
    FinishPendingWorkExceptSelection();
    DropSelection();
}

// ---------------------------------------------------------------------------
// 알림
// ---------------------------------------------------------------------------

void CCanvasWnd::NotifyParent(CanvasNotify code, LPARAM lParam)
{
    if (GetSafeHwnd() && GetParent())
        GetParent()->SendMessage(WM_CANVAS_NOTIFY, code, lParam);
}

void CCanvasWnd::NotifyCursor(CPoint clientPt)
{
    LPARAM value = -1;
    const CPoint img = ClientToImage(clientPt);
    if (IsInImage(img))
        value = MAKELPARAM(img.x, img.y);

    if (value != m_lastCursorNotify)
    {
        m_lastCursorNotify = value;
        NotifyParent(CN_CURSOR, value);
    }
}

void CCanvasWnd::NotifySelectionSize()
{
    CSize size(-1, -1);
    if (m_dragMode == DragMode::ResizeCanvas)
        size = m_resizeSize;
    else if (m_dragMode == DragMode::TextRect)
        size = m_textDragRect.Size();
    else if (m_dragMode == DragMode::FreeSelect)
        size = CSize(-1, -1);
    else if (IsShapeActive())
        size = m_shape.Bounds().Size() + CSize(1, 1);
    else if (m_hasSelection || m_dragMode == DragMode::SelectRect)
        size = m_selRect.Size();
    else if (m_textActive)
        size = m_textRect.Size();

    const LPARAM value = size.cx < 0 ? -1 : MAKELPARAM(size.cx, size.cy);
    if (value != m_lastSelSizeNotify)
    {
        m_lastSelSizeNotify = value;
        NotifyParent(CN_SELSIZE, value);
    }
}

// ---------------------------------------------------------------------------
// 좌표 변환
// ---------------------------------------------------------------------------

CPoint CCanvasWnd::ImageOrigin() const
{
    return CPoint(RulerSize() + kMargin - m_scroll.x, RulerSize() + kMargin - m_scroll.y);
}

int CCanvasWnd::Scale(int v) const
{
    return static_cast<int>(std::floor(v * m_zoom + 1e-9));
}

CPoint CCanvasWnd::ImageToClient(CPoint pt) const
{
    const CPoint org = ImageOrigin();
    return CPoint(org.x + Scale(pt.x), org.y + Scale(pt.y));
}

CRect CCanvasWnd::ImageToClient(const CRect& rc) const
{
    return CRect(ImageToClient(rc.TopLeft()), ImageToClient(rc.BottomRight()));
}

CPoint CCanvasWnd::ClientToImage(CPoint pt) const
{
    const CPoint org = ImageOrigin();
    return CPoint(static_cast<int>(std::floor((pt.x - org.x) / m_zoom)),
                  static_cast<int>(std::floor((pt.y - org.y) / m_zoom)));
}

// ---------------------------------------------------------------------------
// 스크롤과 확대/축소
// ---------------------------------------------------------------------------

CSize CCanvasWnd::ContentSize() const
{
    const CSize size = GetImageSize();
    return CSize(std::max(1, Scale(size.cx)) + kMargin * 2 + kShadow + RulerSize(),
                 std::max(1, Scale(size.cy)) + kMargin * 2 + kShadow + RulerSize());
}

void CCanvasWnd::UpdateScrollBars()
{
    if (m_updatingScroll || !GetSafeHwnd())
        return;
    m_updatingScroll = true;

    const CSize content = ContentSize();

    // 스크롤바가 생기거나 사라지면 클라이언트 영역이 바뀌므로 두 번 계산합니다.
    for (int pass = 0; pass < 2; ++pass)
    {
        CRect rc;
        GetClientRect(&rc);
        m_scroll.x = std::clamp<int>(m_scroll.x, 0, std::max<int>(0, content.cx - rc.Width()));
        m_scroll.y = std::clamp<int>(m_scroll.y, 0, std::max<int>(0, content.cy - rc.Height()));

        SCROLLINFO si{ sizeof(SCROLLINFO) };
        si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        si.nMin = 0;
        si.nMax = content.cx - 1;
        si.nPage = static_cast<UINT>(rc.Width());
        si.nPos = m_scroll.x;
        SetScrollInfo(SB_HORZ, &si, TRUE);

        si.nMax = content.cy - 1;
        si.nPage = static_cast<UINT>(rc.Height());
        si.nPos = m_scroll.y;
        SetScrollInfo(SB_VERT, &si, TRUE);
    }

    m_updatingScroll = false;
}

void CCanvasWnd::ScrollTo(int x, int y)
{
    CRect rc;
    GetClientRect(&rc);
    const CSize content = ContentSize();
    x = std::clamp<int>(x, 0, std::max<int>(0, content.cx - rc.Width()));
    y = std::clamp<int>(y, 0, std::max<int>(0, content.cy - rc.Height()));
    if (x == m_scroll.x && y == m_scroll.y)
        return;

    m_scroll = CPoint(x, y);
    SetScrollPos(SB_HORZ, x, TRUE);
    SetScrollPos(SB_VERT, y, TRUE);
    UpdateTextEditPlacement();
    Invalidate(FALSE);
}

void CCanvasWnd::HandleScroll(int bar, UINT code)
{
    SCROLLINFO si{ sizeof(SCROLLINFO) };
    si.fMask = SIF_ALL;
    GetScrollInfo(bar, &si);

    int pos = si.nPos;
    switch (code)
    {
    case SB_LINEUP:        pos -= kScrollLine; break;
    case SB_LINEDOWN:      pos += kScrollLine; break;
    case SB_PAGEUP:        pos -= static_cast<int>(si.nPage); break;
    case SB_PAGEDOWN:      pos += static_cast<int>(si.nPage); break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION: pos = si.nTrackPos; break; // 32비트 위치
    case SB_TOP:           pos = si.nMin; break;
    case SB_BOTTOM:        pos = si.nMax; break;
    default:               return;
    }

    if (bar == SB_HORZ)
        ScrollTo(pos, m_scroll.y);
    else
        ScrollTo(m_scroll.x, pos);
}

void CCanvasWnd::SetZoomAt(double zoom, CPoint anchorClient)
{
    zoom = std::clamp(zoom, kZoomLevels[0], kZoomLevels[_countof(kZoomLevels) - 1]);
    if (std::abs(zoom - m_zoom) < 1e-9)
        return;

    // 기준점 아래의 이미지 위치가 확대 후에도 같은 화면 위치에 오도록 스크롤을 맞춥니다.
    const CPoint org = ImageOrigin();
    const double ix = (anchorClient.x - org.x) / m_zoom;
    const double iy = (anchorClient.y - org.y) / m_zoom;

    m_zoom = zoom;
    UpdateScrollBars();
    m_scroll = CPoint(-100000, -100000); // ScrollTo가 항상 갱신하도록
    ScrollTo(static_cast<int>(std::lround(ix * zoom)) + kMargin + RulerSize() - anchorClient.x,
             static_cast<int>(std::lround(iy * zoom)) + kMargin + RulerSize() - anchorClient.y);
    UpdateTextEditPlacement();
    Invalidate(FALSE);
    NotifyParent(CN_ZOOM);
}

void CCanvasWnd::SetZoom(double zoom)
{
    CRect rc;
    GetClientRect(&rc);
    SetZoomAt(zoom, rc.CenterPoint());
}

void CCanvasWnd::ZoomIn()
{
    for (double level : kZoomLevels)
    {
        if (level > m_zoom + 1e-9)
        {
            SetZoom(level);
            return;
        }
    }
}

int CCanvasWnd::ZoomLevelCount()
{
    return static_cast<int>(_countof(kZoomLevels));
}

double CCanvasWnd::ZoomLevelAt(int index)
{
    return kZoomLevels[std::clamp(index, 0, ZoomLevelCount() - 1)];
}

int CCanvasWnd::NearestZoomLevel(double zoom)
{
    int best = 0;
    for (int i = 1; i < ZoomLevelCount(); ++i)
    {
        if (std::abs(kZoomLevels[i] - zoom) < std::abs(kZoomLevels[best] - zoom))
            best = i;
    }
    return best;
}

void CCanvasWnd::ZoomOut()
{
    for (int i = _countof(kZoomLevels) - 1; i >= 0; --i)
    {
        if (kZoomLevels[i] < m_zoom - 1e-9)
        {
            SetZoom(kZoomLevels[i]);
            return;
        }
    }
}

void CCanvasWnd::OnSize(UINT nType, int cx, int cy)
{
    CWnd::OnSize(nType, cx, cy);
    UpdateScrollBars();
    UpdateTextEditPlacement();
    Invalidate(FALSE);
}

void CCanvasWnd::OnHScroll(UINT nSBCode, UINT /*nPos*/, CScrollBar* /*pScrollBar*/)
{
    HandleScroll(SB_HORZ, nSBCode);
}

void CCanvasWnd::OnVScroll(UINT nSBCode, UINT /*nPos*/, CScrollBar* /*pScrollBar*/)
{
    HandleScroll(SB_VERT, nSBCode);
}

BOOL CCanvasWnd::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    if (nFlags & MK_CONTROL) // Ctrl + 휠: 마우스 위치를 기준으로 확대/축소
    {
        ScreenToClient(&pt);
        double target = m_zoom;
        if (zDelta > 0)
        {
            for (double level : kZoomLevels)
                if (level > m_zoom + 1e-9) { target = level; break; }
        }
        else
        {
            for (int i = _countof(kZoomLevels) - 1; i >= 0; --i)
                if (kZoomLevels[i] < m_zoom - 1e-9) { target = kZoomLevels[i]; break; }
        }
        SetZoomAt(target, pt);
        return TRUE;
    }

    UINT lines = 3;
    SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &lines, 0);
    const int amount = -MulDiv(zDelta, static_cast<int>(lines) * kScrollLine, WHEEL_DELTA);

    if (nFlags & MK_SHIFT) // Shift + 휠: 가로 스크롤
        ScrollTo(m_scroll.x + amount, m_scroll.y);
    else
        ScrollTo(m_scroll.x, m_scroll.y + amount);
    return TRUE;
}

void CCanvasWnd::OnMouseHWheel(UINT /*nFlags*/, short zDelta, CPoint /*pt*/)
{
    ScrollTo(m_scroll.x + MulDiv(zDelta, 3 * kScrollLine, WHEEL_DELTA), m_scroll.y);
}

// ---------------------------------------------------------------------------
// 화면 출력 (더블 버퍼링)
// ---------------------------------------------------------------------------

BOOL CCanvasWnd::OnEraseBkgnd(CDC* /*pDC*/)
{
    return TRUE; // OnPaint에서 전부 그리므로 배경 지우기를 생략 (깜박임 방지)
}

void CCanvasWnd::DrawImageScaled(CDC& dc, CImage& image, const CRect& imageRect, const CRect& clip, bool alpha) const
{
    // imageRect: image가 놓일 위치 (이미지 좌표). 화면에 보이는 부분만 늘려 그립니다.
    const CRect dest = ImageToClient(imageRect);
    CRect visible;
    if (!visible.IntersectRect(dest, clip))
        return;

    // 보이는 화면 영역 → 원본 픽셀 범위 (경계 픽셀이 잘리지 않게 한 칸 넓힘)
    const CPoint a = ClientToImage(visible.TopLeft()) - imageRect.TopLeft();
    const CPoint b = ClientToImage(visible.BottomRight() - CSize(1, 1)) - imageRect.TopLeft();
    CRect src(a.x, a.y, b.x + 1, b.y + 1);
    src.IntersectRect(src, CRect(0, 0, image.GetWidth(), image.GetHeight()));
    if (src.IsRectEmpty())
        return;

    CRect srcInImage = src;
    srcInImage.OffsetRect(imageRect.TopLeft());
    const CRect dst = ImageToClient(srcInImage);

    if (alpha)
    {
        // 떠 있는 선택 영역: 선택 밖(알파 0)은 비치게 합성
        BLENDFUNCTION bf{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        HDC hSrc = image.GetDC();
        ::GdiAlphaBlend(dc.GetSafeHdc(), dst.left, dst.top, dst.Width(), dst.Height(),
                        hSrc, src.left, src.top, src.Width(), src.Height(), bf);
        image.ReleaseDC();
        return;
    }

    if (m_zoom < 1.0)
    {
        dc.SetStretchBltMode(HALFTONE); // 축소할 때는 평균을 내서 부드럽게
        dc.SetBrushOrg(0, 0);
    }
    else
    {
        dc.SetStretchBltMode(COLORONCOLOR); // 확대할 때는 픽셀이 또렷하게
    }
    image.StretchBlt(dc.GetSafeHdc(), dst, src, SRCCOPY);
}

void CCanvasWnd::OnPaint()
{
    CPaintDC dc(this);

    CRect client;
    GetClientRect(&client);
    if (client.IsRectEmpty())
        return;

    CDC mem;
    mem.CreateCompatibleDC(&dc);
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(&dc, client.Width(), client.Height());
    CBitmap* pOldBmp = mem.SelectObject(&bmp);

    mem.FillSolidRect(client, kWorkspaceColor);

    if (!m_image.IsNull())
    {
        const CRect imgClient = ImageToClient(ImageBounds());
        CRect shadow = imgClient;
        shadow.OffsetRect(kShadow, kShadow);
        mem.FillSolidRect(shadow, kShadowColor);

        // 도형을 그리거나 편집하는 중에는 도형이 그려진 미리 보기 이미지를 보여 줍니다.
        CImage& base = (IsShapeActive() && !m_preview.IsNull()) ? m_preview : m_image;
        DrawImageScaled(mem, base, ImageBounds(), client, false);

        if (m_floating && !m_floatImage.IsNull())
        {
            CRect clip;
            clip.IntersectRect(client, imgClient);
            DrawImageScaled(mem, m_floatImage, m_selRect, clip, true);
        }

        if (m_showGrid)
            DrawGrid(mem, client);
        DrawOverlays(mem);
    }
    if (m_showRulers)
        DrawRulers(mem);

    dc.BitBlt(0, 0, client.Width(), client.Height(), &mem, 0, 0, SRCCOPY);
    mem.SelectObject(pOldBmp);
}

void CCanvasWnd::DrawHandleBox(CDC& dc, const CRect& rc) const
{
    CBrush frame(RGB(80, 80, 80));
    dc.FillSolidRect(rc, RGB(255, 255, 255));
    dc.FrameRect(rc, &frame);
}

void CCanvasWnd::DrawOverlays(CDC& dc) const
{
    // 지우개 위치
    if (m_tool == ToolType::Eraser && m_hoverValid && m_dragMode != DragMode::ResizeCanvas)
    {
        CBrush black(RGB(0, 0, 0));
        CRect rc = ImageToClient(EraserRect(m_hoverPt));
        if (rc.Width() < 2) rc.right = rc.left + 2;
        if (rc.Height() < 2) rc.bottom = rc.top + 2;
        dc.FrameRect(rc, &black);
    }

    // 자유형 선택 드래그 중인 경로
    if (m_dragMode == DragMode::FreeSelect && m_lasso.size() >= 2)
    {
        std::vector<CPoint> pts;
        for (const CPoint& p : m_lasso)
            pts.push_back(ImageToClient(p));
        DrawDashedPolyline(dc, pts, false);
    }

    // 선택 영역 테두리
    if (m_hasSelection || m_dragMode == DragMode::SelectRect)
    {
        if (m_selFreeform && m_hasSelection)
        {
            for (const std::vector<CPoint>& ring : m_selRings)
            {
                std::vector<CPoint> pts;
                for (const CPoint& p : ring)
                    pts.push_back(ImageToClient(p + m_selRect.TopLeft()));
                DrawDashedPolyline(dc, pts, true);
            }
        }
        else
        {
            CRect rc = ImageToClient(m_selRect);
            rc.InflateRect(1, 1);
            DrawDashedRect(dc, rc);
        }
    }

    // 선택 영역 크기 핸들
    for (const CRect& rc : SelectionHandleRects())
        DrawHandleBox(dc, rc);

    // 돋보기: 클릭하면 확대될 영역
    if (m_tool == ToolType::Magnifier && m_hoverValid && m_dragMode == DragMode::None)
    {
        const int next = NearestZoomLevel(m_zoom) + 1;
        if (next < ZoomLevelCount())
        {
            // 마우스 아래 점을 고정하고 확대하므로, 화면 전체를 마우스 위치 기준으로 줄인 사각형
            CRect client;
            GetClientRect(&client);
            const double ratio = m_zoom / ZoomLevelAt(next);
            const CPoint m = ImageToClient(m_hoverPt);
            const CRect area(CPoint(static_cast<int>(m.x * (1 - ratio)), static_cast<int>(m.y * (1 - ratio))),
                             CSize(static_cast<int>(client.Width() * ratio), static_cast<int>(client.Height() * ratio)));
            DrawDashedRect(dc, area);
        }
    }

    // 편집 중인 도형: 점선 테두리와 핸들
    if (m_shapeStage == ShapeStage::Editing)
    {
        if (m_shape.kind != ShapeKind::Line)
        {
            CRect rc = m_shape.Bounds();
            rc.right += 1;
            rc.bottom += 1;
            CRect rcClient = ImageToClient(rc);
            rcClient.InflateRect(1, 1);
            DrawDashedRect(dc, rcClient);
        }
        for (const CRect& rc : ShapeHandleRects())
            DrawHandleBox(dc, rc);
    }

    // 글상자 영역 지정 중 / 입력 중
    if (m_dragMode == DragMode::TextRect)
    {
        DrawDashedRect(dc, ImageToClient(m_textDragRect));
    }
    else if (m_textActive)
    {
        CRect rc = ImageToClient(m_textRect);
        rc.InflateRect(1, 1);
        DrawDashedRect(dc, rc);
    }

    // 캔버스 크기 핸들 / 새 크기 미리 보기
    if (m_dragMode == DragMode::ResizeCanvas)
    {
        CRect rc(ImageOrigin(), CSize(Scale(m_resizeSize.cx), Scale(m_resizeSize.cy)));
        DrawDashedRect(dc, rc);
    }
    else
    {
        for (Handle h : { HandleRight, HandleBottom, HandleCorner })
            DrawHandleBox(dc, HandleRect(h));
    }
}

void CCanvasWnd::InvalidateImageRect(CRect rcImage)
{
    CRect rc = ImageToClient(rcImage);
    rc.InflateRect(2, 2);
    InvalidateRect(rc, FALSE);
}

// ---------------------------------------------------------------------------
// 캔버스 크기 핸들
// ---------------------------------------------------------------------------

CRect CCanvasWnd::HandleRect(Handle handle) const
{
    const CRect img = ImageToClient(ImageBounds());
    const int midX = (img.left + img.right) / 2 - kHandle / 2;
    const int midY = (img.top + img.bottom) / 2 - kHandle / 2;
    switch (handle)
    {
    case HandleRight:  return CRect(CPoint(img.right + 1, midY), CSize(kHandle, kHandle));
    case HandleBottom: return CRect(CPoint(midX, img.bottom + 1), CSize(kHandle, kHandle));
    case HandleCorner: return CRect(CPoint(img.right + 1, img.bottom + 1), CSize(kHandle, kHandle));
    default:           return CRect();
    }
}

CCanvasWnd::Handle CCanvasWnd::HitTestHandle(CPoint clientPt) const
{
    if (m_image.IsNull())
        return HandleNone;
    for (Handle h : { HandleCorner, HandleRight, HandleBottom })
    {
        CRect rc = HandleRect(h);
        rc.InflateRect(2, 2); // 조금 넉넉하게 잡히도록
        if (rc.PtInRect(clientPt))
            return h;
    }
    return HandleNone;
}

CSize CCanvasWnd::ResizeTargetSize(CPoint clientPt) const
{
    const CPoint org = ImageOrigin();
    CSize size = GetImageSize();
    if (m_resizeHandle == HandleRight || m_resizeHandle == HandleCorner)
        size.cx = static_cast<int>(std::lround((clientPt.x - org.x) / m_zoom));
    if (m_resizeHandle == HandleBottom || m_resizeHandle == HandleCorner)
        size.cy = static_cast<int>(std::lround((clientPt.y - org.y) / m_zoom));
    size.cx = std::clamp<int>(size.cx, 1, kMaxImageSide);
    size.cy = std::clamp<int>(size.cy, 1, kMaxImageSide);
    return size;
}

void CCanvasWnd::ResizeCanvasTo(CSize size, bool recordHistory)
{
    if (size == GetImageSize())
        return;

    CImage resized;
    if (!CreateDib32(resized, size.cx, size.cy, m_color2)) // 늘어난 부분은 색 2
        return;
    if (recordHistory)
        m_history.Push(CaptureSnapshot());

    HDC hdc = resized.GetDC();
    m_image.BitBlt(hdc, 0, 0); // 줄어들면 오른쪽/아래가 잘림
    resized.ReleaseDC();
    ReplaceImage(m_image, resized);

    UpdateScrollBars();
    Invalidate(FALSE);
    MarkModified();
}

// ---------------------------------------------------------------------------
// 마우스 분배
// ---------------------------------------------------------------------------

void CCanvasWnd::BeginDrag(DragMode mode, DragButton button, CPoint imgPt)
{
    m_dragMode = mode;
    m_dragButton = button;
    m_dragStart = m_dragLast = imgPt;
    SetCapture();
}

void CCanvasWnd::OnButtonDown(DragButton button, UINT nFlags, CPoint point)
{
    SetFocus();
    if (m_image.IsNull())
        return;

    if (m_dragMode != DragMode::None)
    {
        // 드래그 도중 반대쪽 버튼을 누르면 취소 (Windows 그림판과 같은 동작)
        if (button != m_dragButton)
            CancelDrag();
        return;
    }

    // 글상자 입력 중에 바깥을 누르면 확정만 하고 끝냅니다.
    if (m_textActive)
    {
        CommitText();
        return;
    }

    if (IsInRuler(point))
        return; // 눈금자 위 클릭은 무시

    const CPoint img = ClientToImage(point);

    // 선택 영역 크기 핸들 (캔버스 크기 핸들과 겹치면 선택 영역 쪽이 먼저)
    if (button == DragButton::Left)
    {
        const int selHandle = HitTestSelectionHandle(point);
        if (selHandle >= 0)
        {
            BeginSelectionResize(selHandle, img);
            NotifySelectionSize();
            return;
        }
    }

    // 캔버스 크기 핸들 (편집 중인 도형의 핸들이 겹치면 도형 쪽이 먼저)
    if (button == DragButton::Left && HitTestShapeHandle(point) < 0)
    {
        const Handle handle = HitTestHandle(point);
        if (handle != HandleNone)
        {
            FinishPendingWork();
            m_resizeHandle = handle;
            m_resizeSize = GetImageSize();
            BeginDrag(DragMode::ResizeCanvas, button, img);
            NotifySelectionSize();
            Invalidate(FALSE);
            return;
        }
    }

    switch (m_tool)
    {
    case ToolType::Pencil:
    case ToolType::Eraser:
    case ToolType::Brush:
        BeginFreehand(button, img);
        break;
    case ToolType::Shape:
        ShapeButtonDown(button, nFlags, img, point);
        break;
    case ToolType::Fill:
        DoFill(img, button == DragButton::Right ? m_color2 : m_color1);
        break;
    case ToolType::Picker:
        DoPick(img, button);
        break;
    case ToolType::Magnifier:
    {
        // 왼쪽 클릭: 한 단계 확대, 오른쪽 클릭: 한 단계 축소 (클릭한 점 기준)
        const int level = NearestZoomLevel(m_zoom) + (button == DragButton::Left ? 1 : -1);
        if (level >= 0 && level < ZoomLevelCount())
            SetZoomAt(ZoomLevelAt(level), point);
        break;
    }
    case ToolType::Text:
        if (button == DragButton::Left && IsInImage(img))
        {
            m_textDragRect = CRect(img, img);
            BeginDrag(DragMode::TextRect, button, img);
        }
        break;
    case ToolType::Select:
    case ToolType::FreeSelect:
        if (button != DragButton::Left)
            break;
        if (m_hasSelection && m_selRect.PtInRect(img))
        {
            BeginMoveSelection(img, (nFlags & MK_CONTROL) != 0);
        }
        else
        {
            DropSelection();
            if (m_tool == ToolType::Select)
            {
                m_selFreeform = false;
                m_selRect = SelectionFromDrag(img, img);
                BeginDrag(DragMode::SelectRect, button, img);
            }
            else
            {
                m_lasso.assign(1, img);
                BeginDrag(DragMode::FreeSelect, button, img);
            }
            Invalidate(FALSE);
        }
        break;
    }
    NotifySelectionSize();
}

void CCanvasWnd::OnButtonUp(DragButton button, UINT nFlags, CPoint point)
{
    if (m_dragMode != DragMode::None && m_dragButton == button)
        EndDrag(ClientToImage(point), nFlags);
}

void CCanvasWnd::EndDrag(CPoint imgPt, UINT nFlags)
{
    const DragMode mode = m_dragMode;
    const bool shift = (nFlags & MK_SHIFT) != 0; // 미리 보기와 같은 기준

    // 끝점 반영 (마지막 이동 메시지와 위치가 다를 수 있음)
    switch (mode)
    {
    case DragMode::Freehand:
        ContinueFreehand(imgPt);
        break;
    case DragMode::ShapeDraw:
    case DragMode::ShapeMove:
    case DragMode::ShapeHandle:
        ShapeDragMove(imgPt, shift);
        break;
    case DragMode::SelectRect:
        m_selRect = SelectionFromDrag(m_dragStart, imgPt);
        break;
    case DragMode::SelectionHandle:
        UpdateSelectionResize(imgPt);
        break;
    default:
        break;
    }

    // ReleaseCapture → OnCaptureChanged에서 취소되지 않도록 먼저 바꿈
    m_dragMode = DragMode::None;
    if (GetCapture() == this)
        ReleaseCapture();

    KillTimer(kAirbrushTimer);
    switch (mode)
    {
    case DragMode::Freehand:
        m_history.Push(std::move(m_beforeDrag));
        m_beforeDrag = ImageSnapshot();
        MarkModified();
        break;
    case DragMode::ShapeDraw:
    case DragMode::ShapeMove:
    case DragMode::ShapeHandle:
        ShapeDragEnd(imgPt, shift);
        break;
    case DragMode::SelectRect:
        m_hasSelection = !m_selRect.IsRectEmpty();
        NotifyParent(CN_STATE);
        break;
    case DragMode::FreeSelect:
        FinishFreeSelect();
        break;
    case DragMode::ResizeCanvas:
        ResizeCanvasTo(m_resizeSize, true); // 마지막 마우스 이동에서 계산한 크기
        break;
    case DragMode::TextRect:
    {
        CRect rc(m_dragStart, imgPt);
        rc.NormalizeRect();
        if (rc.Width() < 8 || rc.Height() < 8)
            rc = CRect(m_dragStart, CSize(240, TextPixelHeight() * 2 + 4)); // 클릭만 했으면 기본 크기
        StartText(rc);
        break;
    }
    default:
        break;
    }
    m_dragButton = DragButton::None;

    Invalidate(FALSE);
    NotifySelectionSize();
}

void CCanvasWnd::CancelDrag()
{
    const DragMode mode = m_dragMode;
    if (mode == DragMode::None)
        return;

    m_dragMode = DragMode::None;
    if (GetCapture() == this)
        ReleaseCapture();
    KillTimer(kAirbrushTimer);

    switch (mode)
    {
    case DragMode::Freehand:
        RestoreSnapshot(m_beforeDrag); // 이미 그린 획을 되돌림
        break;
    case DragMode::ShapeDraw:
    case DragMode::ShapeMove:
    case DragMode::ShapeHandle:
        ShapeDragCancel();
        break;
    case DragMode::SelectRect:
        m_hasSelection = false;
        break;
    case DragMode::FreeSelect:
        m_lasso.clear();
        break;
    case DragMode::SelectionHandle:
        CancelSelectionResize();
        break;
    default:
        break; // 이동 중인 선택 영역은 그 자리에 둠
    }
    m_dragButton = DragButton::None;
    m_beforeDrag = ImageSnapshot();
    Invalidate(FALSE);
    NotifySelectionSize();
}

void CCanvasWnd::OnLButtonDown(UINT nFlags, CPoint point)
{
    OnButtonDown(DragButton::Left, nFlags, point);
}

void CCanvasWnd::OnLButtonUp(UINT nFlags, CPoint point)
{
    OnButtonUp(DragButton::Left, nFlags, point);
}

void CCanvasWnd::OnLButtonDblClk(UINT nFlags, CPoint point)
{
    // 다각형을 그리는 중이면 더블클릭으로 닫고, 그 밖에는 보통 클릭으로 처리
    if (m_shapeStage == ShapeStage::Polygon && m_dragMode == DragMode::None)
        FinishPolygon();
    else
        OnButtonDown(DragButton::Left, nFlags, point);
}

void CCanvasWnd::OnRButtonDown(UINT nFlags, CPoint point)
{
    OnButtonDown(DragButton::Right, nFlags, point);
}

void CCanvasWnd::OnRButtonUp(UINT nFlags, CPoint point)
{
    OnButtonUp(DragButton::Right, nFlags, point);
}

void CCanvasWnd::OnRButtonDblClk(UINT nFlags, CPoint point)
{
    if (m_shapeStage == ShapeStage::Polygon && m_dragMode == DragMode::None)
        FinishPolygon();
    else
        OnButtonDown(DragButton::Right, nFlags, point);
}

void CCanvasWnd::OnMouseMove(UINT nFlags, CPoint point)
{
    if (!m_trackingMouse)
    {
        TRACKMOUSEEVENT tme{ sizeof(TRACKMOUSEEVENT), TME_LEAVE, m_hWnd, 0 };
        m_trackingMouse = TrackMouseEvent(&tme) != FALSE;
    }

    const CPoint img = ClientToImage(point);
    NotifyCursor(point);

    if (m_tool == ToolType::Eraser)
    {
        if (m_hoverValid)
            InvalidateImageRect(EraserRect(m_hoverPt));
        InvalidateImageRect(EraserRect(img));
    }
    if (m_showRulers && (!m_hoverValid || m_hoverPt != img))
    {
        // 눈금자의 커서 위치 표시 갱신
        CRect rc;
        GetClientRect(&rc);
        InvalidateRect(CRect(0, 0, rc.right, RulerSize()), FALSE);
        InvalidateRect(CRect(0, 0, RulerSize(), rc.bottom), FALSE);
    }
    m_hoverPt = img;
    m_hoverValid = true;

    if (m_tool == ToolType::Magnifier)
        Invalidate(FALSE); // 확대될 영역 표시를 따라 움직임

    const bool shift = (nFlags & MK_SHIFT) != 0;
    switch (m_dragMode)
    {
    case DragMode::Freehand:
        ContinueFreehand(img);
        break;
    case DragMode::ShapeDraw:
    case DragMode::ShapeMove:
    case DragMode::ShapeHandle:
        ShapeDragMove(img, shift);
        break;
    case DragMode::SelectRect:
        m_selRect = SelectionFromDrag(m_dragStart, img);
        Invalidate(FALSE);
        break;
    case DragMode::FreeSelect:
        if (m_lasso.empty() || m_lasso.back() != img)
            m_lasso.push_back(img);
        Invalidate(FALSE);
        break;
    case DragMode::MoveSelection:
        m_selRect.MoveToXY(img.x - m_moveGrab.x, img.y - m_moveGrab.y);
        Invalidate(FALSE);
        break;
    case DragMode::SelectionHandle:
        UpdateSelectionResize(img);
        break;
    case DragMode::TextRect:
        m_textDragRect = CRect(m_dragStart, img);
        m_textDragRect.NormalizeRect();
        m_textDragRect.IntersectRect(m_textDragRect, ImageBounds());
        Invalidate(FALSE);
        break;
    case DragMode::ResizeCanvas:
        m_resizeSize = ResizeTargetSize(point);
        Invalidate(FALSE);
        break;
    default:
        return;
    }
    m_dragLast = img;
    NotifySelectionSize();
}

void CCanvasWnd::OnMouseLeave()
{
    m_trackingMouse = false;
    if (m_hoverValid && m_tool == ToolType::Eraser)
        InvalidateImageRect(EraserRect(m_hoverPt));
    m_hoverValid = false;
    m_lastCursorNotify = -2;
    NotifyParent(CN_CURSOR, -1);
}

void CCanvasWnd::OnCaptureChanged(CWnd* pWnd)
{
    // 다른 창이 마우스를 가져가면 (Alt+Tab 등) 드래그를 취소
    if (pWnd != this && m_dragMode != DragMode::None)
        CancelDrag();
    CWnd::OnCaptureChanged(pWnd);
}

BOOL CCanvasWnd::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
    if (nHitTest != HTCLIENT || pWnd != this)
        return CWnd::OnSetCursor(pWnd, nHitTest, message);

    CPoint pt;
    GetCursorPos(&pt);
    ScreenToClient(&pt);

    if (IsInRuler(pt))
    {
        ::SetCursor(::LoadCursor(nullptr, IDC_ARROW));
        return TRUE;
    }

    // 선택 영역 크기 핸들
    const int selHandle = (m_dragMode == DragMode::SelectionHandle) ? m_selHandle
                        : (m_dragMode == DragMode::None ? HitTestSelectionHandle(pt) : -1);
    if (selHandle >= 0)
    {
        ::SetCursor(::LoadCursor(nullptr, HandleCursor(selHandle)));
        return TRUE;
    }

    // 편집 중인 도형의 핸들/본체
    const int shapeHandle = (m_dragMode == DragMode::ShapeHandle) ? m_shapeHandle
                          : (m_dragMode == DragMode::None ? HitTestShapeHandle(pt) : -1);
    if (shapeHandle >= 0)
    {
        // 0 왼쪽위, 1 위, 2 오른쪽위, 3 오른쪽, 4 오른쪽아래, 5 아래, 6 왼쪽아래, 7 왼쪽, 8/9 직선 끝점
        static const LPCTSTR kCursors[] = { IDC_SIZENWSE, IDC_SIZENS, IDC_SIZENESW, IDC_SIZEWE,
                                            IDC_SIZENWSE, IDC_SIZENS, IDC_SIZENESW, IDC_SIZEWE,
                                            IDC_SIZEALL, IDC_SIZEALL };
        ::SetCursor(::LoadCursor(nullptr, kCursors[std::clamp(shapeHandle, 0, 9)]));
        return TRUE;
    }
    if (m_dragMode == DragMode::ShapeMove || (m_dragMode == DragMode::None && HitTestShapeBody(pt)))
    {
        ::SetCursor(::LoadCursor(nullptr, IDC_SIZEALL));
        return TRUE;
    }

    // 캔버스 크기 핸들
    const Handle handle = (m_dragMode == DragMode::ResizeCanvas) ? m_resizeHandle
                        : (m_dragMode == DragMode::None ? HitTestHandle(pt) : HandleNone);
    if (handle != HandleNone)
    {
        LPCTSTR id = handle == HandleRight ? IDC_SIZEWE : handle == HandleBottom ? IDC_SIZENS : IDC_SIZENWSE;
        ::SetCursor(::LoadCursor(nullptr, id));
        return TRUE;
    }

    // 선택 영역 이동
    const CPoint img = ClientToImage(pt);
    const bool selectTool = (m_tool == ToolType::Select || m_tool == ToolType::FreeSelect);
    if (m_dragMode == DragMode::MoveSelection
        || (m_dragMode == DragMode::None && selectTool && m_hasSelection && m_selRect.PtInRect(img)))
    {
        ::SetCursor(::LoadCursor(nullptr, IDC_SIZEALL));
        return TRUE;
    }

    if (IsInImage(img) || m_dragMode != DragMode::None)
    {
        switch (m_tool)
        {
        case ToolType::Eraser:
            ::SetCursor(nullptr); // 사각형 미리 보기로 위치를 보여 줌
            break;
        case ToolType::Text:
            ::SetCursor(::LoadCursor(nullptr, IDC_IBEAM));
            break;
        default:
            ::SetCursor(::LoadCursor(nullptr, IDC_CROSS));
            break;
        }
        return TRUE;
    }
    return CWnd::OnSetCursor(pWnd, nHitTest, message);
}

UINT CCanvasWnd::OnGetDlgCode()
{
    return DLGC_WANTALLKEYS; // Esc, Enter, Shift, 방향키를 대화 상자가 가로채지 않도록
}

void CCanvasWnd::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    switch (nChar)
    {
    case VK_ESCAPE:
        CancelOperation();
        return;
    case VK_RETURN:
        if (m_dragMode == DragMode::None)
        {
            if (m_shapeStage == ShapeStage::Polygon)
                FinishPolygon();
            else if (m_shapeStage == ShapeStage::CurveBend1 || m_shapeStage == ShapeStage::CurveBend2)
            {
                m_shapeStage = ShapeStage::Editing; // 휘기를 건너뛰고 편집 상태로
                Invalidate(FALSE);
            }
            else if (m_shapeStage == ShapeStage::Editing)
                CommitShape();
        }
        return;
    case VK_SHIFT:
        if (m_dragMode == DragMode::ShapeDraw || m_dragMode == DragMode::ShapeHandle)
            ShapeDragMove(m_dragLast, true);
        return;
    case VK_LEFT:  MoveSelectionBy(-1, 0); return;
    case VK_RIGHT: MoveSelectionBy(1, 0);  return;
    case VK_UP:    MoveSelectionBy(0, -1); return;
    case VK_DOWN:  MoveSelectionBy(0, 1);  return;
    default:
        break;
    }
    CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CCanvasWnd::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (nChar == VK_SHIFT && (m_dragMode == DragMode::ShapeDraw || m_dragMode == DragMode::ShapeHandle))
    {
        ShapeDragMove(m_dragLast, false);
        return;
    }
    CWnd::OnKeyUp(nChar, nRepCnt, nFlags);
}
