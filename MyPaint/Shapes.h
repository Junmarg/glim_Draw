#pragma once

/**
 * @file Shapes.h
 * @brief 도형 23종의 정의, 모양 계산, 그리기
 *
 * 도형은 두 부류입니다.
 * - 상자 도형: 드래그한 두 점 p0, p1로 정해집니다 (직선, 타원, 사각형, 별, 화살표 ...).
 *   p0이 p1보다 오른쪽/아래에 있어도 되며, 그러면 모양이 뒤집힙니다.
 * - 점 도형: 점 목록으로 정해집니다 (곡선 = 시작·제어점 2개·끝, 다각형 = 꼭짓점들).
 */

/** @brief 도형 종류 (순서는 도형 목록에 보이는 순서) */
enum class ShapeKind
{
    Line, Curve, Ellipse, Rectangle, RoundRect, Polygon,
    Triangle, RightTriangle, Diamond, Pentagon, Hexagon,
    ArrowRight, ArrowLeft, ArrowUp, ArrowDown,
    Star4, Star5, Star6,
    CalloutRoundRect, CalloutOval, CalloutCloud,
    Heart, Lightning,
    Count
};

/** @brief 도형 종류 수 */
constexpr int kShapeKindCount = static_cast<int>(ShapeKind::Count);

/**
 * @brief 도형 윤곽선/채우기 방식 (그림판과 같은 7가지). 질감 방식은 같은 이름의 브러시로 칠합니다.
 */
enum class ShapePaint
{
    None,           ///< 없음
    Solid,          ///< 단색
    Crayon,         ///< 크레용
    Marker,         ///< 마커
    Oil,            ///< 유화
    NaturalPencil,  ///< 자연스러운 연필
    Watercolor,     ///< 수채화
    Count
};

/** @brief 윤곽선/채우기 방식 수 */
constexpr int kShapePaintCount = static_cast<int>(ShapePaint::Count);

/** @brief 방식 이름 (목록 표시용) */
LPCTSTR ShapePaintName(ShapePaint paint);

/** @brief 질감 방식인지 (크레용 ~ 수채화) */
inline bool IsTexturePaint(ShapePaint paint)
{
    return paint >= ShapePaint::Crayon && paint < ShapePaint::Count;
}

/** @brief 도형 이름 (목록 표시용) */
LPCTSTR ShapeName(ShapeKind kind);

/** @brief 점 목록으로 정해지는 도형인지 (곡선, 다각형) */
inline bool IsPointShape(ShapeKind kind)
{
    return kind == ShapeKind::Curve || kind == ShapeKind::Polygon;
}

/** @brief 채우기를 할 수 없는 도형인지 (직선, 곡선) */
inline bool IsOpenShape(ShapeKind kind)
{
    return kind == ShapeKind::Line || kind == ShapeKind::Curve;
}

/**
 * @brief 도형 하나의 모양 (이미지 좌표)
 */
struct ShapeData
{
    ShapeKind kind = ShapeKind::Line;
    CPoint p0;                   ///< 상자 도형: 드래그 시작점
    CPoint p1;                   ///< 상자 도형: 드래그 끝점
    std::vector<CPoint> points;  ///< 점 도형: 곡선은 [시작, 제어1, 제어2, 끝], 다각형은 꼭짓점

    /** @brief 도형을 감싸는 사각형 (정규화, 오른쪽/아래 끝 포함 안 함) */
    CRect Bounds() const;
    /** @brief 전체를 dx, dy만큼 옮깁니다. */
    void Offset(int dx, int dy);
};

/**
 * @brief 그리기 방식
 */
struct ShapeStyle
{
    COLORREF outlineColor = RGB(0, 0, 0);
    COLORREF fillColor = RGB(255, 255, 255);
    int width = 1;          ///< 윤곽선 굵기
    bool outline = true;    ///< 윤곽선 그리기 (직선/곡선은 항상 그림)
    bool fill = false;      ///< 안쪽 채우기 (직선/곡선은 무시)
};

/** @brief 도형을 그립니다. */
void DrawShape(CDC& dc, const ShapeData& shape, const ShapeStyle& style);

/** @brief 도형이 그려질 수 있는 영역 (선 굵기 포함) */
CRect ShapePaintBounds(const ShapeData& shape, int width);

/** @brief 목록에 보여 줄 작은 아이콘을 그립니다. */
void DrawShapeIcon(CDC& dc, ShapeKind kind, const CRect& rc, COLORREF color);
