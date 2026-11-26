/**
 * @file CanvasSelection.cpp
 * @brief 캔버스 선택 영역: 사각형/자유형 선택, 이동/복사, 클립보드
 *
 * 선택 영역에는 두 가지 상태가 있습니다.
 * - 고정: 이미지 위에 테두리만 있는 상태 (m_floating == false)
 * - 떠 있음: 선택한 그림을 떼어 m_floatImage로 들고 있는 상태. 이동·회전·크기 조정하거나
 *   붙여넣으면 이 상태가 됩니다. 다른 곳을 클릭하거나 도구를 바꾸면 그 자리에 찍히며(확정) 끝납니다.
 *
 * 떠 있는 그림은 미리 곱한 알파를 가집니다. 사각형 선택은 전부 불투명(알파 255)이고,
 * 자유형 선택은 윤곽선 밖이 투명(알파 0)이라 그 부분은 아래 그림이 그대로 보입니다.
 *
 * 실행 취소 기록은 떼어 낼 때(또는 붙여넣을 때) 한 번만 남기므로,
 * 이동·변환하고 확정한 뒤 실행 취소하면 떼어 내기 전 상태로 한 번에 돌아갑니다.
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
    /**
     * @brief 클립보드 열기. 클립보드 기록(Win+V) 같은 다른 프로그램이 잠깐 열고 있으면
     *        바로 실패하므로 몇 번 다시 시도합니다.
     */
    bool OpenClipboardWithRetry(HWND owner)
    {
        for (int attempt = 0; attempt < 20; ++attempt)
        {
            if (::OpenClipboard(owner))
                return true;
            ::Sleep(10);
        }
        return false;
    }
}

// ---------------------------------------------------------------------------
// 선택 영역 만들기
// ---------------------------------------------------------------------------

CRect CCanvasWnd::SelectionFromDrag(CPoint a, CPoint b) const
{
    CRect rc(a, b);
    rc.NormalizeRect();
    rc.right += 1; // 끝 픽셀까지 포함
    rc.bottom += 1;
    rc.IntersectRect(rc, ImageBounds());
    return rc;
}

void CCanvasWnd::FinishFreeSelect()
{
    std::vector<CPoint> pts;
    pts.swap(m_lasso);
    if (pts.size() < 3)
        return;

    CRect bounds(pts[0], pts[0]);
    for (const CPoint& p : pts)
    {
        bounds.left = std::min<LONG>(bounds.left, p.x);
        bounds.top = std::min<LONG>(bounds.top, p.y);
        bounds.right = std::max<LONG>(bounds.right, p.x);
        bounds.bottom = std::max<LONG>(bounds.bottom, p.y);
    }
    bounds.right += 1;
    bounds.bottom += 1;
    CRect clipped;
    if (!clipped.IntersectRect(bounds, ImageBounds()))
        return;

    m_selRect = clipped;
    m_selFreeform = true;
    std::vector<CPoint> ring;
    for (const CPoint& p : pts)
        ring.push_back(p - clipped.TopLeft()); // 선택 영역 기준 상대 좌표
    m_selRings.assign(1, ring);
    m_hasSelection = true;

    // 넓이가 없는 경로(거의 직선)는 선택하지 않음
    std::vector<uint8_t> mask;
    if (!BuildSelectionMask(mask))
        ClearSelectionState();

    Invalidate(FALSE);
    NotifySelectionSize();
    NotifyParent(CN_STATE);
}

bool CCanvasWnd::BuildSelectionMask(std::vector<uint8_t>& mask) const
{
    const int w = m_selRect.Width();
    const int h = m_selRect.Height();
    if (w <= 0 || h <= 0)
        return false;
    mask.assign(static_cast<size_t>(w) * h, 1);
    if (!m_selFreeform)
        return true;

    // 윤곽선 다각형을 검은 바탕에 흰색으로 칠해서, 칠해진 픽셀을 선택 픽셀로 씀
    CImage temp;
    if (!CreateDib32(temp, w, h, RGB(0, 0, 0)))
        return false;
    HDC hdc = temp.GetDC();
    CDC* pDC = CDC::FromHandle(hdc);
    // CImage::GetDC는 여러 CImage가 함께 쓰는 DC를 빌려주고, ReleaseDC할 때 DC 상태를 되돌리지 않습니다.
    // 바꾼 상태(원점, 채우기 모드 등)가 다른 이미지 그리기에 남지 않도록 SaveDC/RestoreDC로 감쌉니다.
    const int saved = pDC->SaveDC();
    CBrush brush(RGB(255, 255, 255));
    CBrush* pOldBrush = pDC->SelectObject(&brush);
    if (m_selRings.size() == 1)
    {
        // 자유형: 지나간 경로까지 포함해서, 경로가 스스로 겹쳐도 안쪽은 모두 선택
        CPen pen(PS_SOLID, 1, RGB(255, 255, 255));
        CPen* pOldPen = pDC->SelectObject(&pen);
        pDC->SetPolyFillMode(WINDING);
        pDC->Polygon(m_selRings[0].data(), static_cast<int>(m_selRings[0].size()));
        pDC->SelectObject(pOldPen);
    }
    else
    {
        // 반전: 바깥 테두리와 원래 윤곽을 교대 채우기 → 원래 윤곽의 바깥만 칠해짐
        std::vector<CPoint> all;
        std::vector<INT> counts;
        for (const std::vector<CPoint>& ring : m_selRings)
        {
            all.insert(all.end(), ring.begin(), ring.end());
            counts.push_back(static_cast<INT>(ring.size()));
        }
        CGdiObject* pOldPen = pDC->SelectStockObject(NULL_PEN);
        pDC->SetPolyFillMode(ALTERNATE);
        pDC->PolyPolygon(all.data(), counts.data(), static_cast<int>(counts.size()));
        pDC->SelectObject(pOldPen);
    }
    pDC->SelectObject(pOldBrush);
    pDC->RestoreDC(saved);
    temp.ReleaseDC();
    GdiFlush();

    size_t count = 0;
    for (int y = 0; y < h; ++y)
    {
        const auto* row = static_cast<const uint32_t*>(temp.GetPixelAddress(0, y));
        for (int x = 0; x < w; ++x)
        {
            const bool inside = (row[x] & 0x00FFFFFF) != 0;
            mask[static_cast<size_t>(y) * w + x] = inside ? 1 : 0;
            count += inside ? 1 : 0;
        }
    }
    return count > 0;
}

bool CCanvasWnd::BuildSelectionImage(CImage& out) const
{
    std::vector<uint8_t> mask;
    if (!BuildSelectionMask(mask))
        return false;

    const int w = m_selRect.Width();
    const int h = m_selRect.Height();
    if (!CreateDib32(out, w, h, RGB(0, 0, 0)))
        return false;

    GdiFlush();
    for (int y = 0; y < h; ++y)
    {
        const auto* src = static_cast<const uint32_t*>(m_image.GetPixelAddress(m_selRect.left, m_selRect.top + y));
        auto* dst = static_cast<uint32_t*>(out.GetPixelAddress(0, y));
        for (int x = 0; x < w; ++x)
            dst[x] = mask[static_cast<size_t>(y) * w + x] ? ((src[x] & 0x00FFFFFF) | 0xFF000000) : 0;
    }
    return true;
}

// ---------------------------------------------------------------------------
// 떼어 내기 / 찍기 / 이동
// ---------------------------------------------------------------------------

void CCanvasWnd::LiftSelection(bool copy)
{
    if (m_floating || !m_hasSelection)
        return;

    CImage image;
    if (!BuildSelectionImage(image))
        return;
    m_history.Push(CaptureSnapshot());

    if (!copy)
    {
        // 옮기면 원래 자리(선택한 픽셀만)는 배경색(색 2)으로 비움
        GdiFlush();
        const uint32_t fill = ToPixel(m_color2);
        for (int y = 0; y < m_selRect.Height(); ++y)
        {
            const auto* src = static_cast<const uint32_t*>(image.GetPixelAddress(0, y));
            auto* dst = static_cast<uint32_t*>(m_image.GetPixelAddress(m_selRect.left, m_selRect.top + y));
            for (int x = 0; x < m_selRect.Width(); ++x)
            {
                if (src[x] >> 24)
                    dst[x] = fill;
            }
        }
    }
    ReplaceImage(m_floatSource, image);
    RebuildFloatImage();
    m_floating = true;
    MarkModified();
}

void CCanvasWnd::StampFloating()
{
    if (!m_floating || m_floatImage.IsNull())
        return;

    // 알파를 고려해 합성 (자유형 선택 밖, 크기 조정으로 반투명해진 가장자리)
    GdiFlush();
    const CSize size = GetImageSize();
    for (int y = 0; y < m_floatImage.GetHeight(); ++y)
    {
        const int iy = m_selRect.top + y;
        if (iy < 0 || iy >= size.cy)
            continue;
        const auto* src = static_cast<const uint32_t*>(m_floatImage.GetPixelAddress(0, y));
        auto* row = static_cast<uint32_t*>(m_image.GetPixelAddress(0, iy));
        for (int x = 0; x < m_floatImage.GetWidth(); ++x)
        {
            const int ix = m_selRect.left + x;
            if (ix >= 0 && ix < size.cx)
                row[ix] = BlendOver(row[ix], src[x]); // 이미지 밖으로 나간 부분은 잘림
        }
    }
}

void CCanvasWnd::RebuildFloatImage()
{
    // 화면 표시·찍기용 그림 = 원본. 투명 선택이면 색 2와 같은 불투명 픽셀을 투명(0)으로
    if (m_floatSource.IsNull())
        return;
    const int w = m_floatSource.GetWidth();
    const int h = m_floatSource.GetHeight();
    if (!CreateDib32(m_floatImage, w, h, RGB(0, 0, 0)))
        return;
    GdiFlush();
    const uint32_t key = ToPixel(m_color2);
    for (int y = 0; y < h; ++y)
    {
        const auto* src = static_cast<const uint32_t*>(m_floatSource.GetPixelAddress(0, y));
        auto* dst = static_cast<uint32_t*>(m_floatImage.GetPixelAddress(0, y));
        for (int x = 0; x < w; ++x)
        {
            const uint32_t p = src[x];
            dst[x] = (m_transparentSelection && (p >> 24) == 255 && (p & 0x00FFFFFF) == key) ? 0 : p;
        }
    }
}

void CCanvasWnd::SetTransparentSelection(bool transparent)
{
    m_transparentSelection = transparent;
    if (m_floating)
    {
        RebuildFloatImage();
        Invalidate(FALSE);
    }
}

void CCanvasWnd::BeginMoveSelection(CPoint imgPt, bool copy)
{
    if (m_floating && copy)
        StampFloating(); // 떠 있는 그림을 Ctrl+드래그: 지금 자리에 하나 찍고 사본을 옮김
    else
        LiftSelection(copy);

    m_moveGrab = imgPt - m_selRect.TopLeft();
    BeginDrag(DragMode::MoveSelection, DragButton::Left, imgPt);
    Invalidate(FALSE);
}

void CCanvasWnd::ClearSelectionState()
{
    m_hasSelection = false;
    m_selFreeform = false;
    m_selRings.clear();
}

void CCanvasWnd::DropSelection()
{
    if (m_floating)
    {
        StampFloating();
        m_floatImage.Destroy();
        m_floatSource.Destroy();
        m_floating = false;
    }
    if (m_hasSelection)
    {
        ClearSelectionState();
        Invalidate(FALSE);
        NotifySelectionSize();
        NotifyParent(CN_STATE);
    }
}

void CCanvasWnd::MoveSelectionBy(int dx, int dy)
{
    if (!m_hasSelection || m_dragMode != DragMode::None)
        return;
    LiftSelection(false);
    m_selRect.OffsetRect(dx, dy);
    Invalidate(FALSE);
}

void CCanvasWnd::SelectAll()
{
    ChangeToolInternal(ToolType::Select); // 진행 중인 작업도 확정됨
    m_selFreeform = false;
    m_selRect = ImageBounds();
    m_hasSelection = !m_selRect.IsRectEmpty();
    Invalidate(FALSE);
    NotifySelectionSize();
    NotifyParent(CN_STATE);
}

void CCanvasWnd::DeleteSelection()
{
    if (!m_hasSelection)
        return;

    if (m_floating)
    {
        // 떼어 낸 그림만 버리면 됨 (원래 자리는 떼어 낼 때 이미 비웠음)
        m_floatImage.Destroy();
        m_floatSource.Destroy();
        m_floating = false;
    }
    else
    {
        std::vector<uint8_t> mask;
        if (BuildSelectionMask(mask))
        {
            m_history.Push(CaptureSnapshot());
            GdiFlush();
            const uint32_t fill = ToPixel(m_color2);
            const int w = m_selRect.Width();
            for (int y = 0; y < m_selRect.Height(); ++y)
            {
                auto* row = static_cast<uint32_t*>(m_image.GetPixelAddress(m_selRect.left, m_selRect.top + y));
                for (int x = 0; x < w; ++x)
                {
                    if (mask[static_cast<size_t>(y) * w + x])
                        row[x] = fill;
                }
            }
        }
    }
    ClearSelectionState();
    Invalidate(FALSE);
    NotifySelectionSize();
    MarkModified();
}

// ---------------------------------------------------------------------------
// 클립보드
// ---------------------------------------------------------------------------

bool CCanvasWnd::CopySelection()
{
    if (!m_hasSelection)
        return false;
    if (m_floating)
        return WriteClipboard(m_floatImage);
    CImage image;
    return BuildSelectionImage(image) && WriteClipboard(image);
}

bool CCanvasWnd::CutSelection()
{
    if (!CopySelection())
        return false;
    DeleteSelection();
    return true;
}

bool CCanvasWnd::CanPaste()
{
    return ::IsClipboardFormatAvailable(CF_DIB) || ::IsClipboardFormatAvailable(CF_BITMAP);
}

bool CCanvasWnd::Paste()
{
    CImage clip;
    if (!ReadClipboard(clip))
        return false;
    // 지금 화면에 보이는 영역의 왼쪽 위에 붙여넣습니다.
    return InsertFloating(clip, false);
}

bool CCanvasWnd::InsertFloatingImage(const CImage& image)
{
    // 어떤 방향의 DIB가 와도 되도록 줄 단위로 위→아래 DIB에 복사
    const int width = image.GetWidth(), height = image.GetHeight();
    CImage copy;
    if (image.IsNull() || image.GetBPP() != 32 || !CreateDib32(copy, width, height, RGB(0, 0, 0)))
        return false;
    GdiFlush();
    for (int y = 0; y < height; ++y)
        memcpy(copy.GetPixelAddress(0, y), image.GetPixelAddress(0, y), static_cast<size_t>(width) * 4);
    // 보이는 영역 가운데에 넣습니다.
    return InsertFloating(copy, true);
}

bool CCanvasWnd::InsertFloating(CImage& source, bool center)
{
    ChangeToolInternal(ToolType::Select); // 진행 중인 작업 확정 후 선택 도구로

    ImageSnapshot before = CaptureSnapshot();
    const CSize size = GetImageSize();
    if (source.GetWidth() > size.cx || source.GetHeight() > size.cy)
    {
        const int answer = MessageBox(
            _T("넣을 그림이 캔버스보다 큽니다.\n캔버스를 그림 크기에 맞게 늘릴까요?"),
            _T("MyPaint"), MB_YESNOCANCEL | MB_ICONQUESTION);
        if (answer == IDCANCEL)
            return false;
        if (answer == IDYES)
            ResizeCanvasTo(CSize(std::max<int>(size.cx, source.GetWidth()), std::max<int>(size.cy, source.GetHeight())), false);
    }
    m_history.Push(std::move(before)); // 캔버스 늘리기와 넣기를 한 단계로

    CRect client;
    GetClientRect(&client);
    CRect visible(ClientToImage(client.TopLeft()), ClientToImage(client.BottomRight()));
    visible.IntersectRect(visible, ImageBounds());
    CPoint topLeft = visible.TopLeft();
    if (center && !visible.IsRectEmpty())
        topLeft = visible.CenterPoint() - CSize(source.GetWidth() / 2, source.GetHeight() / 2);
    // 그림이 캔버스 안에 들어가면 밖으로 삐져나오지 않게
    topLeft.x = std::max<int>(0, std::min<int>(topLeft.x, GetImageSize().cx - source.GetWidth()));
    topLeft.y = std::max<int>(0, std::min<int>(topLeft.y, GetImageSize().cy - source.GetHeight()));

    m_selRect = CRect(topLeft, CSize(source.GetWidth(), source.GetHeight()));
    m_selFreeform = false;
    ReplaceImage(m_floatSource, source);
    RebuildFloatImage();
    m_floating = true;
    m_hasSelection = true;
    Invalidate(FALSE);
    NotifySelectionSize();
    MarkModified();
    return true;
}

bool CCanvasWnd::WriteClipboard(CImage& source)
{
    // 대부분의 프로그램이 읽을 수 있는 24비트 CF_DIB(아래→위 순서)로 올립니다.
    // 투명한 부분(자유형 선택 밖)은 색 2 위에 합성합니다.
    const int width = source.GetWidth();
    const int height = source.GetHeight();
    if (width <= 0 || height <= 0)
        return false;

    const int stride = (width * 3 + 3) & ~3;
    const size_t bytes = sizeof(BITMAPINFOHEADER) + static_cast<size_t>(stride) * height;
    HGLOBAL hMem = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!hMem)
        return false;

    auto* bih = static_cast<BITMAPINFOHEADER*>(::GlobalLock(hMem));
    ZeroMemory(bih, sizeof(BITMAPINFOHEADER));
    bih->biSize = sizeof(BITMAPINFOHEADER);
    bih->biWidth = width;
    bih->biHeight = height; // 양수: 아래→위
    bih->biPlanes = 1;
    bih->biBitCount = 24;
    bih->biCompression = BI_RGB;
    bih->biSizeImage = static_cast<DWORD>(stride) * height;

    GdiFlush();
    const uint32_t background = ToPixel(m_color2);
    BYTE* bits = reinterpret_cast<BYTE*>(bih + 1);
    for (int y = 0; y < height; ++y)
    {
        const auto* src = static_cast<const uint32_t*>(source.GetPixelAddress(0, y));
        BYTE* dst = bits + static_cast<size_t>(height - 1 - y) * stride;
        for (int x = 0; x < width; ++x)
        {
            const uint32_t p = BlendOver(background, src[x]);
            dst[x * 3 + 0] = static_cast<BYTE>(p & 0xFF);         // B
            dst[x * 3 + 1] = static_cast<BYTE>((p >> 8) & 0xFF);  // G
            dst[x * 3 + 2] = static_cast<BYTE>((p >> 16) & 0xFF); // R
        }
    }
    ::GlobalUnlock(hMem);

    if (!OpenClipboardWithRetry(m_hWnd))
    {
        ::GlobalFree(hMem);
        return false;
    }
    ::EmptyClipboard();
    const bool ok = ::SetClipboardData(CF_DIB, hMem) != nullptr;
    ::CloseClipboard();
    if (!ok)
        ::GlobalFree(hMem); // 성공하면 메모리는 클립보드 소유
    return ok;
}

bool CCanvasWnd::ReadClipboard(CImage& out)
{
    if (!CanPaste() || !OpenClipboardWithRetry(m_hWnd))
        return false;

    // CF_DIB만 있어도 시스템이 CF_BITMAP을 만들어 주므로 CF_BITMAP 하나로 읽습니다.
    bool ok = false;
    auto hBitmap = static_cast<HBITMAP>(::GetClipboardData(CF_BITMAP));
    BITMAP bm{};
    if (hBitmap && ::GetObject(hBitmap, sizeof(bm), &bm) && bm.bmWidth > 0 && bm.bmHeight > 0
        && CreateDib32(out, bm.bmWidth, bm.bmHeight, RGB(255, 255, 255)))
    {
        CDC src;
        src.CreateCompatibleDC(nullptr);
        HGDIOBJ old = ::SelectObject(src.GetSafeHdc(), hBitmap);
        HDC hdc = out.GetDC();
        ::BitBlt(hdc, 0, 0, bm.bmWidth, bm.bmHeight, src.GetSafeHdc(), 0, 0, SRCCOPY);
        out.ReleaseDC();
        ::SelectObject(src.GetSafeHdc(), old); // 클립보드 소유 비트맵이므로 삭제하지 않음
        ok = true;
    }
    ::CloseClipboard();

    if (ok)
    {
        // 떠 있는 그림으로 쓰므로 전부 불투명(알파 255)으로
        GdiFlush();
        for (int y = 0; y < out.GetHeight(); ++y)
        {
            auto* row = static_cast<uint32_t*>(out.GetPixelAddress(0, y));
            for (int x = 0; x < out.GetWidth(); ++x)
                row[x] |= 0xFF000000;
        }
    }
    return ok;
}

// ---------------------------------------------------------------------------
// 선택 영역 크기 핸들
// ---------------------------------------------------------------------------
//
// 핸들을 잡으면 그림을 떼어 내고(떠 있음), 끄는 동안 잡을 때의 원본을 새 크기로 다시 표본화합니다.
// 매번 원본에서 계산하므로 여러 번 늘였다 줄여도 화질이 누적해서 나빠지지 않습니다.

std::vector<CRect> CCanvasWnd::SelectionHandleRects() const
{
    if (!m_hasSelection || (m_tool != ToolType::Select && m_tool != ToolType::FreeSelect))
        return {};
    if (m_dragMode != DragMode::None && m_dragMode != DragMode::SelectionHandle)
        return {};
    CRect box = ImageToClient(m_selRect);
    box.InflateRect(1, 1);
    return BoxHandleRects(box);
}

int CCanvasWnd::HitTestSelectionHandle(CPoint clientPt) const
{
    const std::vector<CRect> rects = SelectionHandleRects();
    for (size_t i = 0; i < rects.size(); ++i)
    {
        CRect rc = rects[i];
        rc.InflateRect(2, 2);
        if (rc.PtInRect(clientPt))
            return static_cast<int>(i);
    }
    return -1;
}

void CCanvasWnd::BeginSelectionResize(int handle, CPoint imgPt)
{
    LiftSelection(false);
    if (!m_floating)
        return;
    m_selHandle = handle;
    m_selDragStartRect = m_selRect;
    m_selDragSource = ToBuffer(m_floatSource);
    m_selDragRings = m_selRings;
    BeginDrag(DragMode::SelectionHandle, DragButton::Left, imgPt);
    Invalidate(FALSE);
}

void CCanvasWnd::UpdateSelectionResize(CPoint imgPt)
{
    CRect rc = ResizeByHandle(m_selDragStartRect, m_selHandle, imgPt.x - m_dragStart.x, imgPt.y - m_dragStart.y);
    rc.right = rc.left + std::min<int>(rc.Width(), kMaxImageSide);
    rc.bottom = rc.top + std::min<int>(rc.Height(), kMaxImageSide);
    if (rc == m_selRect)
        return;

    if (rc.Size() != CSize(m_floatSource.GetWidth(), m_floatSource.GetHeight()))
    {
        FromBuffer(m_floatSource, Resize(m_selDragSource, rc.Width(), rc.Height()));
        RebuildFloatImage();
        // 윤곽선도 비례해서
        const double sx = static_cast<double>(rc.Width()) / m_selDragStartRect.Width();
        const double sy = static_cast<double>(rc.Height()) / m_selDragStartRect.Height();
        m_selRings = m_selDragRings;
        for (std::vector<CPoint>& ring : m_selRings)
            for (CPoint& p : ring)
                p = CPoint(static_cast<int>(std::lround(p.x * sx)), static_cast<int>(std::lround(p.y * sy)));
    }
    m_selRect = rc;
    Invalidate(FALSE);
    NotifySelectionSize();
}

void CCanvasWnd::CancelSelectionResize()
{
    FromBuffer(m_floatSource, m_selDragSource);
    RebuildFloatImage();
    m_selRect = m_selDragStartRect;
    m_selRings = m_selDragRings;
    Invalidate(FALSE);
}

// ---------------------------------------------------------------------------
// 선택 영역 반전
// ---------------------------------------------------------------------------

void CCanvasWnd::InvertSelection()
{
    if (!m_hasSelection || m_image.IsNull())
        return;
    FinishPendingWorkExceptSelection();

    // 떠 있으면 지금 자리에 찍고, 윤곽선만 남김
    if (m_floating)
    {
        StampFloating();
        m_floatImage.Destroy();
        m_floatSource.Destroy();
        m_floating = false;
    }

    // 지금 윤곽을 이미지 좌표의 고리들로
    std::vector<std::vector<CPoint>> rings;
    if (m_selFreeform)
    {
        for (const std::vector<CPoint>& ring : m_selRings)
        {
            std::vector<CPoint> abs;
            for (const CPoint& p : ring)
                abs.push_back(p + m_selRect.TopLeft());
            rings.push_back(abs);
        }
    }
    else
    {
        const CRect& r = m_selRect;
        rings.push_back({ CPoint(r.left, r.top), CPoint(r.right, r.top), CPoint(r.right, r.bottom), CPoint(r.left, r.bottom) });
    }

    // 바깥 테두리 고리를 앞에 추가 → 교대 채우기로 원래 윤곽의 바깥이 선택됨
    const CSize size = GetImageSize();
    rings.insert(rings.begin(), { CPoint(0, 0), CPoint(size.cx, 0), CPoint(size.cx, size.cy), CPoint(0, size.cy) });

    m_selRect = ImageBounds();
    m_selRings = rings; // 선택 영역이 (0,0)에서 시작하므로 이미지 좌표 = 상대 좌표
    m_selFreeform = true;

    std::vector<uint8_t> mask;
    if (!BuildSelectionMask(mask))
        ClearSelectionState(); // 원래 전체를 선택했었다면 반전하면 아무것도 없음

    // 선택 도구로 (SetTool은 선택을 확정해 버리므로 도구만 바꿈)
    if (m_tool != ToolType::Select && m_tool != ToolType::FreeSelect)
    {
        m_tool = ToolType::Select;
        NotifyParent(CN_TOOL);
    }
    Invalidate(FALSE);
    NotifySelectionSize();
    NotifyParent(CN_STATE);
}
