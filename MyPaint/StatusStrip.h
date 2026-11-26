#pragma once

/**
 * @file StatusStrip.h
 * @brief 어두운 상태 표시줄: 커서 위치, 선택 크기, 이미지 크기, 안내, 확대/축소 슬라이더
 */

/** @brief 슬라이더로 확대 단계를 바꾸면 부모 창에 보내는 메시지. wParam = 단계 번호 */
constexpr UINT WM_STATUS_ZOOM_LEVEL = WM_APP + 3;

/**
 * @brief 직접 그리는 상태 표시줄
 *
 * 오른쪽의 [−] [슬라이더] [+] [배율]로 확대/축소합니다.
 * [−]/[+]는 부모에 ID_VIEW_ZOOM_OUT/ID_VIEW_ZOOM_IN 명령을, 슬라이더는 WM_STATUS_ZOOM_LEVEL을 보냅니다.
 */
class CStatusStrip : public CWnd
{
public:
    BOOL Create(CWnd* pParent, UINT nID);
    /** @brief 알맞은 높이 (DPI 적용) */
    int PreferredHeight() const;

    void SetCursorText(const CString& text) { SetField(m_cursor, text); }
    void SetSelectionText(const CString& text) { SetField(m_selection, text); }
    void SetImageText(const CString& text) { SetField(m_image, text); }
    void SetHint(const CString& text) { SetField(m_hint, text); }
    /**
     * @brief 확대 상태 표시
     * @param level 현재 단계 번호, @param levelCount 단계 수, @param zoom 배율 (1.0 = 100%)
     */
    void SetZoom(int level, int levelCount, double zoom);

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnMouseLeave();
    afx_msg void OnCaptureChanged(CWnd* pWnd);
    DECLARE_MESSAGE_MAP()

private:
    enum Hot { HotNone, HotZoomOut, HotZoomIn, HotSlider };

    void SetField(CString& field, const CString& text);
    void Layout();
    int LevelFromX(int x) const;
    int ThumbX() const;
    int Px(int v) const { return MulDiv(v, m_dpi, 96); }

    CString m_cursor, m_selection, m_image, m_hint;
    int m_level = 3;
    int m_levelCount = 11;
    double m_zoom = 1.0;
    CFont m_font;
    int m_dpi = 96;

    CRect m_rcCursor, m_rcSelection, m_rcImage, m_rcHint;
    CRect m_rcZoomOut, m_rcTrack, m_rcZoomIn, m_rcPercent;
    Hot m_hot = HotNone;
    bool m_dragging = false;
    bool m_tracking = false;
};
