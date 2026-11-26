/**
 * @file Shapes.cpp
 * @brief 도형 23종의 정의, 모양 계산, 그리기
 *
 * 다각형 계열 도형은 모두 [0,1]×[0,1] 단위 좌표의 다각형으로 정의해 두고,
 * 그릴 때 p0→(0,0), p1→(1,1)이 되도록 늘려서 그립니다.
 * 그래서 어떤 비율로 드래그해도, 거꾸로 드래그해도(뒤집힘) 같은 코드로 그려집니다.
 */
#include "pch.h"
#include "Shapes.h"

#include <cmath>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    struct UnitPoint { double u, v; };
    using UnitPoly = std::vector<UnitPoint>;
    using UnitShape = std::vector<UnitPoly>; // 구름 설명선처럼 조각이 여럿인 도형도 있음

    /** @brief 모든 조각을 합친 범위가 정확히 [0,1]이 되도록 늘림 */
    UnitShape Normalize(UnitShape shape)
    {
        double minU = 1e9, minV = 1e9, maxU = -1e9, maxV = -1e9;
        for (const UnitPoly& poly : shape)
            for (const UnitPoint& p : poly)
            {
                minU = std::min(minU, p.u); maxU = std::max(maxU, p.u);
                minV = std::min(minV, p.v); maxV = std::max(maxV, p.v);
            }
        for (UnitPoly& poly : shape)
            for (UnitPoint& p : poly)
            {
                p.u = (p.u - minU) / (maxU - minU);
                p.v = (p.v - minV) / (maxV - minV);
            }
        return shape;
    }

    /** @brief 정다각형 (첫 꼭짓점이 위) */
    UnitPoly Regular(int n)
    {
        UnitPoly poly;
        for (int i = 0; i < n; ++i)
        {
            const double a = -kPi / 2 + 2 * kPi * i / n;
            poly.push_back({ std::cos(a), std::sin(a) });
        }
        return poly;
    }

    /** @brief 별 (바깥 꼭짓점 n개, inner = 안쪽 반지름 비율) */
    UnitPoly Star(int n, double inner)
    {
        UnitPoly poly;
        for (int i = 0; i < n * 2; ++i)
        {
            const double r = (i % 2 == 0) ? 1.0 : inner;
            const double a = -kPi / 2 + kPi * i / n;
            poly.push_back({ r * std::cos(a), r * std::sin(a) });
        }
        return poly;
    }

    /** @brief 타원 호의 점들 (각도는 화면 좌표 기준, 아래쪽이 +) */
    void AddArc(UnitPoly& poly, double cu, double cv, double ru, double rv, double fromDeg, double toDeg, int steps)
    {
        for (int i = 0; i <= steps; ++i)
        {
            const double a = (fromDeg + (toDeg - fromDeg) * i / steps) * kPi / 180.0;
            poly.push_back({ cu + ru * std::cos(a), cv + rv * std::sin(a) });
        }
    }

    UnitShape MakeArrowRight()
    {
        return { { {0, 0.25}, {0.6, 0.25}, {0.6, 0}, {1, 0.5}, {0.6, 1}, {0.6, 0.75}, {0, 0.75} } };
    }

    /** @brief 도형 종류별 단위 모양 (다각형 계열만) */
    UnitShape MakeUnitShape(ShapeKind kind)
    {
        switch (kind)
        {
        case ShapeKind::Triangle:      return { { {0.5, 0}, {1, 1}, {0, 1} } };
        case ShapeKind::RightTriangle: return { { {0, 0}, {1, 1}, {0, 1} } };
        case ShapeKind::Diamond:       return { { {0.5, 0}, {1, 0.5}, {0.5, 1}, {0, 0.5} } };
        case ShapeKind::Pentagon:      return Normalize({ Regular(5) });
        case ShapeKind::Hexagon:       return { { {0.25, 0}, {0.75, 0}, {1, 0.5}, {0.75, 1}, {0.25, 1}, {0, 0.5} } };
        case ShapeKind::ArrowRight:    return MakeArrowRight();
        case ShapeKind::ArrowLeft:
        case ShapeKind::ArrowUp:
        case ShapeKind::ArrowDown:
        {
            UnitShape s = MakeArrowRight();
            for (UnitPoint& p : s[0])
            {
                const UnitPoint q = p;
                if (kind == ShapeKind::ArrowLeft) p = { 1 - q.u, q.v };
                else if (kind == ShapeKind::ArrowUp) p = { q.v, 1 - q.u };
                else p = { q.v, q.u };
            }
            return s;
        }
        case ShapeKind::Star4: return Normalize({ Star(4, 0.38) });
        case ShapeKind::Star5: return Normalize({ Star(5, 0.40) });
        case ShapeKind::Star6: return Normalize({ Star(6, 0.55) });
        case ShapeKind::CalloutRoundRect:
        {
            // 둥근 사각형 본체(위 75%) + 왼쪽 아래 꼬리
            const double rx = 0.08, ry = 0.1, bottom = 0.75;
            UnitPoly p;
            AddArc(p, rx, ry, rx, ry, 180, 270, 6);
            AddArc(p, 1 - rx, ry, rx, ry, 270, 360, 6);
            AddArc(p, 1 - rx, bottom - ry, rx, ry, 0, 90, 6);
            p.push_back({ 0.35, bottom });
            p.push_back({ 0.12, 1.0 });
            p.push_back({ 0.22, bottom });
            AddArc(p, rx, bottom - ry, rx, ry, 90, 180, 6);
            return Normalize({ p });
        }
        case ShapeKind::CalloutOval:
        {
            // 타원 본체 + 115°~135° 사이에서 나오는 꼬리
            UnitPoly p;
            AddArc(p, 0.5, 0.375, 0.5, 0.375, 135, 475, 48);
            p.push_back({ 0.08, 1.0 });
            return Normalize({ p });
        }
        case ShapeKind::CalloutCloud:
        {
            // 볼록한 부분이 10개인 구름 + 생각 풍선 두 개
            UnitPoly body;
            for (int i = 0; i < 120; ++i)
            {
                const double a = 2 * kPi * i / 120;
                const double r = 0.82 + 0.18 * std::abs(std::sin(5 * a));
                body.push_back({ 0.5 + 0.5 * r * std::cos(a), 0.36 + 0.36 * r * std::sin(a) });
            }
            UnitPoly bubble1, bubble2;
            AddArc(bubble1, 0.22, 0.84, 0.06, 0.06, 0, 360, 16);
            AddArc(bubble2, 0.11, 0.96, 0.035, 0.035, 0, 360, 12);
            return Normalize({ body, bubble1, bubble2 });
        }
        case ShapeKind::Heart:
        {
            UnitPoly p;
            for (int i = 0; i < 100; ++i)
            {
                const double t = 2 * kPi * i / 100;
                const double s = std::sin(t);
                p.push_back({ 16 * s * s * s,
                              -(13 * std::cos(t) - 5 * std::cos(2 * t) - 2 * std::cos(3 * t) - std::cos(4 * t)) });
            }
            return Normalize({ p });
        }
        case ShapeKind::Lightning:
            return { { {0.35, 0}, {0.68, 0}, {0.52, 0.36}, {0.78, 0.36}, {0.28, 1}, {0.42, 0.55}, {0.2, 0.55} } };
        default:
            return {};
        }
    }

    /** @brief 단위 모양 캐시 (한 번만 계산) */
    const UnitShape& UnitShapeOf(ShapeKind kind)
    {
        static const std::vector<UnitShape> cache = [] {
            std::vector<UnitShape> all;
            for (int i = 0; i < kShapeKindCount; ++i)
                all.push_back(MakeUnitShape(static_cast<ShapeKind>(i)));
            return all;
        }();
        return cache[static_cast<size_t>(kind)];
    }

    /** @brief 단위 좌표 → 이미지 좌표 (p0 → (0,0), p1 → (1,1)) */
    CPoint MapUnit(const UnitPoint& p, CPoint p0, CPoint p1)
    {
        return CPoint(static_cast<int>(std::lround(p0.x + p.u * (p1.x - p0.x))),
                      static_cast<int>(std::lround(p0.y + p.v * (p1.y - p0.y))));
    }

    /** @brief 윤곽선 펜 (굵기 1은 1픽셀 펜, 그 이상은 끝과 꺾임이 둥근 펜) */
    void CreateOutlinePen(CPen& pen, COLORREF color, int width)
    {
        if (width <= 1)
        {
            pen.CreatePen(PS_SOLID, 1, color);
        }
        else
        {
            LOGBRUSH lb{ BS_SOLID, color, 0 };
            pen.CreatePen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND | PS_JOIN_ROUND, width, &lb);
        }
    }

    /** @brief 굵은 직선 (길이가 0이면 점) */
    void DrawLine(CDC& dc, CPoint from, CPoint to, COLORREF color, int width)
    {
        if (width <= 1)
        {
            CPen pen(PS_SOLID, 1, color);
            CPen* pOld = dc.SelectObject(&pen);
            dc.MoveTo(from);
            dc.LineTo(to);
            dc.SelectObject(pOld);
            dc.SetPixelV(to, color); // LineTo는 끝점을 그리지 않음
            return;
        }
        if (from == to)
        {
            CBrush brush(color);
            CBrush* pOldBrush = dc.SelectObject(&brush);
            CGdiObject* pOldPen = dc.SelectStockObject(NULL_PEN);
            const int left = to.x - width / 2;
            const int top = to.y - width / 2;
            dc.Ellipse(left, top, left + width + 1, top + width + 1);
            dc.SelectObject(pOldPen);
            dc.SelectObject(pOldBrush);
            return;
        }
        CPen pen;
        CreateOutlinePen(pen, color, width);
        CPen* pOld = dc.SelectObject(&pen);
        dc.MoveTo(from);
        dc.LineTo(to);
        dc.SelectObject(pOld);
    }
}

// ---------------------------------------------------------------------------

LPCTSTR ShapeName(ShapeKind kind)
{
    static const LPCTSTR kNames[kShapeKindCount] = {
        _T("직선"), _T("곡선"), _T("타원"), _T("사각형"), _T("둥근 사각형"), _T("다각형"),
        _T("삼각형"), _T("직각 삼각형"), _T("마름모"), _T("오각형"), _T("육각형"),
        _T("오른쪽 화살표"), _T("왼쪽 화살표"), _T("위쪽 화살표"), _T("아래쪽 화살표"),
        _T("4각 별"), _T("5각 별"), _T("6각 별"),
        _T("둥근 사각형 설명선"), _T("타원형 설명선"), _T("구름 설명선"),
        _T("하트"), _T("번개"),
    };
    const int i = static_cast<int>(kind);
    return (i >= 0 && i < kShapeKindCount) ? kNames[i] : _T("");
}

LPCTSTR ShapePaintName(ShapePaint paint)
{
    static const LPCTSTR kNames[kShapePaintCount] = {
        _T("없음"), _T("단색"), _T("크레용"), _T("마커"), _T("유화"), _T("자연스러운 연필"), _T("수채화"),
    };
    const int i = static_cast<int>(paint);
    return (i >= 0 && i < kShapePaintCount) ? kNames[i] : _T("");
}

CRect ShapeData::Bounds() const
{
    if (IsPointShape(kind))
    {
        if (points.empty())
            return CRect();
        CRect rc(points[0], points[0]);
        for (const CPoint& p : points)
        {
            rc.left = std::min<LONG>(rc.left, p.x);
            rc.top = std::min<LONG>(rc.top, p.y);
            rc.right = std::max<LONG>(rc.right, p.x);
            rc.bottom = std::max<LONG>(rc.bottom, p.y);
        }
        return rc;
    }
    CRect rc(p0, p1);
    rc.NormalizeRect();
    return rc;
}

void ShapeData::Offset(int dx, int dy)
{
    p0.Offset(dx, dy);
    p1.Offset(dx, dy);
    for (CPoint& p : points)
        p.Offset(dx, dy);
}

CRect ShapePaintBounds(const ShapeData& shape, int width)
{
    CRect rc = shape.Bounds();
    rc.InflateRect(width + 2, width + 2);
    return rc;
}

void DrawShape(CDC& dc, const ShapeData& shape, const ShapeStyle& style)
{
    const int width = std::max(1, style.width);

    // 직선
    if (shape.kind == ShapeKind::Line)
    {
        DrawLine(dc, shape.p0, shape.p1, style.outlineColor, width);
        return;
    }

    // 곡선 (3차 베지어)
    if (shape.kind == ShapeKind::Curve)
    {
        if (shape.points.size() != 4)
            return;
        CPen pen;
        CreateOutlinePen(pen, style.outlineColor, width);
        CPen* pOld = dc.SelectObject(&pen);
        dc.PolyBezier(shape.points.data(), 4);
        dc.SelectObject(pOld);
        if (width <= 1)
            dc.SetPixelV(shape.points[3], style.outlineColor);
        return;
    }

    if (!style.outline && !style.fill)
        return;

    // 채우기 브러시와 윤곽선 펜 준비
    CBrush fillBrush;
    CGdiObject* pOldBrush = nullptr;
    if (style.fill)
    {
        fillBrush.CreateSolidBrush(style.fillColor);
        pOldBrush = dc.SelectObject(&fillBrush);
    }
    else
    {
        pOldBrush = dc.SelectStockObject(NULL_BRUSH);
    }

    const bool boxed = shape.kind == ShapeKind::Ellipse || shape.kind == ShapeKind::Rectangle
                    || shape.kind == ShapeKind::RoundRect;
    CPen pen;
    CGdiObject* pOldPen = nullptr;
    if (!style.outline)
        pOldPen = dc.SelectStockObject(NULL_PEN);
    else if (boxed)
    {
        // PS_INSIDEFRAME: 굵어져도 드래그한 사각형 안쪽에 그려짐
        pen.CreatePen(PS_INSIDEFRAME, width, style.outlineColor);
        pOldPen = dc.SelectObject(&pen);
    }
    else
    {
        CreateOutlinePen(pen, style.outlineColor, width);
        pOldPen = dc.SelectObject(&pen);
    }

    if (boxed)
    {
        CRect rc = shape.Bounds();
        rc.right += 1; // 드래그한 끝 픽셀까지 포함
        rc.bottom += 1;
        if (shape.kind == ShapeKind::Ellipse)
            dc.Ellipse(rc);
        else if (shape.kind == ShapeKind::Rectangle)
            dc.Rectangle(rc);
        else
        {
            const int r = std::max(4, static_cast<int>(std::min(rc.Width(), rc.Height())) / 4);
            dc.RoundRect(rc, CPoint(r, r));
        }
    }
    else if (shape.kind == ShapeKind::Polygon)
    {
        if (shape.points.size() >= 2)
            dc.Polygon(shape.points.data(), static_cast<int>(shape.points.size()));
    }
    else
    {
        for (const UnitPoly& unit : UnitShapeOf(shape.kind))
        {
            std::vector<CPoint> pts;
            pts.reserve(unit.size());
            for (const UnitPoint& p : unit)
                pts.push_back(MapUnit(p, shape.p0, shape.p1));
            dc.Polygon(pts.data(), static_cast<int>(pts.size()));
        }
    }

    dc.SelectObject(pOldPen);
    dc.SelectObject(pOldBrush);
}

void DrawShapeIcon(CDC& dc, ShapeKind kind, const CRect& rc, COLORREF color)
{
    ShapeData shape;
    shape.kind = kind;
    shape.p0 = rc.TopLeft();
    shape.p1 = CPoint(rc.right - 1, rc.bottom - 1);
    const int w = rc.Width(), h = rc.Height();
    if (kind == ShapeKind::Curve)
    {
        shape.points = { CPoint(rc.left, rc.bottom - 2), CPoint(rc.left + w / 3, rc.top - h / 2),
                         CPoint(rc.left + w * 2 / 3, rc.bottom + h / 2), CPoint(rc.right - 1, rc.top + 2) };
    }
    else if (kind == ShapeKind::Polygon)
    {
        shape.points = { CPoint(rc.left + w / 4, rc.top), CPoint(rc.right - 1, rc.top + h / 3),
                         CPoint(rc.left + w * 2 / 3, rc.bottom - 1), CPoint(rc.left, rc.top + h * 2 / 3) };
    }
    ShapeStyle style;
    style.outlineColor = color;
    style.width = 1;
    DrawShape(dc, shape, style);
}
