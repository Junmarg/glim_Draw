/**
 * @file MyPaint.cpp
 * @brief 애플리케이션 클래스 구현
 */
#include "pch.h"
#include "MyPaint.h"
#include "MyPaintDlg.h"
#include "Theme.h"

#include <gdiplus.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CMyPaintApp, CWinApp)
END_MESSAGE_MAP()

/** @brief 유일한 애플리케이션 개체 */
CMyPaintApp theApp;

CMyPaintApp::CMyPaintApp()
{
}

BOOL CMyPaintApp::InitInstance()
{
    // 공용 컨트롤 6.0(비주얼 스타일)을 쓰려면 InitCommonControlsEx가 필요합니다.
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

    CWinApp::InitInstance();

    // 도구 막대 아이콘을 안티앨리어싱으로 그리는 GDI+
    Gdiplus::GdiplusStartupInput gdiplusInput;
    ULONG_PTR gdiplusToken = 0;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusInput, nullptr);
    // 팝업 메뉴를 어두운 테마로 (창을 만들기 전에)
    theme::InitDarkMenus();

    // 최근 파일 목록 등을 HKCU\Software\MyPaint\MyPaint 아래에 저장
    SetRegistryKey(_T("MyPaint"));

    // "MyPaint.exe 파일경로" (탐색기에서 연결 프로그램으로 열기, 아이콘에 끌어다 놓기)
    CCommandLineInfo cmdInfo;
    ParseCommandLine(cmdInfo);

    CMyPaintDlg dlg;
    if (cmdInfo.m_nShellCommand == CCommandLineInfo::FileOpen)
        dlg.SetStartupFile(cmdInfo.m_strFileName);
    m_pMainWnd = &dlg;
    dlg.DoModal();
    Gdiplus::GdiplusShutdown(gdiplusToken);

    // 대화 상자가 닫혔으므로 메시지 펌프를 시작하지 않고 종료합니다.
    return FALSE;
}
