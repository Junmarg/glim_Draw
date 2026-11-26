#pragma once

/**
 * @file ImagePropertiesDlg.h
 * @brief "이미지 속성" 대화 상자 (Ctrl+E): 캔버스 크기 변경
 */

/**
 * @brief 캔버스 너비/높이(픽셀)를 입력받습니다. 그림은 늘이거나 줄이지 않고 캔버스만 바뀝니다.
 */
class CImagePropertiesDlg : public CDialog
{
public:
    /**
     * @param size 현재 캔버스 크기
     * @param fileInfo 파일 정보 표시 문자열 (예: "C:\\a.png" 또는 "저장하지 않은 그림")
     */
    CImagePropertiesDlg(CSize size, const CString& fileInfo, CWnd* pParent = nullptr);

    CSize m_size; ///< [결과] 새 캔버스 크기

protected:
    BOOL OnInitDialog() override;
    void OnOK() override;

private:
    CString m_fileInfo;
};
