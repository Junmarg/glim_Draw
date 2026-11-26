/**
 * @file BrushEngine.cpp
 * @brief 질감 브러시 9종 구현
 */
#include "pch.h"
#include "BrushEngine.h"

#include <cmath>

namespace
{
    constexpr int kTile = 64;

    uint32_t ToPixel(COLORREF c)
    {
        return (static_cast<uint32_t>(GetRValue(c)) << 16) | (static_cast<uint32_t>(GetGValue(c)) << 8) | GetBValue(c);
    }

    /** @brief 두 픽셀을 a/255 비율로 섞음 (a=0이면 from, 255면 to) */
    uint32_t Lerp(uint32_t from, uint32_t to, int a)
    {
        uint32_t out = 0;
        for (int shift = 0; shift < 24; shift += 8)
        {
            const int f = static_cast<int>((from >> shift) & 0xFF);
            const int t = static_cast<int>((to >> shift) & 0xFF);
            out |= static_cast<uint32_t>(f + (t - f) * a / 255) << shift;
        }
        return out;
    }

    /** @brief 색을 밝게/어둡게 (amount: -255 ~ 255) */
    uint32_t Shade(uint32_t c, int amount)
    {
        uint32_t out = 0;
        for (int shift = 0; shift < 24; shift += 8)
        {
            const int v = std::clamp(static_cast<int>((c >> shift) & 0xFF) + amount, 0, 255);
            out |= static_cast<uint32_t>(v) << shift;
        }
        return out;
    }

    /**
     * @brief 이미지 위치마다 정해진 0~255 잡음. 위치로만 정해지므로 같은 곳은 항상 같은 값
     *        → 크레용/연필 질감이 종이 결처럼 고정됨 (획을 겹쳐도 같은 자리가 비어 있음)
     */
    int GrainAt(int x, int y)
    {
        uint32_t h = static_cast<uint32_t>(x) * 73856093u ^ static_cast<uint32_t>(y) * 19349663u;
        h ^= h >> 13;
        h *= 0x5bd1e995u;
        h ^= h >> 15;
        return static_cast<int>(h & 0xFF);
    }
}

LPCTSTR BrushName(BrushKind kind)
{
    static const LPCTSTR kNames[kBrushKindCount] = {
        _T("브러시"), _T("캘리그래피 1"), _T("캘리그래피 2"), _T("에어브러시"), _T("유화 브러시"),
        _T("크레용"), _T("마커"), _T("자연스러운 연필"), _T("수채화 브러시"),
    };
    const int i = static_cast<int>(kind);
    return (i >= 0 && i < kBrushKindCount) ? kNames[i] : _T("");
}

void CBrushStroke::Begin(BrushKind kind, COLORREF color, int width, uint32_t* pixels, int pitch,
                         int imageWidth, int imageHeight, const uint32_t* before, unsigned seed)
{
    m_kind = kind;
    m_color = ToPixel(color);
    m_width = std::max(1, width);
    m_pixels = pixels;
    m_pitch = pitch;
    m_w = imageWidth;
    m_h = imageHeight;
    m_before = before;
    m_rng.seed(seed);
    m_coverage.clear();

    // 유화: 붓털 배치와 색조를 획마다 한 번 정함 → 획을 따라 줄무늬가 이어짐
    m_bristles.clear();
    if (kind == BrushKind::Oil)
    {
        const double r = m_width / 2.0 + 1;
        const int count = std::max(6, m_width * 2);
        std::uniform_real_distribution<double> pos(-r, r);
        std::uniform_int_distribution<int> tone(-45, 35);
        while (static_cast<int>(m_bristles.size()) < count)
        {
            const double dx = pos(m_rng), dy = pos(m_rng);
            if (dx * dx + dy * dy <= r * r)
                m_bristles.push_back({ static_cast<int>(std::lround(dx)), static_cast<int>(std::lround(dy)), Shade(m_color, tone(m_rng)) });
        }
    }
}

// ---------------------------------------------------------------------------
// 픽셀 쓰기
// ---------------------------------------------------------------------------

void CBrushStroke::Put(int x, int y, uint32_t pixel)
{
    if (x >= 0 && y >= 0 && x < m_w && y < m_h)
        m_pixels[static_cast<size_t>(y) * m_pitch + x] = pixel;
}

uint8_t& CBrushStroke::CoverageAt(int x, int y)
{
    const uint32_t key = (static_cast<uint32_t>(x / kTile) << 16) | static_cast<uint32_t>(y / kTile);
    std::vector<uint8_t>& tile = m_coverage[key];
    if (tile.empty())
        tile.assign(kTile * kTile, 0);
    return tile[static_cast<size_t>(y % kTile) * kTile + (x % kTile)];
}

void CBrushStroke::Cover(int x, int y, int alpha)
{
    if (x < 0 || y < 0 || x >= m_w || y >= m_h || alpha <= 0)
        return;
    alpha = std::min(alpha, 255);
    uint8_t& cov = CoverageAt(x, y);
    if (alpha <= cov)
        return; // 이번 획에서 이미 더 진하게 칠한 곳
    cov = static_cast<uint8_t>(alpha);
    // 획을 긋기 전 픽셀을 바탕으로 섞으므로 한 획 안에서는 쌓이지 않음
    const uint32_t base = m_before[static_cast<size_t>(y) * m_w + x];
    m_pixels[static_cast<size_t>(y) * m_pitch + x] = Lerp(base, m_color, alpha);
}

// ---------------------------------------------------------------------------
// 경로 따라 찍기
// ---------------------------------------------------------------------------

double CBrushStroke::Spacing() const
{
    // 자국 사이 간격: 너무 넓으면 구슬처럼 끊기고, 너무 좁으면 느림
    switch (m_kind)
    {
    case BrushKind::Normal:     return std::max(1.0, m_width / 4.0);
    case BrushKind::Airbrush:   return std::max(2.0, m_width * 0.75);
    case BrushKind::Crayon:     return std::max(1.0, m_width / 3.0);
    case BrushKind::Watercolor: return std::max(1.0, m_width / 5.0);
    default:                    return 1.0; // 펜촉·붓털·마커·연필은 1px마다
    }
}

CRect CBrushStroke::Dab(CPoint p)
{
    CRect dirty;
    dirty.SetRectEmpty();
    Stamp(p, dirty);
    return dirty;
}

CRect CBrushStroke::Segment(CPoint from, CPoint to)
{
    CRect dirty;
    dirty.SetRectEmpty();
    const double dx = to.x - from.x, dy = to.y - from.y;
    const double length = std::sqrt(dx * dx + dy * dy);
    const double step = Spacing();
    const int count = std::max(1, static_cast<int>(std::ceil(length / step)));
    for (int i = 1; i <= count; ++i)
    {
        const double t = static_cast<double>(i) / count;
        Stamp(CPoint(static_cast<int>(std::lround(from.x + dx * t)), static_cast<int>(std::lround(from.y + dy * t))), dirty);
    }
    return dirty;
}

CRect CBrushStroke::Spray(CPoint center)
{
    CRect dirty;
    dirty.SetRectEmpty();
    StampSpray(center, dirty);
    return dirty;
}

CRect CBrushStroke::FillArea(const std::vector<uint8_t>& mask, const CRect& area)
{
    const int w = area.Width();
    for (int y = 0; y < area.Height(); ++y)
    {
        const int iy = area.top + y;
        // 유화: 몇 줄마다 색조를 바꿔 가로 붓질 자국처럼
        const uint32_t oilTone = Shade(m_color, (GrainAt(iy / 3, 17) % 70) - 40);
        for (int x = 0; x < w; ++x)
        {
            if (!mask[static_cast<size_t>(y) * w + x])
                continue;
            const int ix = area.left + x;
            const int grain = GrainAt(ix, iy);
            switch (m_kind)
            {
            case BrushKind::Crayon:        if (grain < 160) Cover(ix, iy, 235); break;
            case BrushKind::Marker:        Cover(ix, iy, 140); break;
            case BrushKind::NaturalPencil: Cover(ix, iy, 120 + grain / 2); break;
            case BrushKind::Watercolor:    Cover(ix, iy, static_cast<int>(110 * (0.8 + 0.2 * grain / 255.0))); break;
            case BrushKind::Oil:           Put(ix, iy, oilTone); break;
            default:                       Put(ix, iy, m_color); break;
            }
        }
    }
    return area;
}

void CBrushStroke::Stamp(CPoint p, CRect& dirty)
{
    switch (m_kind)
    {
    case BrushKind::Normal:       StampCircle(p, m_width / 2.0, dirty); break;
    case BrushKind::Calligraphy1: StampNib(p, true, dirty); break;
    case BrushKind::Calligraphy2: StampNib(p, false, dirty); break;
    case BrushKind::Airbrush:     StampSpray(p, dirty); break;
    case BrushKind::Oil:          StampOil(p, dirty); break;
    default:                      StampCoverage(p, dirty); break;
    }
}

// ---------------------------------------------------------------------------
// 브러시별 자국
// ---------------------------------------------------------------------------

void CBrushStroke::StampCircle(CPoint p, double radius, CRect& dirty)
{
    // 일반 브러시: 꽉 찬 원
    const int r = static_cast<int>(std::ceil(radius));
    const double r2 = std::max(0.25, radius * radius);
    for (int y = -r; y <= r; ++y)
        for (int x = -r; x <= r; ++x)
            if (x * x + y * y <= r2)
                Put(p.x + x, p.y + y, m_color);
    dirty.UnionRect(dirty, CRect(p.x - r, p.y - r, p.x + r + 1, p.y + r + 1));
}

void CBrushStroke::StampNib(CPoint p, bool slash, CRect& dirty)
{
    // 캘리그래피: 45°로 기운 납작한 펜촉. 펜촉과 같은 방향으로 움직이면 가늘고, 수직이면 굵음
    const int half = std::max(1, m_width / 2);
    for (int i = -half; i <= half; ++i)
    {
        const int x = p.x + i;
        const int y = slash ? p.y - i : p.y + i;
        Put(x, y, m_color);
        Put(x + 1, y, m_color); // 2px 두께: 대각선으로 움직일 때 구멍이 나지 않도록
    }
    dirty.UnionRect(dirty, CRect(p.x - half, p.y - half, p.x + half + 2, p.y + half + 1));
}

void CBrushStroke::StampSpray(CPoint p, CRect& dirty)
{
    // 에어브러시: 원 안에 점을 고르게 흩뿌림
    const double radius = std::max(4.0, m_width * 1.5);
    const int dots = std::max(6, static_cast<int>(radius * radius / 10));
    std::uniform_real_distribution<double> pos(-radius, radius);
    for (int i = 0; i < dots; ++i)
    {
        const double dx = pos(m_rng), dy = pos(m_rng);
        if (dx * dx + dy * dy <= radius * radius)
            Put(p.x + static_cast<int>(std::lround(dx)), p.y + static_cast<int>(std::lround(dy)), m_color);
    }
    const int r = static_cast<int>(std::ceil(radius));
    dirty.UnionRect(dirty, CRect(p.x - r, p.y - r, p.x + r + 1, p.y + r + 1));
}

void CBrushStroke::StampOil(CPoint p, CRect& dirty)
{
    // 유화: 붓털마다 정해진 위치/색조로 찍음 → 획 방향으로 색 줄무늬
    for (const Bristle& b : m_bristles)
    {
        Put(p.x + b.dx, p.y + b.dy, b.color);
        if (m_width >= 8)
            Put(p.x + b.dx + 1, p.y + b.dy, b.color);
    }
    const int r = m_width / 2 + 3;
    dirty.UnionRect(dirty, CRect(p.x - r, p.y - r, p.x + r + 1, p.y + r + 1));
}

void CBrushStroke::StampCoverage(CPoint p, CRect& dirty)
{
    int r = 0;
    switch (m_kind)
    {
    case BrushKind::Crayon:
    {
        // 크레용: 둥근 자국 중 종이 결(위치 잡음)이 낮은 곳만 칠함 → 군데군데 빔
        r = std::max(1, m_width / 2);
        for (int y = -r; y <= r; ++y)
            for (int x = -r; x <= r; ++x)
                if (x * x + y * y <= r * r && GrainAt(p.x + x, p.y + y) < 160)
                    Cover(p.x + x, p.y + y, 235);
        break;
    }
    case BrushKind::Marker:
    {
        // 마커: 납작한 사각 펜촉, 반투명
        r = std::max(1, m_width / 2);
        for (int y = -r; y <= r; ++y)
            for (int x = -r / 2; x <= r / 2; ++x)
                Cover(p.x + x, p.y + y, 140);
        break;
    }
    case BrushKind::NaturalPencil:
    {
        // 자연스러운 연필: 가늘고, 진하기가 위치마다 조금씩 다름
        r = std::max(1, m_width / 4);
        for (int y = -r; y <= r; ++y)
            for (int x = -r; x <= r; ++x)
                if (x * x + y * y <= r * r)
                    Cover(p.x + x, p.y + y, 120 + GrainAt(p.x + x, p.y + y) / 2);
        break;
    }
    case BrushKind::Watercolor:
    {
        // 수채화: 가운데가 가장 진하고 가장자리로 갈수록 옅어짐, 약간의 얼룩
        r = std::max(2, m_width / 2 + 1);
        const double rr = static_cast<double>(r) * r;
        for (int y = -r; y <= r; ++y)
            for (int x = -r; x <= r; ++x)
            {
                const double d2 = (x * x + y * y) / rr;
                if (d2 > 1.0)
                    continue;
                const double grain = 0.8 + 0.2 * GrainAt(p.x + x, p.y + y) / 255.0;
                Cover(p.x + x, p.y + y, static_cast<int>(110 * (1.0 - d2 * 0.7) * grain));
            }
        break;
    }
    default:
        break;
    }
    dirty.UnionRect(dirty, CRect(p.x - r, p.y - r, p.x + r + 1, p.y + r + 1));
}
