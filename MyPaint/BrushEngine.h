#pragma once

/**
 * @file BrushEngine.h
 * @brief 질감 브러시 9종: 붓 자국(도장)을 경로를 따라 찍어 한 획을 그립니다.
 *
 * 픽셀에 쓰는 방법은 두 가지입니다.
 * - 직접 쓰기: 불투명한 브러시 (일반, 캘리그래피, 에어브러시, 유화)
 * - 덮임 정도(coverage) 쓰기: 반투명/질감 브러시 (크레용, 마커, 자연스러운 연필, 수채화)
 *   픽셀마다 "이번 획에서 가장 진하게 덮인 정도"를 기억해 두고, 획을 긋기 전 픽셀과 브러시 색을
 *   그 정도로 섞습니다. 그래서 한 획 안에서 같은 곳을 여러 번 지나가도 진해지지 않고
 *   (실제 마커처럼), 다음 획을 겹치면 그때 진해집니다.
 */

#include <cstdint>
#include <random>
#include <unordered_map>
#include <vector>

/** @brief 브러시 종류 (목록에 보이는 순서) */
enum class BrushKind
{
    Normal,        ///< 브러시: 둥근 붓
    Calligraphy1,  ///< 캘리그래피 1: "/" 방향 납작한 펜촉
    Calligraphy2,  ///< 캘리그래피 2: "\" 방향 납작한 펜촉
    Airbrush,      ///< 에어브러시: 점을 흩뿌림 (누르고 있으면 계속)
    Oil,           ///< 유화 브러시: 붓털마다 다른 색조의 줄무늬
    Crayon,        ///< 크레용: 종이 결처럼 군데군데 비는 질감
    Marker,        ///< 마커: 반투명, 한 획 안에서는 겹쳐도 진해지지 않음
    NaturalPencil, ///< 자연스러운 연필: 가늘고 거친 반투명 선
    Watercolor,    ///< 수채화 브러시: 가장자리로 갈수록 옅어지는 반투명
    Count
};

/** @brief 브러시 종류 수 */
constexpr int kBrushKindCount = static_cast<int>(BrushKind::Count);

/** @brief 브러시 이름 */
LPCTSTR BrushName(BrushKind kind);

/**
 * @brief 브러시 한 획
 *
 * 사용 순서: Begin → Dab(누른 점) → Segment(이동할 때마다) → (에어브러시는 타이머마다 Spray) → 끝
 * 각 함수는 바뀐 영역(이미지 좌표)을 돌려주므로 그 부분만 화면을 갱신하면 됩니다.
 */
class CBrushStroke
{
public:
    /**
     * @brief 획을 시작합니다.
     * @param pixels 그릴 이미지의 첫 줄 픽셀 (32비트, 위→아래)
     * @param pitch 한 줄의 픽셀 수
     * @param before 획을 긋기 전 이미지 픽셀 (width*height, 덮임 브러시가 섞을 바탕)
     * @param seed 난수 시드 (에어브러시 점, 유화 붓털 배치)
     */
    void Begin(BrushKind kind, COLORREF color, int width, uint32_t* pixels, int pitch,
               int imageWidth, int imageHeight, const uint32_t* before, unsigned seed);

    /** @brief 누른 자리에 한 번 찍기 */
    CRect Dab(CPoint p);
    /** @brief from → to 경로를 따라 찍기 */
    CRect Segment(CPoint from, CPoint to);
    /** @brief 에어브러시: 마우스가 멈춰 있어도 계속 뿌리기 (타이머에서 호출) */
    CRect Spray(CPoint center);

    /**
     * @brief 영역을 이 브러시의 질감으로 채움 (도형 질감 채우기)
     * @param mask area 크기의 바이트 배열 (0이 아니면 채울 픽셀)
     * @param area mask가 놓인 위치 (이미지 좌표)
     */
    CRect FillArea(const std::vector<uint8_t>& mask, const CRect& area);

    /** @brief 에어브러시처럼 누르고 있는 동안 계속 칠하는 브러시인지 */
    bool NeedsTimer() const { return m_kind == BrushKind::Airbrush; }

private:
    void Stamp(CPoint p, CRect& dirty);
    void StampCircle(CPoint p, double radius, CRect& dirty);
    void StampNib(CPoint p, bool slash, CRect& dirty);
    void StampSpray(CPoint p, CRect& dirty);
    void StampOil(CPoint p, CRect& dirty);
    void StampCoverage(CPoint p, CRect& dirty);

    void Put(int x, int y, uint32_t pixel);       ///< 직접 쓰기
    void Cover(int x, int y, int alpha);          ///< 덮임 정도 쓰기 (이번 획 최댓값만 반영)
    uint8_t& CoverageAt(int x, int y);
    double Spacing() const;

    BrushKind m_kind = BrushKind::Normal;
    uint32_t m_color = 0;     ///< 브러시 색 (0x00RRGGBB)
    int m_width = 1;
    uint32_t* m_pixels = nullptr;
    int m_pitch = 0;
    int m_w = 0;
    int m_h = 0;
    const uint32_t* m_before = nullptr;
    std::mt19937 m_rng;

    /** @brief 유화 붓털 하나: 붓 중심에서의 위치와 색 */
    struct Bristle { int dx, dy; uint32_t color; };
    std::vector<Bristle> m_bristles;

    /** @brief 덮임 정도: 64×64 조각으로 필요한 곳만 할당 (큰 이미지에서도 메모리를 아낌) */
    std::unordered_map<uint32_t, std::vector<uint8_t>> m_coverage;
};
