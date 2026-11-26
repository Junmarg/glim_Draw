/**
 * @file CanvasTransform.cpp
 * @brief 캔버스 이미지 변환: 회전, 대칭, 크기 조정, 기울이기, 자르기
 *
 * 선택 영역이 있으면 선택 영역만, 없으면 이미지 전체를 변환합니다.
 * - 선택 영역: 떼어 낸(떠 있는) 그림을 변환합니다. 떼어 낼 때 실행 취소 기록이 남으므로
 *   여러 번 변환해도 기록은 한 번이고, 확정은 선택을 끝낼 때 합니다.
 * - 이미지 전체: 변환할 때마다 기록을 남기고 바로 적용합니다.
 *
 * 픽셀 계산은 RasterOps의 함수들이 하고, 여기서는 CImage ↔ PixelBuffer 변환과
 * 선택 영역 상태(위치, 자유형 윤곽선) 갱신을 맡습니다.
 */
#include "pch.h"
#include "CanvasWnd.h"
#include "CanvasInternal.h"

#include <cmath>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

using namespace canvas;

PixelBuffer CCanvasWnd::ToBuffer(const CImage& image)
{
    GdiFlush();
    PixelBuffer buffer(image.GetWidth(), image.GetHeight());
    for (int y = 0; y < buffer.height; ++y)
        memcpy(&buffer.At(0, y), image.GetPixelAddress(0, y), static_cast<size_t>(buffer.width) * 4);
    return buffer;
}

bool CCanvasWnd::FromBuffer(CImage& image, const PixelBuffer& buffer)
{
    CImage temp;
    if (!CreateDib32(temp, buffer.width, buffer.height, RGB(0, 0, 0)))
        return false;
    GdiFlush();
    for (int y = 0; y < buffer.height; ++y)
        memcpy(temp.GetPixelAddress(0, y), buffer.pixels.data() + static_cast<size_t>(y) * buffer.width, static_cast<size_t>(buffer.width) * 4);
    ReplaceImage(image, temp);
    return true;
}

// ---------------------------------------------------------------------------

void CCanvasWnd::FlipRotateImage(FlipRotate op)
{
    FinishPendingWorkExceptSelection();

    if (m_hasSelection)
    {
        LiftSelection(false);
        if (!m_floating)
            return;

        const int ow = m_floatImage.GetWidth();
        const int oh = m_floatImage.GetHeight();
        const PixelBuffer result = ApplyFlipRotate(ToBuffer(m_floatSource), op);
        if (!FromBuffer(m_floatSource, result))
            return;
        RebuildFloatImage();

        // 자유형 윤곽선도 같이 돌림
        for (std::vector<CPoint>& ring : m_selRings)
        for (CPoint& p : ring)
        {
            int x = p.x, y = p.y;
            FlipRotatePoint(op, ow, oh, x, y);
            p = CPoint(x, y);
        }
        m_selRect = CRect(m_selRect.TopLeft(), CSize(result.width, result.height));
        Invalidate(FALSE);
        NotifySelectionSize();
        MarkModified();
        return;
    }

    if (m_image.IsNull())
        return;
    ImageSnapshot before = CaptureSnapshot();
    if (!FromBuffer(m_image, ApplyFlipRotate(ToBuffer(m_image), op)))
        return;
    m_history.Push(std::move(before));
    UpdateScrollBars();
    Invalidate(FALSE);
    MarkModified();
}

CSize CCanvasWnd::GetTransformTargetSize() const
{
    return m_hasSelection ? m_selRect.Size() : GetImageSize();
}

bool CCanvasWnd::ResizeAndSkew(CSize newSize, int skewX, int skewY)
{
    FinishPendingWorkExceptSelection();
    if (m_image.IsNull() || newSize.cx <= 0 || newSize.cy <= 0)
        return false;

    // 결과 크기를 미리 계산해 너무 크면 하지 않음 (기울이면 그만큼 넓어짐)
    constexpr double kDeg = 3.14159265358979323846 / 180.0;
    const double resultW = newSize.cx + std::ceil(std::abs(std::tan(skewX * kDeg)) * newSize.cy);
    const double resultH = newSize.cy + std::ceil(std::abs(std::tan(skewY * kDeg)) * resultW);
    if (resultW > kMaxImageSide || resultH > kMaxImageSide)
        return false;

    if (m_hasSelection)
    {
        LiftSelection(false);
        if (!m_floating)
            return false;

        const int ow = m_floatImage.GetWidth();
        const int oh = m_floatImage.GetHeight();
        PixelBuffer buffer = Resize(ToBuffer(m_floatSource), newSize.cx, newSize.cy);
        buffer = Skew(buffer, skewX, skewY, 0); // 새로 생긴 곳은 투명
        if (!FromBuffer(m_floatSource, buffer))
            return false;
        RebuildFloatImage();

        for (std::vector<CPoint>& ring : m_selRings)
        for (CPoint& p : ring)
        {
            double x = p.x * static_cast<double>(newSize.cx) / ow;
            double y = p.y * static_cast<double>(newSize.cy) / oh;
            SkewPoint(newSize.cx, newSize.cy, skewX, skewY, x, y);
            p = CPoint(static_cast<int>(std::lround(x)), static_cast<int>(std::lround(y)));
        }
        m_selRect = CRect(m_selRect.TopLeft(), CSize(buffer.width, buffer.height));
        Invalidate(FALSE);
        NotifySelectionSize();
        MarkModified();
        return true;
    }

    ImageSnapshot before = CaptureSnapshot();
    PixelBuffer buffer = Resize(ToBuffer(m_image), newSize.cx, newSize.cy);
    buffer = Skew(buffer, skewX, skewY, ToPixel(m_color2)); // 새로 생긴 곳은 색 2
    if (!FromBuffer(m_image, buffer))
        return false;
    m_history.Push(std::move(before));
    UpdateScrollBars();
    Invalidate(FALSE);
    MarkModified();
    return true;
}

void CCanvasWnd::SetCanvasSize(CSize size)
{
    FinishPendingWork();
    size.cx = std::clamp<int>(size.cx, 1, kMaxImageSide);
    size.cy = std::clamp<int>(size.cy, 1, kMaxImageSide);
    ResizeCanvasTo(size, true);
}

void CCanvasWnd::CropToSelection()
{
    if (!m_hasSelection)
        return;
    FinishPendingWorkExceptSelection();

    // 떠 있으면 떠 있는 그림을, 아니면 선택한 부분을 새 이미지로
    CImage selected;
    CImage* source = &m_floatImage;
    if (!m_floating)
    {
        if (!BuildSelectionImage(selected))
            return;
        source = &selected;
    }

    CImage result;
    if (!CreateDib32(result, source->GetWidth(), source->GetHeight(), m_color2))
        return;
    GdiFlush();
    for (int y = 0; y < result.GetHeight(); ++y)
    {
        const auto* src = static_cast<const uint32_t*>(source->GetPixelAddress(0, y));
        auto* dst = static_cast<uint32_t*>(result.GetPixelAddress(0, y));
        for (int x = 0; x < result.GetWidth(); ++x)
            dst[x] = BlendOver(dst[x], src[x]); // 자유형 선택 밖은 색 2
    }

    // 떠 있었다면 떼어 낼 때 남긴 기록(떼어 내기 전 이미지)으로 충분
    if (!m_floating)
        m_history.Push(CaptureSnapshot());
    ReplaceImage(m_image, result);

    m_floatImage.Destroy();
    m_floatSource.Destroy();
    m_floating = false;
    ClearSelectionState();
    UpdateScrollBars();
    Invalidate(FALSE);
    NotifySelectionSize();
    MarkModified();
}
