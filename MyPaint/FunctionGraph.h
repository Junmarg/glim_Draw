#pragma once

/**
 * @file FunctionGraph.h
 * @brief 다항 함수 그래프 그리기 (좌표축, 격자, 눈금, 곡선, 근·극값 표시)
 */

#include "Polynomial.h"

/**
 * @brief 그래프 그리기 설정
 */
struct GraphOptions
{
    double xMin = -5.0, xMax = 5.0;   ///< x 범위
    double yMin = -10.0, yMax = 10.0; ///< y 범위 (autoY면 그릴 때 계산한 값으로 바뀜)
    bool autoY = true;                ///< y 범위를 x 범위의 함수값에 맞춤
    bool showAxes = true;             ///< 좌표축과 눈금
    bool showGrid = true;             ///< 격자선
    bool showLabels = true;           ///< 눈금 숫자
    bool showPoints = true;           ///< 근(x절편)·극대·극소 점과 좌표
    bool showFormula = true;          ///< 왼쪽 위에 "y = ..." 수식
    bool transparent = false;         ///< 배경 투명 (켜면 그래프만 캔버스 위에 얹힘)
    COLORREF curveColor = RGB(0, 0, 0);
    COLORREF background = RGB(255, 255, 255);
    int lineWidth = 3;                ///< 곡선 굵기 (픽셀)
    CSize size{ 600, 400 };           ///< 그림 크기 (픽셀)
};

namespace graph
{
    /** @brief 그림 한 변의 최대 크기 */
    constexpr int kMaxSide = 4000;

    /**
     * @brief x 범위에서 함수값의 최소·최대에 여백을 더한 y 범위
     */
    void AutoYRange(const Polynomial& f, double xMin, double xMax, double& yMin, double& yMax);

    /**
     * @brief 그래프를 그립니다.
     * @param f 함수
     * @param options 설정. autoY면 yMin/yMax를 계산해 넣습니다.
     * @param[out] out 32비트 위→아래 DIB, 미리 곱한 알파 (배경 투명이면 그래프 밖 알파 0)
     * @return 성공하면 true
     */
    bool Render(const Polynomial& f, GraphOptions& options, CImage& out);
}
