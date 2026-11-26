#pragma once

/**
 * @file ResizeSkewDlg.h
 * @brief "크기 조정 및 기울이기" 대화 상자
 */

/**
 * @brief 크기 조정(백분율/픽셀, 가로세로 비율 유지)과 기울이기(도) 값을 입력받습니다.
 */
class CResizeSkewDlg : public CDialog
{
public:
    /**
     * @param original 변환 대상의 현재 크기 (선택 영역 또는 이미지)
     * @param pParent 부모 창
     */
    CResizeSkewDlg(CSize original, CWnd* pParent = nullptr);

    CSize m_newSize;   ///< [결과] 새 크기 (픽셀)
    int m_skewX = 0;   ///< [결과] 가로 기울기 (도)
    int m_skewY = 0;   ///< [결과] 세로 기울기 (도)

protected:
    BOOL OnInitDialog() override;
    void OnOK() override;

    afx_msg void OnUnitChanged();
    afx_msg void OnWidthChanged();
    afx_msg void OnHeightChanged();
    DECLARE_MESSAGE_MAP()

private:
    bool IsPercent() const;
    /** @brief 한쪽 값이 바뀌었을 때 비율 유지가 켜져 있으면 다른 쪽을 맞춤 */
    void SyncAspect(bool widthChanged);

    CSize m_original;
    bool m_percentMode = true;  ///< 지금 입력란에 보이는 값의 단위
    bool m_updating = false;    ///< 값을 코드에서 바꾸는 중 (EN_CHANGE 재귀 방지)
};
