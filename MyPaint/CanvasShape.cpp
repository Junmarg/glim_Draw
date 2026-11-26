/**
 * @file CanvasShape.cpp
 * @brief 캔버스 도형 도구: 그리기, 곡선/다각형 단계, 확정 전 편집
 *
 * 단계 (ShapeStage)
 * ```
 *  None ──드래그──▶ Drawing ──놓음──▶ Editing ──바깥 클릭/Esc/Enter/도구 변경──▶ 확정(None)
 *                     │                 ▲  └ 핸들 드래그: 크기 조정, 안쪽 드래그: 이동, 색·굵기·윤곽선·채우기 변경
 *                     │(곡선)           │
 *                     ├─▶ CurveBend1 ─클릭/드래그─▶ CurveBend2 ─클릭/드래그─┘
 *                     │(다각형)         │
 *                     └─▶ Polygon ─클릭/드래그마다 꼭짓점 추가─ 시작점 클릭/더블클릭/Enter ─┘
 * ```
 * 확정하기 전까지 도형은 미리 보기 이미지(m_preview)에만 그려지고,
 * 확정할 때 m_image에 한 번 그리고 실행 취소 기록을 남깁니다.
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
    constexpr int kCloseDistance = 6;   ///< 다각형 시작점을 다시 눌렀다고 보는 거리 (화면 픽셀)
    constexpr int kLineHandleA = 8;     ///< 직선 시작점 핸들 번호
    constexpr int kLineHandleB = 9;     ///< 직선 끝점 핸들 번호

    /** @brief 점과 선분 사이 거리 */
    double DistanceToSegment(CPoint p, CPoint a, CPoint b)
    {
        const double dx = b.x - a.x, dy = b.y - a.y;
        const double len2 = dx * dx + dy * dy;
        double t = len2 > 0 ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / len2 : 0.0;
        t = std::clamp(t, 0.0, 1.0);
        const double cx = a.x + t * dx - p.x, cy = a.y + t * dy - p.y;
        return std::sqrt(cx * cx + cy * cy);
    }
}

// ---------------------------------------------------------------------------
// 설정
// ---------------------------------------------------------------------------

ShapeStyle CCanvasWnd::CurrentShapeStyle() const
{
    // 왼쪽 버튼: 윤곽선 = 색 1, 채우기 = 색 2 / 오른쪽 버튼: 반대 (Windows 그림판과 같음)
    ShapeStyle style;
    style.outlineColor = m_shapeSwapped ? m_color2 : m_color1;
    style.fillColor = m_shapeSwapped ? m_color1 : m_color2;
    style.width = GetToolWidth(ToolType::Shape, m_sizeLevel);
    style.outline = (EffectiveOutline() == ShapePaint::Solid); // 질감 방식은 RenderShape가 따로 칠함
    style.fill = (EffectiveFill() == ShapePaint::Solid);
    return style;
}

void CCanvasWnd::SetShapeKind(ShapeKind kind)
{
    FinishPendingWork();
    m_shapeKind = kind;
}

void CCanvasWnd::SetShapeOptions(ShapePaint outline, ShapePaint fill)
{
    m_shapeOutline = outline;
    m_shapeFill = fill;
    RefreshShapePreview();
}

ShapePaint CCanvasWnd::EffectiveOutline() const
{
    // 직선·곡선은 윤곽선이 전부이므로 "없음"이면 단색으로
    if (IsOpenShape(m_shape.kind) && m_shapeOutline == ShapePaint::None)
        return ShapePaint::Solid;
    return m_shapeOutline;
}

ShapePaint CCanvasWnd::EffectiveFill() const
{
    return IsOpenShape(m_shape.kind) ? ShapePaint::None : m_shapeFill;
}

int CCanvasWnd::TextureWidth() const
{
    // 질감 윤곽선은 1px로는 질감이 보이지 않으므로 굵기의 두 배(최소 4px)
    if (!IsTexturePaint(EffectiveOutline()))
        return 0;
    return std::max(4, GetToolWidth(ToolType::Shape, m_sizeLevel) * 2);
}

// ---------------------------------------------------------------------------
// 미리 보기 / 확정 / 취소
// ---------------------------------------------------------------------------

void CCanvasWnd::RefreshShapePreview()
{
    if (!IsShapeActive() || m_preview.IsNull())
        return;

    HDC hdc = m_preview.GetDC();
    CDC* pDC = CDC::FromHandle(hdc);

    // 직전에 그린 도형 자리만 원본에서 다시 복사해 지움
    CRect restore;
    if (restore.IntersectRect(m_shapeDrawnArea, ImageBounds()))
        m_image.BitBlt(hdc, restore.left, restore.top, restore.Width(), restore.Height(), restore.left, restore.top);

    const ShapeStyle style = CurrentShapeStyle();
    if (m_shapeStage == ShapeStage::Polygon || (m_shapeStage == ShapeStage::Drawing && m_shape.kind == ShapeKind::Polygon))
    {
        // 다각형을 만드는 중에는 닫지 않은 꺾은선으로 보여 줌
        ShapeData segment;
        segment.kind = ShapeKind::Line;
        for (size_t i = 1; i < m_shape.points.size(); ++i)
        {
            segment.p0 = m_shape.points[i - 1];
            segment.p1 = m_shape.points[i];
            DrawShape(*pDC, segment, style);
        }
    }
    m_preview.ReleaseDC();
    if (!(m_shapeStage == ShapeStage::Polygon || (m_shapeStage == ShapeStage::Drawing && m_shape.kind == ShapeKind::Polygon)))
        RenderShape(m_preview, static_cast<const uint32_t*>(m_image.GetPixelAddress(0, 0))); // 바탕 = 확정 이미지

    m_shapeDrawnArea = ShapePaintBounds(m_shape, std::max(style.width, TextureWidth()) + 4);
    Invalidate(FALSE);
    NotifySelectionSize();
}

void CCanvasWnd::CommitShape()
{
    if (!IsShapeActive())
        return;
    if (m_shapeStage == ShapeStage::Polygon)
    {
        FinishPolygon(); // 꼭짓점이 모자라면 취소됨
        if (!IsShapeActive())
            return;
    }

    ImageSnapshot before = CaptureSnapshot();
    RenderShape(m_image, before.pixels.data()); // 반투명 질감은 그리기 전 이미지와 섞음
    m_history.Push(std::move(before));

    m_shapeStage = ShapeStage::None;
    m_preview.Destroy();
    Invalidate(FALSE);
    NotifySelectionSize();
    MarkModified();
}

void CCanvasWnd::CancelShape()
{
    if (!IsShapeActive())
        return;
    m_shapeStage = ShapeStage::None;
    m_preview.Destroy();
    Invalidate(FALSE);
    NotifySelectionSize();
    NotifyParent(CN_STATE);
}

void CCanvasWnd::FinishPolygon()
{
    if (m_shapeStage != ShapeStage::Polygon)
        return;

    // 같은 자리를 연달아 누른 점(더블클릭 등)은 하나로
    std::vector<CPoint>& pts = m_shape.points;
    pts.erase(std::unique(pts.begin(), pts.end()), pts.end());
    while (pts.size() > 1 && pts.back() == pts.front())
        pts.pop_back();

    if (pts.size() >= 3)
    {
        m_shapeStage = ShapeStage::Editing;
        RefreshShapePreview();
    }
    else
    {
        CancelShape();
    }
}

// ---------------------------------------------------------------------------
// 마우스
// ---------------------------------------------------------------------------

void CCanvasWnd::ShapeButtonDown(DragButton button, UINT /*nFlags*/, CPoint imgPt, CPoint clientPt)
{
    switch (m_shapeStage)
    {
    case ShapeStage::Editing:
        if (button == DragButton::Left)
        {
            const int handle = HitTestShapeHandle(clientPt);
            if (handle >= 0)
            {
                m_shapeHandle = handle;
                m_shapeAtDragStart = m_shape;
                BeginDrag(DragMode::ShapeHandle, button, imgPt);
                return;
            }
            if (HitTestShapeBody(clientPt))
            {
                m_shapeAtDragStart = m_shape;
                BeginDrag(DragMode::ShapeMove, button, imgPt);
                return;
            }
        }
        CommitShape(); // 바깥을 누르면 확정하고 새 도형을 시작
        break;

    case ShapeStage::CurveBend1:
    case ShapeStage::CurveBend2:
        m_shapeAtDragStart = m_shape;
        BeginDrag(DragMode::ShapeDraw, button, imgPt);
        ShapeDragMove(imgPt, false);
        return;

    case ShapeStage::Polygon:
        m_shapeAtDragStart = m_shape;
        m_shape.points.push_back(imgPt);
        BeginDrag(DragMode::ShapeDraw, button, imgPt);
        RefreshShapePreview();
        return;

    default:
        break;
    }

    // 새 도형 시작
    if (!EnsurePreview())
        return;
    m_shapeSwapped = (button == DragButton::Right);
    m_shape = ShapeData();
    m_shape.kind = m_shapeKind;
    m_shapeSeed = ::GetTickCount(); // 질감 난수를 도형마다 고정 (미리 보기가 떨리지 않게)
    if (m_shapeKind == ShapeKind::Curve)
        m_shape.points.assign(4, imgPt);
    else if (m_shapeKind == ShapeKind::Polygon)
        m_shape.points.assign(2, imgPt);
    else
        m_shape.p0 = m_shape.p1 = imgPt;

    m_shapeStage = ShapeStage::Drawing;
    m_shapeAtDragStart = m_shape;
    BeginDrag(DragMode::ShapeDraw, button, imgPt);
    RefreshShapePreview();
}

void CCanvasWnd::ShapeDragMove(CPoint imgPt, bool shift)
{
    switch (m_dragMode)
    {
    case DragMode::ShapeDraw:
        switch (m_shapeStage)
        {
        case ShapeStage::Drawing:
            if (m_shape.kind == ShapeKind::Curve)
            {
                // 처음에는 직선: 제어점을 양 끝에 둠
                m_shape.points[3] = ConstrainPoint(m_shape.points[0], imgPt, shift, true);
                m_shape.points[1] = m_shape.points[0];
                m_shape.points[2] = m_shape.points[3];
            }
            else if (m_shape.kind == ShapeKind::Polygon)
            {
                m_shape.points[1] = ConstrainPoint(m_shape.points[0], imgPt, shift, true);
            }
            else
            {
                m_shape.p1 = ConstrainPoint(m_shape.p0, imgPt, shift, m_shape.kind == ShapeKind::Line);
            }
            break;
        case ShapeStage::CurveBend1:
            m_shape.points[1] = m_shape.points[2] = imgPt; // 첫 휘기: 두 제어점을 함께
            break;
        case ShapeStage::CurveBend2:
            m_shape.points[2] = imgPt;                     // 두 번째 휘기: 두 번째 제어점만
            break;
        case ShapeStage::Polygon:
        {
            const size_t n = m_shape.points.size();
            m_shape.points[n - 1] = ConstrainPoint(m_shape.points[n - 2], imgPt, shift, true);
            break;
        }
        default:
            break;
        }
        break;

    case DragMode::ShapeMove:
        m_shape = m_shapeAtDragStart;
        m_shape.Offset(imgPt.x - m_dragStart.x, imgPt.y - m_dragStart.y);
        break;

    case DragMode::ShapeHandle:
        ApplyShapeHandleDrag(imgPt, shift);
        break;

    default:
        return;
    }
    RefreshShapePreview();
}

void CCanvasWnd::ShapeDragEnd(CPoint /*imgPt*/, bool /*shift*/)
{
    switch (m_shapeStage)
    {
    case ShapeStage::Drawing:
        if (m_shape.kind == ShapeKind::Curve)
        {
            if (m_shape.points[0] == m_shape.points[3])
                CancelShape(); // 클릭만 했으면 그리지 않음
            else
                m_shapeStage = ShapeStage::CurveBend1;
        }
        else if (m_shape.kind == ShapeKind::Polygon)
        {
            if (m_shape.points[0] == m_shape.points[1])
                CancelShape();
            else
                m_shapeStage = ShapeStage::Polygon;
        }
        else
        {
            if (m_shape.p0 == m_shape.p1)
                CancelShape();
            else
                m_shapeStage = ShapeStage::Editing;
        }
        break;

    case ShapeStage::CurveBend1:
        m_shapeStage = ShapeStage::CurveBend2;
        break;

    case ShapeStage::CurveBend2:
        m_shapeStage = ShapeStage::Editing;
        break;

    case ShapeStage::Polygon:
    {
        // 마지막 점을 시작점 근처에 놓으면 닫기
        const std::vector<CPoint>& pts = m_shape.points;
        const CPoint a = ImageToClient(pts.back());
        const CPoint b = ImageToClient(pts.front());
        if (pts.size() >= 4 && std::abs(a.x - b.x) <= kCloseDistance && std::abs(a.y - b.y) <= kCloseDistance)
        {
            m_shape.points.pop_back();
            FinishPolygon();
        }
        break;
    }

    default:
        break; // 편집 중 이동/핸들: 그대로 편집 상태
    }

    if (IsShapeActive())
        RefreshShapePreview();
    NotifyParent(CN_STATE);
}

void CCanvasWnd::ShapeDragCancel()
{
    if (m_shapeStage == ShapeStage::Drawing)
    {
        CancelShape();
        return;
    }
    // 휘기·꼭짓점 추가·편집 드래그는 그 한 단계만 되돌림
    m_shape = m_shapeAtDragStart;
    RefreshShapePreview();
}

// ---------------------------------------------------------------------------
// 편집 핸들
// ---------------------------------------------------------------------------

std::vector<CRect> CCanvasWnd::ShapeHandleRects() const
{
    std::vector<CRect> rects;
    if (m_shapeStage != ShapeStage::Editing)
        return rects;

    auto box = [](CPoint c) { return CRect(c.x - kHandle / 2, c.y - kHandle / 2, c.x - kHandle / 2 + kHandle, c.y - kHandle / 2 + kHandle); };

    if (m_shape.kind == ShapeKind::Line)
    {
        // 직선은 양 끝점에 핸들 두 개 (픽셀 가운데)
        const int half = std::max(0, Scale(1) / 2);
        rects.push_back(box(ImageToClient(m_shape.p0) + CSize(half, half)));
        rects.push_back(box(ImageToClient(m_shape.p1) + CSize(half, half)));
        return rects;
    }

    CRect b = m_shape.Bounds();
    b.right += 1;
    b.bottom += 1;
    CRect c = ImageToClient(b);
    c.InflateRect(1, 1);
    const int mx = (c.left + c.right) / 2;
    const int my = (c.top + c.bottom) / 2;
    // 0 왼쪽위, 1 위, 2 오른쪽위, 3 오른쪽, 4 오른쪽아래, 5 아래, 6 왼쪽아래, 7 왼쪽
    for (CPoint p : { CPoint(c.left, c.top), CPoint(mx, c.top), CPoint(c.right, c.top), CPoint(c.right, my),
                      CPoint(c.right, c.bottom), CPoint(mx, c.bottom), CPoint(c.left, c.bottom), CPoint(c.left, my) })
    {
        rects.push_back(box(p));
    }
    return rects;
}

int CCanvasWnd::HitTestShapeHandle(CPoint clientPt) const
{
    const std::vector<CRect> rects = ShapeHandleRects();
    for (size_t i = 0; i < rects.size(); ++i)
    {
        CRect rc = rects[i];
        rc.InflateRect(2, 2);
        if (rc.PtInRect(clientPt))
            return m_shape.kind == ShapeKind::Line ? kLineHandleA + static_cast<int>(i) : static_cast<int>(i);
    }
    return -1;
}

bool CCanvasWnd::HitTestShapeBody(CPoint clientPt) const
{
    if (m_shapeStage != ShapeStage::Editing)
        return false;

    if (m_shape.kind == ShapeKind::Line)
    {
        const double tolerance = std::max(4.0, GetToolWidth(ToolType::Shape, m_sizeLevel) * m_zoom / 2 + 3);
        return DistanceToSegment(clientPt, ImageToClient(m_shape.p0), ImageToClient(m_shape.p1)) <= tolerance;
    }
    CRect b = m_shape.Bounds();
    b.right += 1;
    b.bottom += 1;
    CRect c = ImageToClient(b);
    c.InflateRect(3, 3);
    return c.PtInRect(clientPt) != FALSE;
}

void CCanvasWnd::ApplyShapeHandleDrag(CPoint imgPt, bool shift)
{
    ShapeData s = m_shapeAtDragStart;

    if (s.kind == ShapeKind::Line)
    {
        if (m_shapeHandle == kLineHandleA)
            s.p0 = ConstrainPoint(s.p1, imgPt, shift, true);
        else
            s.p1 = ConstrainPoint(s.p0, imgPt, shift, true);
        m_shape = s;
        return;
    }

    // 잡은 위치에서 움직인 만큼만 가장자리를 옮김 (잡는 순간 1px 튀지 않도록)
    const int dx = imgPt.x - m_dragStart.x;
    const int dy = imgPt.y - m_dragStart.y;
    const int h = m_shapeHandle;
    const bool moveLeft = (h == 0 || h == 6 || h == 7);
    const bool moveRight = (h == 2 || h == 3 || h == 4);
    const bool moveTop = (h == 0 || h == 1 || h == 2);
    const bool moveBottom = (h == 4 || h == 5 || h == 6);

    if (!IsPointShape(s.kind))
    {
        // 상자 도형: p0/p1 중 왼쪽/오른쪽(위/아래)에 있는 좌표를 옮김. 반대편을 넘어가면 뒤집힘
        LONG& left = (s.p0.x <= s.p1.x) ? s.p0.x : s.p1.x;
        LONG& right = (s.p0.x <= s.p1.x) ? s.p1.x : s.p0.x;
        LONG& top = (s.p0.y <= s.p1.y) ? s.p0.y : s.p1.y;
        LONG& bottom = (s.p0.y <= s.p1.y) ? s.p1.y : s.p0.y;
        if (moveLeft) left += dx;
        if (moveRight) right += dx;
        if (moveTop) top += dy;
        if (moveBottom) bottom += dy;
    }
    else
    {
        // 점 도형: 감싸는 사각형을 바꾸고 모든 점을 비례해서 옮김
        const CRect ob = s.Bounds();
        CRect nb = ob;
        if (moveLeft) nb.left += dx;
        if (moveRight) nb.right += dx;
        if (moveTop) nb.top += dy;
        if (moveBottom) nb.bottom += dy;
        if (nb.Width() < 1) { if (moveLeft) nb.left = nb.right - 1; else nb.right = nb.left + 1; }
        if (nb.Height() < 1) { if (moveTop) nb.top = nb.bottom - 1; else nb.bottom = nb.top + 1; }

        for (CPoint& p : s.points)
        {
            p.x = ob.Width() == 0 ? p.x + (nb.left - ob.left)
                : nb.left + static_cast<int>(std::lround((p.x - ob.left) * static_cast<double>(nb.Width()) / ob.Width()));
            p.y = ob.Height() == 0 ? p.y + (nb.top - ob.top)
                : nb.top + static_cast<int>(std::lround((p.y - ob.top) * static_cast<double>(nb.Height()) / ob.Height()));
        }
    }
    m_shape = s;
}

// ---------------------------------------------------------------------------
// 윤곽선 / 채우기 (질감 포함)
// ---------------------------------------------------------------------------
//
// 단색은 GDI로 그리고, 질감(크레용·마커·유화·자연스러운 연필·수채화)은 같은 이름의 브러시로 칠합니다.
// - 질감 채우기: 도형 안쪽을 GDI로 마스크 이미지에 칠한 뒤, 마스크 픽셀마다 브러시 질감을 적용
// - 질감 윤곽선: 도형을 GDI 경로(path)로 기록해 꺾은선으로 펴고(FlattenPath), 그 선을 따라 브러시 획을 그음
//   → 타원·둥근 사각형·곡선도 같은 방법으로 처리됨

namespace
{
    BrushKind BrushForPaint(ShapePaint paint)
    {
        switch (paint)
        {
        case ShapePaint::Crayon:        return BrushKind::Crayon;
        case ShapePaint::Marker:        return BrushKind::Marker;
        case ShapePaint::Oil:           return BrushKind::Oil;
        case ShapePaint::NaturalPencil: return BrushKind::NaturalPencil;
        case ShapePaint::Watercolor:    return BrushKind::Watercolor;
        default:                        return BrushKind::Normal;
        }
    }
}

void CCanvasWnd::RenderShape(CImage& target, const uint32_t* before)
{
    const ShapeStyle base = CurrentShapeStyle();
    const ShapePaint outline = EffectiveOutline();
    const ShapePaint fill = EffectiveFill();

    // 1) 채우기
    if (fill == ShapePaint::Solid)
    {
        ShapeStyle s = base;
        s.outline = false;
        HDC hdc = target.GetDC();
        DrawShape(*CDC::FromHandle(hdc), m_shape, s);
        target.ReleaseDC();
    }
    else if (IsTexturePaint(fill))
    {
        TextureFillShape(target, before, fill, base.fillColor);
    }

    // 2) 윤곽선 (채우기 위에)
    if (outline == ShapePaint::Solid)
    {
        ShapeStyle s = base;
        s.fill = false;
        HDC hdc = target.GetDC();
        DrawShape(*CDC::FromHandle(hdc), m_shape, s);
        target.ReleaseDC();
    }
    else if (IsTexturePaint(outline))
    {
        // 반투명 윤곽선은 "채운 뒤의 그림"과 섞어야 하므로, 채우기가 있으면 그 상태를 바탕으로 복사
        std::vector<uint32_t> afterFill;
        const uint32_t* outlineBase = before;
        if (fill != ShapePaint::None)
        {
            GdiFlush();
            const CSize size(target.GetWidth(), target.GetHeight());
            afterFill.resize(static_cast<size_t>(size.cx) * size.cy);
            for (int y = 0; y < size.cy; ++y)
                memcpy(&afterFill[static_cast<size_t>(y) * size.cx], target.GetPixelAddress(0, y), static_cast<size_t>(size.cx) * 4);
            outlineBase = afterFill.data();
        }
        TextureOutlineShape(target, outlineBase, outline, base.outlineColor);
    }
}

void CCanvasWnd::TextureFillShape(CImage& target, const uint32_t* before, ShapePaint paint, COLORREF color)
{
    CRect area = m_shape.Bounds();
    area.right += 2;
    area.bottom += 2;
    if (area.IsRectEmpty())
        return;

    // 안쪽 마스크: 도형을 흰색으로 채워 그림
    CImage maskImage;
    if (!CreateDib32(maskImage, area.Width(), area.Height(), RGB(0, 0, 0)))
        return;
    HDC hdc = maskImage.GetDC();
    CDC* pDC = CDC::FromHandle(hdc);
    // CImage::GetDC는 여러 CImage가 함께 쓰는 DC를 빌려주고, ReleaseDC할 때 DC 상태를 되돌리지 않습니다.
    // 바꾼 상태(원점, 채우기 모드 등)가 다른 이미지 그리기에 남지 않도록 SaveDC/RestoreDC로 감쌉니다.
    const int saved = pDC->SaveDC();
    pDC->SetViewportOrg(-area.left, -area.top);
    ShapeStyle s;
    s.outline = false;
    s.fill = true;
    s.fillColor = RGB(255, 255, 255);
    DrawShape(*pDC, m_shape, s);
    pDC->RestoreDC(saved);
    maskImage.ReleaseDC();
    GdiFlush();

    std::vector<uint8_t> mask(static_cast<size_t>(area.Width()) * area.Height());
    for (int y = 0; y < area.Height(); ++y)
    {
        const auto* row = static_cast<const uint32_t*>(maskImage.GetPixelAddress(0, y));
        for (int x = 0; x < area.Width(); ++x)
            mask[static_cast<size_t>(y) * area.Width() + x] = (row[x] & 0x00FFFFFF) ? 1 : 0;
    }

    CBrushStroke stroke;
    stroke.Begin(BrushForPaint(paint), color, GetToolWidth(ToolType::Shape, m_sizeLevel),
                 static_cast<uint32_t*>(target.GetPixelAddress(0, 0)), target.GetPitch() / 4,
                 target.GetWidth(), target.GetHeight(), before, m_shapeSeed);
    stroke.FillArea(mask, area);
}

void CCanvasWnd::TextureOutlineShape(CImage& target, const uint32_t* before, ShapePaint paint, COLORREF color)
{
    // 도형 윤곽을 경로로 기록 → 곡선을 꺾은선으로 폄 → 점 목록
    CDC dc;
    dc.CreateCompatibleDC(nullptr);
    dc.BeginPath();
    ShapeStyle s;
    s.outline = true;
    s.fill = false;
    s.width = 1;
    DrawShape(dc, m_shape, s);
    dc.EndPath();
    dc.FlattenPath();
    const int count = dc.GetPath(nullptr, nullptr, 0);
    if (count <= 0)
        return;
    std::vector<POINT> points(count);
    std::vector<BYTE> types(count);
    dc.GetPath(points.data(), types.data(), count);

    GdiFlush();
    CBrushStroke stroke;
    stroke.Begin(BrushForPaint(paint), color, TextureWidth(),
                 static_cast<uint32_t*>(target.GetPixelAddress(0, 0)), target.GetPitch() / 4,
                 target.GetWidth(), target.GetHeight(), before, m_shapeSeed);

    CPoint start, prev;
    for (int i = 0; i < count; ++i)
    {
        const CPoint p(points[i]);
        if ((types[i] & ~PT_CLOSEFIGURE) == PT_MOVETO)
        {
            start = prev = p;
            stroke.Dab(p);
        }
        else
        {
            stroke.Segment(prev, p);
            prev = p;
        }
        if (types[i] & PT_CLOSEFIGURE)
        {
            stroke.Segment(prev, start); // 닫힌 도형: 시작점으로 돌아감
            prev = start;
        }
    }
}
