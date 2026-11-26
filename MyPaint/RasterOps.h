#pragma once

/**
 * @file RasterOps.h
 * @brief 픽셀 배열 변환: 회전, 대칭, 크기 조정, 기울이기
 *
 * 픽셀은 32비트 0xAARRGGBB이고, 떠 있는 선택 영역처럼 알파가 있는 경우
 * 미리 곱한(premultiplied) 알파로 다룹니다. 모든 채널을 똑같이 계산하므로
 * 알파가 없는 이미지(알파 바이트 0)에도 그대로 쓸 수 있습니다.
 */

#include <cstdint>
#include <vector>

/**
 * @brief 위→아래 순서의 픽셀 배열
 */
struct PixelBuffer
{
    int width = 0;
    int height = 0;
    std::vector<uint32_t> pixels;

    PixelBuffer() = default;
    PixelBuffer(int w, int h, uint32_t fill = 0) : width(w), height(h), pixels(static_cast<size_t>(w) * h, fill) {}

    uint32_t& At(int x, int y) { return pixels[static_cast<size_t>(y) * width + x]; }
    uint32_t At(int x, int y) const { return pixels[static_cast<size_t>(y) * width + x]; }
};

/** @brief 이미지 변환 종류 */
enum class FlipRotate
{
    RotateRight,     ///< 오른쪽(시계 방향)으로 90°
    RotateLeft,      ///< 왼쪽(반시계 방향)으로 90°
    Rotate180,       ///< 180°
    FlipVertical,    ///< 상하 대칭
    FlipHorizontal,  ///< 좌우 대칭
};

/** @brief 회전/대칭 */
PixelBuffer ApplyFlipRotate(const PixelBuffer& src, FlipRotate op);

/**
 * @brief 회전/대칭에서 점 하나가 옮겨 가는 위치 (선택 영역 윤곽선 변환용)
 * @param x, y 원래 점 (픽셀 경계 좌표, 0 ~ width/height)
 * @param width, height 원래 크기
 */
void FlipRotatePoint(FlipRotate op, int width, int height, int& x, int& y);

/**
 * @brief 크기 조정. 늘릴 때는 선형 보간, 줄일 때는 겹치는 면적 평균(축소해도 깨지지 않음)
 */
PixelBuffer Resize(const PixelBuffer& src, int newWidth, int newHeight);

/**
 * @brief 기울이기. 가로로 기울인 뒤 세로로 기울입니다 (Windows 그림판과 같은 순서).
 * @param degreesX 가로 기울기 (도, -89 ~ 89). 양수면 아래쪽이 오른쪽으로 밀림
 * @param degreesY 세로 기울기 (도, -89 ~ 89). 양수면 오른쪽이 아래로 밀림
 * @param fill 새로 생긴 빈 곳의 픽셀 값
 */
PixelBuffer Skew(const PixelBuffer& src, double degreesX, double degreesY, uint32_t fill);

/** @brief Skew와 같은 변환을 점 하나에 적용 (선택 영역 윤곽선 변환용) */
void SkewPoint(int width, int height, double degreesX, double degreesY, double& x, double& y);
