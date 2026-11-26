/**
 * @file TopBar.cpp
 * @brief 위쪽 도구 막대 구현
 *
 * 배치 (96 DPI 기준)
 * ```
 *  ┌ 메뉴 줄 (32) ─ 파일 편집 이미지 보기 도움말 │ ↶ ↷ ─────────────────────────────────┐
 *  ├ 도구 줄 ─ [선택] │ [이미지] │ [도구] │ [브러시] │ [도형 모음 | 윤곽선 채우기] │ [굵기] │ [색] ┤
 *  │                                      ↑ 텍스트 도구일 때는 [텍스트] 그룹으로 바뀜          │
 *  └──────────── 그룹 이름 ───────────────────────────────────────────────────────────────┘
 * ```
 */
#include "pch.h"
#include "MyPaint.h"
#include "TopBar.h"
#include "PickerPopup.h"

#include <cmath>
#include <memory>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
    constexpr int kMenuRow = 32;      ///< 메뉴 줄 높이
    constexpr int kContent = 60;      ///< 도구 줄 버튼 영역 높이
    constexpr int kGroupLabel = 16;   ///< 그룹 이름 높이
    constexpr int kGalleryCols = 7;   ///< 도형 모음에 보이는 열 수
    constexpr int kGalleryRows = 3;

    /** @brief "파일(&F)" → "파일" */
    CString StripMnemonic(CString text)
    {
        const int paren = text.Find(_T("(&"));
        if (paren >= 0)
            text = text.Left(paren);
        text.Remove(_T('&'));
        return text;
    }

    /** @brief 도구 명령 ID → 도구 */
    bool ToolFromCommand(UINT id, ToolType& tool)
    {
        if (id < IDC_TOOL_PENCIL || id > IDC_TOOL_MAGNIFIER)
            return false;
        tool = static_cast<ToolType>(id - IDC_TOOL_PENCIL);
        return true;
    }
}

BEGIN_MESSAGE_MAP(CTopBar, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_RBUTTONDOWN()
    ON_WM_CAPTURECHANGED()
END_MESSAGE_MAP()

CTopBar::CTopBar()
    : m_palette{
        // 그림판 기본 20색: 첫 줄 진한 색 10개, 둘째 줄 밝은 색 10개 / 셋째 줄 사용자 지정 10칸
        RGB(0, 0, 0),       RGB(127, 127, 127), RGB(136, 0, 21),    RGB(237, 28, 36),   RGB(255, 127, 39),
        RGB(255, 242, 0),   RGB(34, 177, 76),   RGB(0, 162, 232),   RGB(63, 72, 204),   RGB(163, 73, 164),
        RGB(255, 255, 255), RGB(195, 195, 195), RGB(185, 122, 87),  RGB(255, 174, 201), RGB(255, 201, 14),
        RGB(239, 228, 176), RGB(181, 230, 29),  RGB(153, 217, 234), RGB(112, 146, 190), RGB(200, 191, 231),
        RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255),
        RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255) }
{
}

BOOL CTopBar::Create(CWnd* pParent, UINT nID)
{
    const CString cls = AfxRegisterWndClass(0, ::LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr);
    if (!CWnd::CreateEx(0, cls, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, CRect(0, 0, 10, 10), pParent, nID))
        return FALSE;
    m_dpi = static_cast<int>(GetDpiForWindow(m_hWnd));
    m_menuHeight = Px(kMenuRow);
    theme::CreateUiFont(m_font, Px(12));
    theme::CreateUiFont(m_smallFont, Px(11));
    m_menu.LoadMenu(IDR_MAIN_MENU);
    Layout();
    return TRUE;
}

int CTopBar::PreferredHeight() const
{
    return Px(kMenuRow) + Px(4) + Px(kContent) + Px(2) + Px(kGroupLabel) + Px(4);
}

COLORREF CTopBar::GetPaletteColor(int index) const
{
    return (index >= 0 && index < kPaletteCount) ? m_palette[index] : RGB(0, 0, 0);
}

void CTopBar::AddCustomColor(COLORREF color)
{
    m_palette[20 + m_nextCustom] = color;
    m_nextCustom = (m_nextCustom + 1) % 10;
    Invalidate(FALSE);
}

void CTopBar::SetState(const TopBarState& state)
{
    const bool textMode = (state.tool == ToolType::Text);
    m_state = state;
    if (textMode != m_textMode)
    {
        m_textMode = textMode; // 텍스트 도구면 브러시·도형 그룹 자리에 텍스트 그룹
        Layout();
    }
    else if (textMode)
    {
        Layout(); // 글꼴 이름이 바뀌면 버튼 글자도 바뀜
    }
    Invalidate(FALSE);
}

// ---------------------------------------------------------------------------
// 배치
// ---------------------------------------------------------------------------

void CTopBar::Layout()
{
    if (!GetSafeHwnd())
        return;
    m_items.clear();
    m_groups.clear();
    int x = Px(8);
    AddMenuRow(x);
    AddGroups();
    UpdateTooltips();
    m_hot = m_pressed = -1;
}

void CTopBar::AddMenuRow(int& x)
{
    CClientDC dc(this);
    CFont* pOld = dc.SelectObject(&m_font);
    for (int i = 0; i < static_cast<int>(m_menu.GetMenuItemCount()); ++i)
    {
        CString text;
        m_menu.GetMenuString(i, text, MF_BYPOSITION);
        Item it;
        it.kind = Kind::Menu;
        it.index = i;
        it.label = StripMnemonic(text);
        const int w = dc.GetTextExtent(it.label).cx + Px(20);
        it.rc = CRect(x, Px(4), x + w, m_menuHeight - Px(2));
        m_items.push_back(it);
        x += w;
    }
    dc.SelectObject(pOld);

    // 실행 취소 / 다시 실행
    x += Px(12);
    for (const auto& [cmd, glyph, tip] : { std::tuple<UINT, wchar_t, LPCTSTR>{ ID_EDIT_UNDO_ACTION, theme::kGlyphUndo, _T("실행 취소 (Ctrl+Z)") },
                                           std::tuple<UINT, wchar_t, LPCTSTR>{ ID_EDIT_REDO_ACTION, theme::kGlyphRedo, _T("다시 실행 (Ctrl+Y)") } })
    {
        Item it;
        it.kind = Kind::Small;
        it.command = cmd;
        it.icon.glyph = glyph;
        it.tip = tip;
        it.rc = CRect(x, Px(4), x + Px(32), m_menuHeight - Px(2));
        m_items.push_back(it);
        x += Px(34);
    }
}

void CTopBar::AddGroups()
{
    const int top = m_menuHeight + Px(4);
    const int bottom = top + Px(kContent);
    const int gap = Px(14);
    int x = Px(10);

    auto glyphIcon = [](wchar_t g) { Icon i; i.glyph = g; return i; };
    auto drawnIcon = [](theme::DrawnIcon d) { Icon i; i.drawn = true; i.shape = d; return i; };
    auto add = [&](Kind kind, const CRect& rc, UINT cmd, Popup popup, Icon icon, LPCTSTR label, LPCTSTR tip, int index = -1) {
        Item it;
        it.kind = kind; it.rc = rc; it.command = cmd; it.popup = popup; it.icon = icon;
        it.label = label; it.tip = tip; it.index = index;
        m_items.push_back(it);
    };
    auto group = [&](LPCTSTR name, int left, int right) {
        m_groups.push_back({ name, CRect(left - Px(6), top, right + Px(6), bottom + Px(2) + Px(kGroupLabel)) });
        x = right + gap;
    };
    const int bw = Px(32), bh = Px(29);              // 작은 버튼
    const int row1 = top + Px(1), row2 = top + Px(31);

    // [선택]
    {
        const int left = x;
        add(Kind::Large, CRect(x, top, x + Px(52), bottom), IDC_TOOL_SELECT, Popup::Selection,
            drawnIcon(theme::DrawnIcon::RectSelect), _T("선택"), _T("선택 (아래쪽: 선택 방식, 모두 선택, 반전, 투명 선택)"));
        group(_T("선택"), left, x + Px(52));
    }
    // [이미지]
    {
        const int left = x;
        add(Kind::Small, CRect(x, row1, x + bw, row1 + bh), ID_IMAGE_CROP, Popup::None, glyphIcon(theme::kGlyphCrop), _T(""), _T("자르기 (Ctrl+Shift+X)"));
        add(Kind::Small, CRect(x + bw + Px(2), row1, x + bw * 2 + Px(2), row1 + bh), ID_IMAGE_RESIZE_SKEW, Popup::None,
            glyphIcon(theme::kGlyphResize), _T(""), _T("크기 조정 및 기울이기 (Ctrl+W)"));
        add(Kind::Labeled, CRect(x, row2, x + bw * 2 + Px(2), row2 + bh), 0, Popup::Rotate, glyphIcon(theme::kGlyphRotate), _T(""), _T("회전 / 대칭"));
        group(_T("이미지"), left, x + bw * 2 + Px(2));
    }
    // [그래프] 나만의 기능: n차 함수 그래프 넣기
    {
        const int left = x;
        add(Kind::Large, CRect(x, top, x + Px(52), bottom), ID_IMAGE_FUNCTION_GRAPH, Popup::None, drawnIcon(theme::DrawnIcon::Graph),
            _T("f(x)"), _T("함수 그래프 넣기 (Ctrl+Shift+G): n차 함수를 입력하면 그래프를 그려 캔버스에 넣음"));
        group(_T("그래프"), left, x + Px(52));
    }
    // [도구]
    {
        const int left = x;
        struct T { UINT id; Icon icon; LPCTSTR tip; };
        const T tools[6] = {
            { IDC_TOOL_PENCIL, glyphIcon(theme::kGlyphPencil), _T("연필") },
            { IDC_TOOL_FILL, drawnIcon(theme::DrawnIcon::Bucket), _T("채우기") },
            { IDC_TOOL_TEXT, glyphIcon(theme::kGlyphText), _T("텍스트") },
            { IDC_TOOL_ERASER, glyphIcon(theme::kGlyphEraser), _T("지우개") },
            { IDC_TOOL_PICKER, glyphIcon(theme::kGlyphPicker), _T("스포이트") },
            { IDC_TOOL_MAGNIFIER, glyphIcon(theme::kGlyphZoom), _T("돋보기") },
        };
        for (int i = 0; i < 6; ++i)
        {
            const int cx = x + (i % 3) * (bw + Px(2));
            const int cy = (i < 3) ? row1 : row2;
            add(Kind::Small, CRect(cx, cy, cx + bw, cy + bh), tools[i].id, Popup::None, tools[i].icon, _T(""), tools[i].tip);
        }
        group(_T("도구"), left, x + bw * 3 + Px(4));
    }

    if (m_textMode)
    {
        // [텍스트] 글꼴 / 굵게·기울임·밑줄 / 배경 투명
        const int left = x;
        const int w = Px(170);
        add(Kind::Labeled, CRect(x, row1, x + w, row1 + bh), ID_TEXT_FONT_CMD, Popup::None, glyphIcon(theme::kGlyphFont),
            m_state.fontLabel, _T("글꼴 (글꼴, 크기, 효과, 색)"));
        add(Kind::Small, CRect(x, row2, x + bw, row2 + bh), ID_TEXT_BOLD_CMD, Popup::None, glyphIcon(theme::kGlyphBold), _T(""), _T("굵게 (Ctrl+B)"));
        add(Kind::Small, CRect(x + bw + Px(2), row2, x + bw * 2 + Px(2), row2 + bh), ID_TEXT_ITALIC_CMD, Popup::None,
            glyphIcon(theme::kGlyphItalic), _T(""), _T("기울임 (Ctrl+I)"));
        add(Kind::Small, CRect(x + (bw + Px(2)) * 2, row2, x + bw * 3 + Px(4), row2 + bh), ID_TEXT_UNDERLINE_CMD, Popup::None,
            glyphIcon(theme::kGlyphUnderline), _T(""), _T("밑줄 (Ctrl+U)"));
        add(Kind::Labeled, CRect(x + bw * 3 + Px(10), row2, x + w, row2 + bh), ID_TEXT_TRANSPARENT_CMD, Popup::None, Icon(),
            _T("배경 투명"), _T("배경 투명: 끄면 글상자를 색 2로 채움"));
        group(_T("텍스트"), left, x + w);
    }
    else
    {
        // [브러시]
        {
            const int left = x;
            add(Kind::Large, CRect(x, top, x + Px(56), bottom), IDC_TOOL_BRUSH, Popup::Brush, glyphIcon(theme::kGlyphBrush),
                _T("브러시"), _T("브러시 (아래쪽: 브러시 종류)"));
            group(_T("브러시"), left, x + Px(56));
        }
        // [도형] 모음 + 윤곽선/채우기
        {
            const int left = x;
            const int cw = Px(24), ch = Px(20);
            for (int i = 0; i < kGalleryCols * kGalleryRows && i < kShapeKindCount; ++i)
            {
                const int cx = x + (i % kGalleryCols) * cw;
                const int cy = top + (i / kGalleryCols) * ch;
                add(Kind::ShapeCell, CRect(cx, cy, cx + cw, cy + ch), ID_SHAPE_FIRST + i, Popup::None, Icon(), _T(""),
                    ShapeName(static_cast<ShapeKind>(i)), i);
            }
            const int moreX = x + kGalleryCols * cw;
            add(Kind::ShapeMore, CRect(moreX, top, moreX + Px(16), top + ch * kGalleryRows), 0, Popup::Shapes, Icon(), _T(""), _T("도형 전체 보기"));
            const int ox = moreX + Px(22);
            const int ow = Px(92);
            add(Kind::Labeled, CRect(ox, row1, ox + ow, row1 + bh), 0, Popup::Outline, Icon(), _T("윤곽선"), _T("도형 윤곽선"));
            add(Kind::Labeled, CRect(ox, row2, ox + ow, row2 + bh), 0, Popup::Fill, Icon(), _T("채우기"), _T("도형 채우기"));
            group(_T("도형"), left, ox + ow);
        }
        // [굵기]
        {
            const int left = x;
            add(Kind::Large, CRect(x, top, x + Px(52), bottom), 0, Popup::Size, drawnIcon(theme::DrawnIcon::LineWidths),
                _T("굵기"), _T("굵기 (Ctrl+Plus / Ctrl+Minus)"));
            group(_T("굵기"), left, x + Px(52));
        }
    }

    // [색] 색 1, 색 2, 팔레트 3줄, 색 편집
    {
        const int left = x;
        const int sw = Px(36);
        add(Kind::Swatch, CRect(x, top, x + sw, bottom), ID_COLOR_EDIT1, Popup::None, Icon(), _T("색 1"), _T("색 1: 왼쪽 버튼으로 그리는 색 (누르면 색 편집)"), 0);
        add(Kind::Swatch, CRect(x + sw + Px(4), top, x + sw * 2 + Px(4), bottom), ID_COLOR_EDIT2, Popup::None, Icon(), _T("색 2"),
            _T("색 2: 오른쪽 버튼·지우개·배경 색 (누르면 색 편집)"), 1);
        const int px = x + sw * 2 + Px(14);
        const int cell = Px(20);
        for (int i = 0; i < kPaletteCount; ++i)
        {
            const int cx = px + (i % 10) * cell;
            const int cy = top + (i / 10) * cell;
            add(Kind::Palette, CRect(cx, cy, cx + cell, cy + cell), 0, Popup::None, Icon(), _T(""),
                i < 20 ? _T("왼쪽 클릭: 색 1 · 오른쪽 클릭: 색 2") : _T("사용자 지정 색 (색 편집에서 고른 색이 차례로 들어감)"), i);
        }
        const int ex = px + cell * 10 + Px(8);
        add(Kind::Large, CRect(ex, top, ex + Px(52), bottom), ID_COLOR_EDIT1, Popup::None, glyphIcon(theme::kGlyphPalette),
            _T("색 편집"), _T("색 편집 (색 1)"));
        group(_T("색"), left, ex + Px(52));
    }
    m_contentRight = x;
}

void CTopBar::UpdateTooltips()
{
    if (m_tooltip.GetSafeHwnd())
        m_tooltip.DestroyWindow();
    m_tooltip.Create(this, TTS_ALWAYSTIP | TTS_NOPREFIX);
    theme::UseDarkScrollbars(m_tooltip.GetSafeHwnd()); // 어두운 테마로 그려지게
    for (size_t i = 0; i < m_items.size(); ++i)
    {
        if (!m_items[i].tip.IsEmpty())
            m_tooltip.AddTool(this, m_items[i].tip, m_items[i].rc, static_cast<UINT_PTR>(i + 1));
    }
    m_tooltip.Activate(TRUE);
}

void CTopBar::OnSize(UINT nType, int cx, int cy)
{
    CWnd::OnSize(nType, cx, cy);
    Invalidate(FALSE);
}

// ---------------------------------------------------------------------------
// 상태 판정
// ---------------------------------------------------------------------------

bool CTopBar::IsActive(const Item& item) const
{
    switch (item.kind)
    {
    case Kind::Small:
    case Kind::Large:
    case Kind::Labeled:
    {
        ToolType tool;
        if (ToolFromCommand(item.command, tool))
        {
            if (item.command == IDC_TOOL_SELECT)
                return m_state.tool == ToolType::Select || m_state.tool == ToolType::FreeSelect;
            return m_state.tool == tool;
        }
        switch (item.command)
        {
        case ID_TEXT_BOLD_CMD:        return m_state.textBold;
        case ID_TEXT_ITALIC_CMD:      return m_state.textItalic;
        case ID_TEXT_UNDERLINE_CMD:   return m_state.textUnderline;
        case ID_TEXT_TRANSPARENT_CMD: return m_state.textTransparent;
        default:                      return false;
        }
    }
    case Kind::ShapeCell:
        return m_state.tool == ToolType::Shape && static_cast<int>(m_state.shape) == item.index;
    case Kind::ShapeMore:
        return m_state.tool == ToolType::Shape && static_cast<int>(m_state.shape) >= kGalleryCols * kGalleryRows;
    default:
        return false;
    }
}

bool CTopBar::IsEnabled(const Item& item) const
{
    switch (item.command)
    {
    case ID_EDIT_UNDO_ACTION: return m_state.canUndo;
    case ID_EDIT_REDO_ACTION: return m_state.canRedo;
    case ID_IMAGE_CROP:       return m_state.hasSelection;
    default: break;
    }
    if (item.popup == Popup::Size)
        return CCanvasWnd::ToolUsesSize(m_state.tool);
    if (item.popup == Popup::Fill)
        return !IsOpenShape(m_state.shape);
    return true;
}

CTopBar::Icon CTopBar::ItemIcon(const Item& item) const
{
    if (item.kind == Kind::Large && item.command == IDC_TOOL_SELECT)
    {
        // 선택 버튼은 지금 고른 선택 방식의 아이콘
        Icon icon;
        icon.drawn = true;
        icon.shape = (m_state.tool == ToolType::FreeSelect) ? theme::DrawnIcon::FreeSelect : theme::DrawnIcon::RectSelect;
        return icon;
    }
    return item.icon;
}

// ---------------------------------------------------------------------------
// 그리기
// ---------------------------------------------------------------------------

BOOL CTopBar::OnEraseBkgnd(CDC* /*pDC*/)
{
    return TRUE;
}

void CTopBar::DrawIconAt(CDC& dc, const Icon& icon, const CRect& rc, COLORREF color, int glyphPx)
{
    if (icon.drawn)
    {
        CRect r(CPoint(rc.CenterPoint().x - glyphPx / 2, rc.CenterPoint().y - glyphPx / 2), CSize(glyphPx, glyphPx));
        theme::DrawIcon(dc, icon.shape, r, color);
    }
    else if (icon.glyph)
    {
        theme::DrawGlyph(dc, icon.glyph, rc, color, glyphPx);
    }
}

void CTopBar::DrawItem(CDC& dc, const Item& item, int itemIndex)
{
    const bool hot = (itemIndex == m_hot);
    const bool pressed = (itemIndex == m_pressed);
    const bool active = IsActive(item);
    const bool enabled = IsEnabled(item);
    const COLORREF fg = enabled ? theme::kText : theme::kTextOff;
    const int radius = Px(4);

    // 바탕: 선택된 도구 > 누름 > 마우스 올림
    auto background = [&](const CRect& rc) {
        if (active)
        {
            theme::FillRound(dc, rc, radius, theme::kPressed);
            const int w = std::min<int>(Px(16), rc.Width() / 2);
            theme::FillRound(dc, CRect(rc.CenterPoint().x - w / 2, rc.bottom - Px(3), rc.CenterPoint().x + w / 2, rc.bottom - Px(1)),
                             Px(1), theme::kAccent); // 강조 막대
        }
        else if (enabled && (pressed || hot))
        {
            theme::FillRound(dc, rc, radius, pressed ? theme::kPressed : theme::kHover);
        }
    };

    switch (item.kind)
    {
    case Kind::Menu:
    {
        if (hot || m_openMenu == item.index)
            theme::FillRound(dc, item.rc, radius, m_openMenu == item.index ? theme::kPressed : theme::kHover);
        dc.SetTextColor(theme::kText);
        CRect r = item.rc;
        dc.DrawText(item.label, r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        break;
    }
    case Kind::Small:
        background(item.rc);
        DrawIconAt(dc, ItemIcon(item), item.rc, fg, Px(16));
        break;
    case Kind::Large:
    {
        background(item.rc);
        CRect iconRc(item.rc.left, item.rc.top + Px(4), item.rc.right, item.rc.top + Px(34));
        DrawIconAt(dc, ItemIcon(item), iconRc, fg, Px(22));
        CRect labelRc(item.rc.left, iconRc.bottom, item.rc.right, item.rc.bottom - Px(4));
        dc.SetTextColor(fg);
        if (item.popup != Popup::None)
        {
            // 이름 아래에 ▾
            CRect textRc(labelRc.left, labelRc.top, labelRc.right, labelRc.top + Px(14));
            dc.DrawText(item.label, textRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            theme::DrawGlyph(dc, theme::kGlyphChevron, CRect(labelRc.left, textRc.bottom, labelRc.right, labelRc.bottom), fg, Px(8));
        }
        else
        {
            dc.DrawText(item.label, labelRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
        break;
    }
    case Kind::Labeled:
    {
        background(item.rc);
        CRect r = item.rc;
        r.DeflateRect(Px(6), 0);
        if (item.icon.glyph || item.icon.drawn)
        {
            DrawIconAt(dc, item.icon, CRect(r.left, r.top, r.left + Px(16), r.bottom), fg, Px(15));
            r.left += Px(22);
        }
        if (item.popup != Popup::None)
        {
            theme::DrawGlyph(dc, theme::kGlyphChevron, CRect(r.right - Px(12), r.top, r.right, r.bottom), fg, Px(8));
            r.right -= Px(14);
        }
        dc.SetTextColor(fg);
        dc.DrawText(item.label, r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        break;
    }
    case Kind::ShapeCell:
    {
        background(item.rc);
        CRect r = item.rc;
        r.DeflateRect(Px(5), Px(4));
        DrawShapeIcon(dc, static_cast<ShapeKind>(item.index), r, fg);
        break;
    }
    case Kind::ShapeMore:
        background(item.rc);
        theme::DrawGlyph(dc, theme::kGlyphChevron, item.rc, fg, Px(9));
        break;
    case Kind::Swatch:
    {
        if (hot)
            theme::FillRound(dc, item.rc, radius, theme::kHover);
        const COLORREF color = item.index == 0 ? m_state.color1 : m_state.color2;
        const int d = Px(item.index == 0 ? 28 : 24);
        const CPoint c(item.rc.CenterPoint().x, item.rc.top + Px(18));
        theme::FillCircle(dc, CRect(c.x - d / 2, c.y - d / 2, c.x + d / 2, c.y + d / 2), color, theme::kTextDim);
        dc.SetTextColor(theme::kTextDim);
        CRect labelRc(item.rc.left, item.rc.top + Px(36), item.rc.right, item.rc.bottom);
        dc.DrawText(item.label, labelRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        break;
    }
    case Kind::Palette:
    {
        const int d = Px(16);
        const CPoint c = item.rc.CenterPoint();
        const CRect circle(c.x - d / 2, c.y - d / 2, c.x + d / 2, c.y + d / 2);
        theme::FillCircle(dc, circle, m_palette[item.index], hot ? theme::kText : theme::kBorder);
        if (hot)
        {
            CRect ring = circle;
            ring.InflateRect(Px(2), Px(2));
            theme::FrameRound(dc, ring, ring.Width() / 2, theme::kText, 1.2f);
        }
        break;
    }
    }
}

void CTopBar::OnPaint()
{
    CPaintDC dc(this);
    CRect client;
    GetClientRect(&client);
    CDC mem;
    mem.CreateCompatibleDC(&dc);
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(&dc, client.Width(), client.Height());
    CBitmap* pOldBmp = mem.SelectObject(&bmp);

    mem.FillSolidRect(client, theme::kWindow);
    mem.FillSolidRect(0, client.bottom - 1, client.Width(), 1, theme::kBorder);
    mem.SetBkMode(TRANSPARENT);

    // 그룹 이름과 구분선
    CFont* pOldFont = mem.SelectObject(&m_smallFont);
    mem.SetTextColor(theme::kTextDim);
    for (size_t i = 0; i < m_groups.size(); ++i)
    {
        const Group& g = m_groups[i];
        CRect label(g.rc.left, g.rc.bottom - Px(kGroupLabel), g.rc.right, g.rc.bottom);
        mem.DrawText(g.name, label, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        if (i + 1 < m_groups.size())
        {
            const int sx = (g.rc.right + m_groups[i + 1].rc.left) / 2;
            mem.FillSolidRect(sx, g.rc.top + Px(6), 1, g.rc.Height() - Px(10), theme::kBorder);
        }
    }

    mem.SelectObject(&m_font);
    for (size_t i = 0; i < m_items.size(); ++i)
        DrawItem(mem, m_items[i], static_cast<int>(i));
    mem.SelectObject(pOldFont);

    dc.BitBlt(0, 0, client.Width(), client.Height(), &mem, 0, 0, SRCCOPY);
    mem.SelectObject(pOldBmp);
}

// ---------------------------------------------------------------------------
// 마우스
// ---------------------------------------------------------------------------

int CTopBar::HitTest(CPoint pt) const
{
    for (size_t i = 0; i < m_items.size(); ++i)
        if (m_items[i].rc.PtInRect(pt))
            return static_cast<int>(i);
    return -1;
}

bool CTopBar::IsPopupPart(const Item& item, CPoint pt) const
{
    if (item.popup == Popup::None)
        return false;
    if (item.kind == Kind::Large && item.command != 0)
        return pt.y >= item.rc.top + Px(34); // 큰 버튼: 아래쪽(이름 ▾)이 드롭다운
    return true;
}

void CTopBar::OnMouseMove(UINT /*nFlags*/, CPoint point)
{
    if (const MSG* msg = GetCurrentMessage())
        m_tooltip.RelayEvent(const_cast<MSG*>(msg));
    if (!m_tracking)
    {
        TRACKMOUSEEVENT tme{ sizeof(TRACKMOUSEEVENT), TME_LEAVE, m_hWnd, 0 };
        m_tracking = TrackMouseEvent(&tme) != FALSE;
    }
    const int hot = HitTest(point);
    if (hot != m_hot)
    {
        m_hot = hot;
        Invalidate(FALSE);
    }
}

void CTopBar::OnMouseLeave()
{
    m_tracking = false;
    if (m_hot != -1)
    {
        m_hot = -1;
        Invalidate(FALSE);
    }
}

void CTopBar::OnLButtonDown(UINT /*nFlags*/, CPoint point)
{
    if (const MSG* msg = GetCurrentMessage())
        m_tooltip.RelayEvent(const_cast<MSG*>(msg));
    const int index = HitTest(point);
    if (index < 0)
        return;
    const Item item = m_items[index]; // 메뉴·드롭다운 중에 배치가 바뀔 수 있으므로 복사
    if (!IsEnabled(item))
        return;
    if (item.kind == Kind::Menu)
    {
        OpenMenu(item.index);
        return;
    }
    if (IsPopupPart(item, point))
    {
        ShowPopup(item); // 드롭다운은 누르는 즉시 (메뉴와 같이)
        return;
    }
    m_pressed = index;
    SetCapture();
    Invalidate(FALSE);
}

void CTopBar::OnLButtonUp(UINT /*nFlags*/, CPoint point)
{
    if (m_pressed < 0)
        return;
    const int pressed = m_pressed;
    m_pressed = -1;
    if (GetCapture() == this)
        ReleaseCapture();
    Invalidate(FALSE);
    if (HitTest(point) == pressed)
        Activate(pressed, point, false);
}

void CTopBar::OnRButtonDown(UINT /*nFlags*/, CPoint point)
{
    const int index = HitTest(point);
    if (index >= 0 && m_items[index].kind == Kind::Palette)
        Activate(index, point, true);
}

void CTopBar::OnCaptureChanged(CWnd* pWnd)
{
    if (pWnd != this && m_pressed >= 0)
    {
        m_pressed = -1;
        Invalidate(FALSE);
    }
    CWnd::OnCaptureChanged(pWnd);
}

void CTopBar::SendCommand(UINT id)
{
    GetParent()->SendMessage(WM_COMMAND, MAKEWPARAM(id, 0), 0);
}

void CTopBar::Activate(int itemIndex, CPoint /*pt*/, bool rightButton)
{
    const Item item = m_items[itemIndex];
    switch (item.kind)
    {
    case Kind::Palette:
        SendCommand((rightButton ? ID_COLOR2_FIRST : ID_COLOR1_FIRST) + item.index);
        break;
    case Kind::Large:
        if (item.command == IDC_TOOL_SELECT && m_state.tool == ToolType::FreeSelect)
            SendCommand(IDC_TOOL_FREESELECT); // 선택 버튼: 지금 고른 선택 방식 유지
        else if (item.command)
            SendCommand(item.command);
        break;
    default:
        if (item.command)
            SendCommand(item.command);
        break;
    }
}

void CTopBar::OpenMenu(int index)
{
    CMenu* sub = m_menu.GetSubMenu(index);
    const auto it = std::find_if(m_items.begin(), m_items.end(), [&](const Item& i) { return i.kind == Kind::Menu && i.index == index; });
    if (!sub || it == m_items.end())
        return;
    m_openMenu = index;
    Invalidate(FALSE);
    UpdateWindow();
    CPoint pt(it->rc.left, it->rc.bottom);
    ClientToScreen(&pt);
    // 주 대화 상자를 소유자로: WM_INITMENUPOPUP(켜고 끄기)과 WM_COMMAND(명령)를 대화 상자가 받음
    sub->TrackPopupMenu(TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON, pt.x, pt.y, GetParent());
    m_openMenu = -1;
    Invalidate(FALSE);
}

// ---------------------------------------------------------------------------
// 드롭다운
// ---------------------------------------------------------------------------

void CTopBar::ShowPopup(const Item& item)
{
    CRect anchor = item.rc;
    ClientToScreen(&anchor);

    CPickerPopup::Spec spec;
    std::vector<UINT> commands; // 항목 번호 → 보낼 명령 (0이면 구분선)

    // 목록 한 줄: [✓] [아이콘] 글자
    auto listRow = [this](CDC& dc, const CRect& rc, bool hot, bool checked, bool enabled, const Icon& icon, const CString& text) {
        if (hot)
        {
            CRect r = rc;
            r.DeflateRect(Px(2), Px(1));
            theme::FillRound(dc, r, Px(4), theme::kHover);
        }
        const COLORREF fg = enabled ? theme::kText : theme::kTextOff;
        if (checked)
            theme::DrawGlyph(dc, theme::kGlyphCheck, CRect(rc.left + Px(4), rc.top, rc.left + Px(24), rc.bottom), theme::kAccent, Px(12));
        DrawIconAt(dc, icon, CRect(rc.left + Px(26), rc.top, rc.left + Px(46), rc.bottom), fg, Px(16));
        CFont* pOld = dc.SelectObject(&m_font);
        dc.SetBkMode(TRANSPARENT);
        dc.SetTextColor(fg);
        CRect tr(rc.left + Px(52), rc.top, rc.right - Px(8), rc.bottom);
        dc.DrawText(text, tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        dc.SelectObject(pOld);
    };
    auto separator = [this](CDC& dc, const CRect& rc) {
        dc.FillSolidRect(rc.left + Px(8), rc.CenterPoint().y, rc.Width() - Px(16), 1, theme::kBorder);
    };
    auto glyph = [](wchar_t g) { Icon i; i.glyph = g; return i; };
    auto drawn = [](theme::DrawnIcon d) { Icon i; i.drawn = true; i.shape = d; return i; };

    // 브러시 미리 보기 (드롭다운을 열 때마다 만듦). CImage는 복사하면 비트맵 핸들을 함께 쓰게 되어
    // 벡터가 커지며 옮길 때 원본이 지워지므로, 포인터로 보관합니다.
    std::vector<std::unique_ptr<CImage>> brushPreviews;

    switch (item.popup)
    {
    case Popup::Selection:
    {
        struct Row { UINT cmd; Icon icon; LPCTSTR text; };
        const std::vector<Row> rows = {
            { IDC_TOOL_SELECT, drawn(theme::DrawnIcon::RectSelect), _T("사각형 선택") },
            { IDC_TOOL_FREESELECT, drawn(theme::DrawnIcon::FreeSelect), _T("자유형 선택") },
            { 0, Icon(), _T("") },
            { ID_EDIT_SELECT_ALL_IMG, glyph(theme::kGlyphSelectAll), _T("모두 선택\tCtrl+A") },
            { ID_EDIT_INVERT_SEL, Icon(), _T("선택 영역 반전") },
            { ID_IMAGE_TRANSPARENT_SEL, Icon(), _T("투명 선택") },
            { 0, Icon(), _T("") },
            { ID_EDIT_DELETE_SEL, glyph(theme::kGlyphDelete), _T("선택 영역 삭제") },
        };
        for (const Row& r : rows)
            commands.push_back(r.cmd);
        spec.count = static_cast<int>(rows.size());
        spec.itemSize = CSize(Px(220), Px(32));
        spec.selectable = [this, rows](int i) {
            if (rows[i].cmd == 0) return false;
            if (rows[i].cmd == ID_EDIT_DELETE_SEL || rows[i].cmd == ID_EDIT_INVERT_SEL) return m_state.hasSelection;
            return true;
        };
        spec.draw = [=](CDC& dc, const CRect& rc, int i, bool hot) {
            const Row& r = rows[i];
            if (r.cmd == 0) { separator(dc, rc); return; }
            const bool checked = (r.cmd == IDC_TOOL_SELECT && m_state.tool == ToolType::Select)
                              || (r.cmd == IDC_TOOL_FREESELECT && m_state.tool == ToolType::FreeSelect)
                              || (r.cmd == ID_IMAGE_TRANSPARENT_SEL && m_state.transparentSelection);
            const bool enabled = !((r.cmd == ID_EDIT_DELETE_SEL || r.cmd == ID_EDIT_INVERT_SEL) && !m_state.hasSelection);
            CString text(r.text);
            const int tab = text.Find(_T('\t'));
            listRow(dc, rc, hot, checked, enabled, r.icon, tab >= 0 ? text.Left(tab) : text);
        };
        break;
    }
    case Popup::Rotate:
    {
        const LPCTSTR texts[] = { _T("오른쪽으로 90° 회전"), _T("왼쪽으로 90° 회전"), _T("180° 회전"), _T("상하 대칭"), _T("좌우 대칭") };
        for (UINT id = ID_IMAGE_ROTATE_RIGHT; id <= ID_IMAGE_FLIP_HORIZONTAL; ++id)
            commands.push_back(id);
        spec.count = 5;
        spec.itemSize = CSize(Px(220), Px(32));
        spec.draw = [=](CDC& dc, const CRect& rc, int i, bool hot) {
            listRow(dc, rc, hot, false, true, i < 3 ? glyph(theme::kGlyphRotate) : Icon(), texts[i]);
        };
        break;
    }
    case Popup::Brush:
    {
        // 각 브러시로 짧은 물결 획을 그려 미리 보기
        const CSize pv(Px(96), Px(26));
        for (int b = 0; b < kBrushKindCount; ++b)
        {
            CImage img;
            img.Create(pv.cx, -pv.cy, 32);
            const uint32_t back = ((theme::kSurface & 0xFF) << 16) | (theme::kSurface & 0xFF00) | ((theme::kSurface >> 16) & 0xFF); // COLORREF → DIB 픽셀
            std::vector<uint32_t> before(static_cast<size_t>(pv.cx) * pv.cy, back);
            for (int y = 0; y < pv.cy; ++y)
                memcpy(img.GetPixelAddress(0, y), &before[static_cast<size_t>(y) * pv.cx], static_cast<size_t>(pv.cx) * 4);
            CBrushStroke stroke;
            stroke.Begin(static_cast<BrushKind>(b), theme::kText, Px(7), static_cast<uint32_t*>(img.GetPixelAddress(0, 0)),
                         img.GetPitch() / 4, pv.cx, pv.cy, before.data(), 7);
            CPoint prev(Px(10), pv.cy / 2);
            stroke.Dab(prev);
            for (int x = Px(12); x < pv.cx - Px(10); x += 2)
            {
                const CPoint p(x, pv.cy / 2 + static_cast<int>(std::sin(x * 6.2832 / (pv.cx * 0.6)) * pv.cy * 0.22));
                stroke.Segment(prev, p);
                prev = p;
            }
            brushPreviews.push_back(std::make_unique<CImage>());
            brushPreviews.back()->Attach(img.Detach());
        }
        for (int b = 0; b < kBrushKindCount; ++b)
            commands.push_back(ID_BRUSH_FIRST + b);
        spec.count = kBrushKindCount;
        spec.itemSize = CSize(Px(250), Px(36));
        spec.current = static_cast<int>(m_state.brush);
        spec.draw = [&, this](CDC& dc, const CRect& rc, int i, bool hot) {
            if (hot || (m_state.tool == ToolType::Brush && i == static_cast<int>(m_state.brush)))
            {
                CRect r = rc;
                r.DeflateRect(Px(2), Px(1));
                theme::FillRound(dc, r, Px(4), hot ? theme::kHover : theme::kPressed);
            }
            const CImage& preview = *brushPreviews[i];
            preview.BitBlt(dc.GetSafeHdc(), rc.left + Px(8), rc.top + (rc.Height() - preview.GetHeight()) / 2,
                           preview.GetWidth(), preview.GetHeight(), 0, 0, SRCCOPY);
            CFont* pOld = dc.SelectObject(&m_font);
            dc.SetBkMode(TRANSPARENT);
            dc.SetTextColor(theme::kText);
            CRect tr(rc.left + Px(116), rc.top, rc.right - Px(8), rc.bottom);
            dc.DrawText(BrushName(static_cast<BrushKind>(i)), tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            dc.SelectObject(pOld);
        };
        break;
    }
    case Popup::Shapes:
    {
        for (int s = 0; s < kShapeKindCount; ++s)
            commands.push_back(ID_SHAPE_FIRST + s);
        spec.count = kShapeKindCount;
        spec.columns = 6;
        spec.itemSize = CSize(Px(36), Px(34));
        spec.current = static_cast<int>(m_state.shape);
        spec.draw = [this](CDC& dc, const CRect& rc, int i, bool hot) {
            const bool current = m_state.tool == ToolType::Shape && i == static_cast<int>(m_state.shape);
            if (hot || current)
            {
                CRect r = rc;
                r.DeflateRect(Px(2), Px(2));
                theme::FillRound(dc, r, Px(4), hot ? theme::kHover : theme::kPressed);
            }
            CRect r = rc;
            r.DeflateRect(Px(9), Px(8));
            DrawShapeIcon(dc, static_cast<ShapeKind>(i), r, theme::kText);
        };
        break;
    }
    case Popup::Outline:
    case Popup::Fill:
    {
        const bool outline = (item.popup == Popup::Outline);
        const UINT first = outline ? ID_OUTLINE_FIRST : ID_FILL_FIRST;
        const ShapePaint current = outline ? m_state.outline : m_state.fill;
        for (int p = 0; p < kShapePaintCount; ++p)
            commands.push_back(first + p);
        spec.count = kShapePaintCount;
        spec.itemSize = CSize(Px(200), Px(32));
        spec.current = static_cast<int>(current);
        spec.draw = [=](CDC& dc, const CRect& rc, int i, bool hot) {
            listRow(dc, rc, hot, i == static_cast<int>(current), true, Icon(), ShapePaintName(static_cast<ShapePaint>(i)));
        };
        break;
    }
    case Popup::Size:
    {
        for (int s = 0; s < kSizeLevelCount; ++s)
            commands.push_back(ID_SIZE_LEVEL_FIRST + s);
        spec.count = kSizeLevelCount;
        spec.itemSize = CSize(Px(180), Px(36));
        spec.current = m_state.sizeLevel;
        const ToolType tool = m_state.tool;
        spec.draw = [=](CDC& dc, const CRect& rc, int i, bool hot) {
            if (hot || i == m_state.sizeLevel)
            {
                CRect r = rc;
                r.DeflateRect(Px(2), Px(1));
                theme::FillRound(dc, r, Px(4), hot ? theme::kHover : theme::kPressed);
            }
            const int width = CCanvasWnd::GetToolWidth(tool, i);
            const int thick = std::clamp(width, 1, Px(14));
            dc.FillSolidRect(rc.left + Px(12), rc.CenterPoint().y - thick / 2, Px(90), thick, theme::kText);
            CString text;
            text.Format(_T("%d px"), width);
            CFont* pOld = dc.SelectObject(&m_font);
            dc.SetBkMode(TRANSPARENT);
            dc.SetTextColor(theme::kText);
            CRect tr(rc.left + Px(114), rc.top, rc.right - Px(8), rc.bottom);
            dc.DrawText(text, tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            dc.SelectObject(pOld);
        };
        break;
    }
    default:
        return;
    }

    m_openMenu = -1;
    const int chosen = CPickerPopup::Show(GetParent(), anchor, spec);
    m_hot = -1;
    Invalidate(FALSE);
    if (chosen >= 0 && chosen < static_cast<int>(commands.size()) && commands[chosen] != 0)
        SendCommand(commands[chosen]);
}
