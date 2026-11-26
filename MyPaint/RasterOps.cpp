/**
 * @file RasterOps.cpp
 * @brief 픽셀 배열 변환 구현
 */
#include "pch.h"
#include "RasterOps.h"

#include <cmath>

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    /** @brief 채널별 누적용 */
    struct Accum
    {
        double c[4] = { 0, 0, 0, 0 };
        void Add(uint32_t p, double w)
        {
            for (int i = 0; i < 4; ++i)
                c[i] += static_cast<double>((p >> (i * 8)) & 0xFF) * w;
        }
        uint32_t ToPixel(double totalWeight) const
        {
            uint32_t out = 0;
            for (int i = 0; i < 4; ++i)
            {
                const int v = static_cast<int>(std::lround(c[i] / totalWeight));
                out |= static_cast<uint32_t>(std::clamp(v, 0, 255)) << (i * 8);
            }
            return out;
        }
    };

    /**
     * @brief 한 줄(가로 또는 세로)을 다른 길이로 다시 표본화
     * @param get i번째 원본 픽셀
     * @param put i번째 결과 픽셀 쓰기
     */
    template <typename Get, typename Put>
    void ResampleLine(int srcLen, int dstLen, Get get, Put put)
    {
        if (dstLen >= srcLen)
        {
            // 늘리기: 결과 픽셀 중심이 원본에서 어디인지 구해 두 픽셀 사이를 선형 보간
            for (int i = 0; i < dstLen; ++i)
            {
                double x = (i + 0.5) * srcLen / dstLen - 0.5;
                x = std::clamp(x, 0.0, static_cast<double>(srcLen - 1));
                const int x0 = static_cast<int>(std::floor(x));
                const int x1 = std::min(x0 + 1, srcLen - 1);
                const double f = x - x0;
                Accum a;
                a.Add(get(x0), 1 - f);
                a.Add(get(x1), f);
                put(i, a.ToPixel(1.0));
            }
        }
        else
        {
            // 줄이기: 결과 픽셀 하나가 덮는 원본 구간 [start, end)의 픽셀들을 겹친 길이만큼 평균
            const double scale = static_cast<double>(srcLen) / dstLen;
            for (int i = 0; i < dstLen; ++i)
            {
                const double start = i * scale;
                const double end = (i + 1) * scale;
                Accum a;
                double total = 0;
                for (int x = static_cast<int>(std::floor(start)); x < static_cast<int>(std::ceil(end)) && x < srcLen; ++x)
                {
                    const double w = std::min(end, x + 1.0) - std::max(start, static_cast<double>(x));
                    if (w <= 0)
                        continue;
                    a.Add(get(x), w);
                    total += w;
                }
                put(i, a.ToPixel(total > 0 ? total : 1.0));
            }
        }
    }

    double TanDeg(double deg)
    {
        return std::tan(deg * kPi / 180.0);
    }

    /** @brief 가로 기울이기: 각 줄을 y * tan 만큼 밀기 (가장 가까운 픽셀) */
    PixelBuffer SkewX(const PixelBuffer& src, double degrees, uint32_t fill)
    {
        const double t = TanDeg(degrees);
        const int extra = static_cast<int>(std::ceil(std::abs(t) * src.height));
        PixelBuffer dst(src.width + extra, src.height, fill);
        const double offset = t < 0 ? -t * src.height : 0.0;
        for (int y = 0; y < src.height; ++y)
        {
            const double shift = (y + 0.5) * t + offset;
            for (int x = 0; x < dst.width; ++x)
            {
                const int sx = static_cast<int>(std::floor(x + 0.5 - shift));
                if (sx >= 0 && sx < src.width)
                    dst.At(x, y) = src.At(sx, y);
            }
        }
        return dst;
    }

    /** @brief 세로 기울이기: 각 열을 x * tan 만큼 밀기 */
    PixelBuffer SkewY(const PixelBuffer& src, double degrees, uint32_t fill)
    {
        const double t = TanDeg(degrees);
        const int extra = static_cast<int>(std::ceil(std::abs(t) * src.width));
        PixelBuffer dst(src.width, src.height + extra, fill);
        const double offset = t < 0 ? -t * src.width : 0.0;
        for (int x = 0; x < src.width; ++x)
        {
            const double shift = (x + 0.5) * t + offset;
            for (int y = 0; y < dst.height; ++y)
            {
                const int sy = static_cast<int>(std::floor(y + 0.5 - shift));
                if (sy >= 0 && sy < src.height)
                    dst.At(x, y) = src.At(x, sy);
            }
        }
        return dst;
    }
}

PixelBuffer ApplyFlipRotate(const PixelBuffer& src, FlipRotate op)
{
    const int w = src.width, h = src.height;
    const bool swap = (op == FlipRotate::RotateRight || op == FlipRotate::RotateLeft);
    PixelBuffer dst(swap ? h : w, swap ? w : h);
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            int nx = x, ny = y;
            switch (op)
            {
            case FlipRotate::RotateRight:    nx = h - 1 - y; ny = x; break;
            case FlipRotate::RotateLeft:     nx = y; ny = w - 1 - x; break;
            case FlipRotate::Rotate180:      nx = w - 1 - x; ny = h - 1 - y; break;
            case FlipRotate::FlipVertical:   ny = h - 1 - y; break;
            case FlipRotate::FlipHorizontal: nx = w - 1 - x; break;
            }
            dst.At(nx, ny) = src.At(x, y);
        }
    }
    return dst;
}

void FlipRotatePoint(FlipRotate op, int width, int height, int& x, int& y)
{
    // 픽셀 경계 좌표(0 ~ width)이므로 픽셀 변환의 (w-1-x) 대신 (w-x)를 씀
    const int ox = x, oy = y;
    switch (op)
    {
    case FlipRotate::RotateRight:    x = height - oy; y = ox; break;
    case FlipRotate::RotateLeft:     x = oy; y = width - ox; break;
    case FlipRotate::Rotate180:      x = width - ox; y = height - oy; break;
    case FlipRotate::FlipVertical:   y = height - oy; break;
    case FlipRotate::FlipHorizontal: x = width - ox; break;
    }
}

PixelBuffer Resize(const PixelBuffer& src, int newWidth, int newHeight)
{
    if (newWidth <= 0 || newHeight <= 0 || src.width <= 0 || src.height <= 0)
        return PixelBuffer();
    if (newWidth == src.width && newHeight == src.height)
        return src;

    // 가로 먼저, 그다음 세로 (분리 가능한 필터)
    PixelBuffer temp(newWidth, src.height);
    for (int y = 0; y < src.height; ++y)
    {
        ResampleLine(src.width, newWidth,
            [&](int i) { return src.At(i, y); },
            [&](int i, uint32_t p) { temp.At(i, y) = p; });
    }
    PixelBuffer dst(newWidth, newHeight);
    for (int x = 0; x < newWidth; ++x)
    {
        ResampleLine(src.height, newHeight,
            [&](int i) { return temp.At(x, i); },
            [&](int i, uint32_t p) { dst.At(x, i) = p; });
    }
    return dst;
}

PixelBuffer Skew(const PixelBuffer& src, double degreesX, double degreesY, uint32_t fill)
{
    PixelBuffer result = src;
    if (std::abs(degreesX) > 1e-9)
        result = SkewX(result, degreesX, fill);
    if (std::abs(degreesY) > 1e-9)
        result = SkewY(result, degreesY, fill);
    return result;
}

void SkewPoint(int width, int height, double degreesX, double degreesY, double& x, double& y)
{
    if (std::abs(degreesX) > 1e-9)
    {
        const double t = TanDeg(degreesX);
        x += y * t + (t < 0 ? -t * height : 0.0);
        width += static_cast<int>(std::ceil(std::abs(t) * height));
    }
    if (std::abs(degreesY) > 1e-9)
    {
        const double t = TanDeg(degreesY);
        y += x * t + (t < 0 ? -t * width : 0.0);
    }
}
