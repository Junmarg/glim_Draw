#pragma once

/**
 * @file FullScreenWnd.h
 * @brief 전체 화면 보기 (F11)
 */

/**
 * @brief 그림만 모니터 전체에 보여 주는 창. 아무 키나 마우스 버튼을 누르면 닫힙니다.
 *
 * 스스로 삭제되는(PostNcDestroy에서 delete) 모델리스 창이므로 Show로만 만듭니다.
 */
class CFullScreenWnd : public CWnd
{
public:
    /**
     * @brief 전체 화면 창을 띄웁니다.
     * @param pOwner 주 창 (이 창이 있는 모니터에 띄움)
     * @param image 보여 줄 그림. 소유권을 넘겨받습니다 (호출 후 image는 비어 있음).
     */
    static void Show(CWnd* pOwner, CImage& image);

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnSysKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnKillFocus(CWnd* pNewWnd);
    void PostNcDestroy() override;
    DECLARE_MESSAGE_MAP()

private:
    CFullScreenWnd() = default;
    void Close();

    CImage m_image;
    bool m_closing = false;
};
