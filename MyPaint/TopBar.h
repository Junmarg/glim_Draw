#pragma once

/**
 * @file TopBar.h
 * @brief 위쪽 도구 막대: 메뉴 줄 + 그룹별 도구 (선택, 이미지, 도구, 브러시, 도형, 굵기, 색 / 텍스트)
 */

#include "CanvasWnd.h"
#include "Theme.h"

/**
 * @brief 도구 막대에 보여 줄 현재 상태 (대화 상자가 바뀔 때마다 SetState로 넘김)
 */
struct TopBarState
{
    ToolType tool = ToolType::Pencil;
    BrushKind brush = BrushKind::Normal;
    ShapeKind shape = ShapeKind::Line;
    ShapePaint outline = ShapePaint::Solid;
    ShapePaint fill = ShapePaint::None;
    int sizeLevel = 0;
    COLORREF color1 = RGB(0, 0, 0);
    COLORREF color2 = RGB(255, 255, 255);
    bool transparentSelection = false;
    bool hasSelection = false;
    bool canUndo = false;
    bool canRedo = false;
    bool textBold = false;
    bool textItalic = false;
    bool textUnderline = false;
    bool textTransparent = false;
    CString fontLabel;                    ///< 예: "맑은 고딕, 12pt"
};

/**
 * @brief 직접 그리는 도구 막대
 *
 * 버튼을 누르면 부모 창에 WM_COMMAND(명령 ID)를 보냅니다. 메뉴 줄은 메뉴 리소스의 하위 메뉴를
 * TrackPopupMenu로 띄우므로, 메뉴 명령과 켜고 끄기(WM_INITMENUPOPUP)는 대화 상자가 그대로 처리합니다.
 * 팔레트(기본 20색 + 사용자 지정 10칸)도 여기서 보관합니다.
 */
class CTopBar : public CWnd
{
public:
    CTopBar();

    BOOL Create(CWnd* pParent, UINT nID);
    /** @brief 알맞은 높이 (DPI 적용) */
    int PreferredHeight() const;
    /** @brief 모든 그룹이 보이는 최소 너비 */
    int MinimumWidth() const { return m_contentRight; }

    /** @brief 현재 상태 반영 (선택된 도구 표시, 텍스트 그룹 표시 여부 등) */
    void SetState(const TopBarState& state);
    /** @brief 메뉴 줄의 index번째 메뉴 열기 (Alt+F 등) */
    void OpenMenu(int index);

    /** @brief 팔레트 칸의 색 (0~19 기본, 20~29 사용자 지정) */
    COLORREF GetPaletteColor(int index) const;
    /** @brief 사용자 지정 칸에 색 추가 (10칸이 차면 처음부터 덮어씀) */
    void AddCustomColor(COLORREF color);

    static constexpr int kPaletteCount = 30;

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnMouseLeave();
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnCaptureChanged(CWnd* pWnd);
    DECLARE_MESSAGE_MAP()

private:
    /** @brief 항목 종류 */
    enum class Kind
    {
        Menu,        ///< 메뉴 줄 글자 버튼
        Small,       ///< 작은 아이콘 버튼
        Large,       ///< 큰 버튼 (아이콘 + 이름 + ▾). 위쪽은 명령, 아래쪽은 드롭다운
        Labeled,     ///< 아이콘/글자 + ▾ 가로 버튼 (회전, 윤곽선, 채우기, 글꼴)
        ShapeCell,   ///< 도형 모음 칸
        ShapeMore,   ///< 도형 전체 보기 ▾
        Swatch,      ///< 색 1 / 색 2
        Palette,     ///< 팔레트 칸
    };

    /** @brief 드롭다운 종류 */
    enum class Popup { None, Selection, Rotate, Brush, Shapes, Outline, Fill, Size };

    /** @brief 아이콘: 글리프 또는 직접 그리는 아이콘 */
    struct Icon
    {
        wchar_t glyph = 0;
        bool drawn = false;
        theme::DrawnIcon shape = theme::DrawnIcon::Bucket;
    };

    struct Item
    {
        Kind kind = Kind::Small;
        UINT command = 0;            ///< 누르면 보낼 명령 (Large는 위쪽 부분)
        Popup popup = Popup::None;   ///< 드롭다운 (Large는 아래쪽, Labeled/ShapeMore는 전체)
        int index = -1;              ///< 메뉴 번호, 도형 번호, 팔레트 번호
        Icon icon;
        CString label;               ///< 표시 글자
        CString tip;                 ///< 도구 설명
        CRect rc;
    };

    struct Group
    {
        CString name;
        CRect rc;
    };

    void Layout();
    void AddMenuRow(int& x);
    void AddGroups();
    void UpdateTooltips();

    // 상태 판정
    bool IsActive(const Item& item) const;
    bool IsEnabled(const Item& item) const;
    Icon ItemIcon(const Item& item) const;

    // 그리기
    void DrawItem(CDC& dc, const Item& item, int itemIndex);
    void DrawIconAt(CDC& dc, const Icon& icon, const CRect& rc, COLORREF color, int glyphPx);

    // 동작
    int HitTest(CPoint pt) const;
    bool IsPopupPart(const Item& item, CPoint pt) const;
    void Activate(int itemIndex, CPoint pt, bool rightButton);
    void ShowPopup(const Item& item);
    void SendCommand(UINT id);

    int Px(int v) const { return MulDiv(v, m_dpi, 96); }

    TopBarState m_state;
    std::vector<Item> m_items;
    std::vector<Group> m_groups;
    CMenu m_menu;                   ///< 메뉴 줄용 (IDR_MAIN_MENU)
    CFont m_font;                   ///< 기본 글자
    CFont m_smallFont;              ///< 그룹 이름
    CToolTipCtrl m_tooltip;
    int m_dpi = 96;
    int m_menuHeight = 0;
    int m_contentRight = 0;
    int m_hot = -1;
    int m_pressed = -1;
    int m_openMenu = -1;            ///< 열려 있는 메뉴 (메뉴 줄 버튼 강조용)
    bool m_tracking = false;
    bool m_textMode = false;        ///< 텍스트 도구: 브러시·도형 그룹 대신 텍스트 그룹

    COLORREF m_palette[kPaletteCount];
    int m_nextCustom = 0;
};
