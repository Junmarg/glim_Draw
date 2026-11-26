#pragma once

/**
 * @file Theme.h
 * @brief 어두운 테마: 색, 글꼴, 아이콘 그리기, 창을 어둡게 만드는 도우미
 *
 * 도구 막대, 상태 표시줄, 드롭다운은 표준 컨트롤 대신 직접 그리는 창이고,
 * 모두 이 파일의 색과 그리기 함수를 함께 써서 한 가지 모양으로 보이게 합니다.
 */

namespace theme
{
    // ----- 색 -----
    constexpr COLORREF kWindow     = RGB(32, 32, 32);   ///< 창 바탕 (도구 막대, 상태 표시줄)
    constexpr COLORREF kWorkspace  = RGB(24, 24, 24);   ///< 그림 바깥 작업 영역
    constexpr COLORREF kSurface    = RGB(44, 44, 44);   ///< 드롭다운 등 떠 있는 판
    constexpr COLORREF kHover      = RGB(56, 56, 56);   ///< 마우스를 올린 항목
    constexpr COLORREF kPressed    = RGB(68, 68, 68);   ///< 누른 항목 / 선택된 도구
    constexpr COLORREF kBorder     = RGB(64, 64, 64);   ///< 테두리, 구분선
    constexpr COLORREF kText       = RGB(232, 232, 232);///< 기본 글자, 아이콘
    constexpr COLORREF kTextDim    = RGB(160, 160, 160);///< 보조 글자 (그룹 이름)
    constexpr COLORREF kTextOff    = RGB(96, 96, 96);   ///< 사용할 수 없는 항목
    constexpr COLORREF kAccent     = RGB(76, 194, 255); ///< 강조 (선택 표시, 슬라이더)

    // ----- 아이콘 (Segoe MDL2 Assets 글리프) -----
    constexpr wchar_t kGlyphPencil    = 0xE70F;
    constexpr wchar_t kGlyphEraser    = 0xE75C;
    constexpr wchar_t kGlyphText      = 0xE8D2;
    constexpr wchar_t kGlyphZoom      = 0xE71E;
    constexpr wchar_t kGlyphZoomIn    = 0xE8A3;
    constexpr wchar_t kGlyphZoomOut   = 0xE71F;
    constexpr wchar_t kGlyphPicker    = 0xEF3C;
    constexpr wchar_t kGlyphBrush     = 0xEE56;
    constexpr wchar_t kGlyphCrop      = 0xE7A8;
    constexpr wchar_t kGlyphResize    = 0xE740;
    constexpr wchar_t kGlyphRotate    = 0xE7AD;
    constexpr wchar_t kGlyphUndo      = 0xE7A7;
    constexpr wchar_t kGlyphRedo      = 0xE7A6;
    constexpr wchar_t kGlyphCut       = 0xE8C6;
    constexpr wchar_t kGlyphCopy      = 0xE8C8;
    constexpr wchar_t kGlyphPaste     = 0xE77F;
    constexpr wchar_t kGlyphPalette   = 0xE790;
    constexpr wchar_t kGlyphChevron   = 0xE70D;
    constexpr wchar_t kGlyphCheck     = 0xE73E;
    constexpr wchar_t kGlyphBold      = 0xE8DD;
    constexpr wchar_t kGlyphItalic    = 0xE8DB;
    constexpr wchar_t kGlyphUnderline = 0xE8DC;
    constexpr wchar_t kGlyphFont      = 0xE8D2;
    constexpr wchar_t kGlyphPointer   = 0xE8B0;
    constexpr wchar_t kGlyphPicture   = 0xE7FB;
    constexpr wchar_t kGlyphSelectAll = 0xE8B3;
    constexpr wchar_t kGlyphDelete    = 0xE74D;

    /** @brief 글리프로 없는 아이콘 (GDI+로 직접 그림) */
    enum class DrawnIcon { Bucket, RectSelect, FreeSelect, LineWidths, Graph };

    // ----- 크기 -----
    /** @brief 96 DPI 기준 값을 창의 DPI에 맞춤 */
    int Scale(int value, HWND hwnd);

    // ----- 그리기 -----
    /** @brief 아이콘 글리프 (rc 가운데에) */
    void DrawGlyph(CDC& dc, wchar_t glyph, const CRect& rc, COLORREF color, int pixelSize);
    /** @brief 직접 그리는 아이콘 (안티앨리어싱) */
    void DrawIcon(CDC& dc, DrawnIcon icon, const CRect& rc, COLORREF color);
    /** @brief 둥근 사각형 채우기 (안티앨리어싱) */
    void FillRound(CDC& dc, const CRect& rc, int radius, COLORREF color);
    /** @brief 둥근 사각형 테두리 (안티앨리어싱) */
    void FrameRound(CDC& dc, const CRect& rc, int radius, COLORREF color, float width = 1.0f);
    /** @brief 원 채우기 + 테두리 (색 1/색 2 표시) */
    void FillCircle(CDC& dc, const CRect& rc, COLORREF fill, COLORREF border);

    /** @brief UI 글꼴 (맑은 고딕). pixelHeight는 DPI 적용된 글자 높이 */
    void CreateUiFont(CFont& font, int pixelHeight, bool bold = false);

    // ----- 창을 어둡게 -----
    /** @brief 프로세스 전체: 팝업 메뉴를 어둡게 (Windows 10 1903 이상, 안 되면 아무 일도 안 함) */
    void InitDarkMenus();
    /** @brief 제목 표시줄을 어둡게 (Windows 10 2004 이상 / 11) */
    void UseDarkTitleBar(HWND hwnd);
    /** @brief 창의 스크롤바를 어둡게 */
    void UseDarkScrollbars(HWND hwnd);
    /** @brief 떠 있는 창의 모서리를 둥글게 (Windows 11, 안 되면 무시) */
    void UseRoundCorners(HWND hwnd);
}
