#pragma once

/**
 * @file PickerPopup.h
 * @brief 어두운 드롭다운 (도형 모음, 브러시 목록, 굵기, 선택 메뉴 등)
 */

#include <functional>

/**
 * @brief 항목들을 목록 또는 격자로 보여 주고 하나를 고르게 하는 떠 있는 창
 *
 * TrackPopupMenu처럼 Show 안에서 자체 메시지 루프를 돌다가, 항목을 고르거나
 * 바깥을 누르거나 Esc를 누르면 돌아옵니다. 항목은 호출하는 쪽이 넘겨준 함수로 그립니다.
 */
class CPickerPopup : public CWnd
{
public:
    /** @brief 항목 하나를 그리는 함수: (DC, 항목 사각형, 번호, 마우스가 올라가 있는지) */
    using DrawItemFn = std::function<void(CDC& dc, const CRect& rc, int index, bool hot)>;

    /** @brief 드롭다운 모양 */
    struct Spec
    {
        int count = 0;                       ///< 항목 수
        int columns = 1;                     ///< 열 수 (1이면 목록, 2 이상이면 격자)
        CSize itemSize{ 200, 32 };           ///< 항목 크기 (DPI 적용된 픽셀)
        DrawItemFn draw;                     ///< 항목 그리기
        std::function<bool(int)> selectable; ///< 고를 수 있는 항목인지 (구분선 등은 false). 없으면 모두 가능
        int current = -1;                    ///< 처음에 강조할 항목
    };

    /**
     * @brief 드롭다운을 띄우고 고른 항목을 돌려줍니다.
     * @param pOwner 주 창
     * @param anchor 기준 사각형 (화면 좌표). 그 아래(공간이 없으면 위)에 띄움
     * @return 고른 항목 번호, 취소하면 -1
     */
    static int Show(CWnd* pOwner, const CRect& anchor, const Spec& spec);

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnCaptureChanged(CWnd* pWnd);
    afx_msg int OnMouseActivate(CWnd* pDesktopWnd, UINT nHitTest, UINT message);
    DECLARE_MESSAGE_MAP()

private:
    CPickerPopup(const Spec& spec, int padding) : m_spec(spec), m_padding(padding), m_hot(spec.current) {}
    CRect ItemRect(int index) const;
    int HitTest(CPoint pt) const;
    bool IsSelectable(int index) const;
    void MoveHot(int dx, int dy);
    void Finish(int result);
    void CloseIfOutside(CPoint point);

    const Spec& m_spec;
    int m_padding;
    int m_hot;
    int m_result = -1;
    bool m_done = false;
};
