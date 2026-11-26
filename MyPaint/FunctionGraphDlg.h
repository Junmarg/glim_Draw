#pragma once

/**
 * @file FunctionGraphDlg.h
 * @brief "함수 그래프" 대화 상자 (Ctrl+Shift+G): n차 함수를 입력하면 그래프를 그려 캔버스에 넣음
 */

#include "FunctionGraph.h"

/**
 * @brief 수식·범위·그림 설정을 입력받고 미리 보기를 보여 줍니다.
 *
 * 입력이 바뀔 때마다 잠깐(타이머) 기다렸다가 실제 크기로 그려 미리 보기에 줄여 표시하므로,
 * 미리 보기와 캔버스에 들어가는 그림이 같습니다.
 */
class CFunctionGraphDlg : public CDialog
{
public:
    /**
     * @param options 처음 설정 (색은 호출하는 쪽에서 색 1/색 2로 채움)
     * @param expression 처음 수식
     */
    CFunctionGraphDlg(const GraphOptions& options, const CString& expression, CWnd* pParent = nullptr);

    GraphOptions m_options; ///< [결과] 설정 (autoY면 계산된 y 범위 포함)
    CString m_expression;   ///< [결과] 수식
    CImage m_result;        ///< [결과] 그래프 그림 (미리 곱한 알파)

protected:
    BOOL OnInitDialog() override;
    void OnOK() override;

    afx_msg void OnInputChanged(UINT nID);
    afx_msg void OnExpressionChanged();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    DECLARE_MESSAGE_MAP()

private:
    /**
     * @brief 입력 칸을 읽어 검사합니다.
     * @param[out] badControl 잘못된 칸의 ID (실패 시)
     * @return 성공하면 true
     */
    bool ReadInputs(Polynomial& f, GraphOptions& o, CString& error, int& badControl);
    void ScheduleUpdate();
    void UpdatePreview();
    void UpdateDetails(const Polynomial& f, const GraphOptions& o);
    bool ReadNumber(int id, double& value) const;
    void SetNumber(int id, double value);

    CImage m_preview;          ///< 마지막으로 그린 그래프 (실제 크기)
    bool m_previewTransparent = false;
    bool m_hasError = false;   ///< 안내 줄을 빨간색으로
    bool m_updating = false;   ///< 코드가 칸을 바꾸는 중 (변경 알림 무시)
};
