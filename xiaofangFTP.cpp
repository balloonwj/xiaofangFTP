// xiaofangFTP.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "xiaofangFTP.h"

#include "Processor.h"
#include "UIProxy.h"

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK SiteManagerProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
void ClearExpiredLog(PCTSTR);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    //清理过期的日志文件
    ClearExpiredLog(_T(".log"));

    CAsyncLog::init("xiaofangFTP");


    Processor::getInstance().init();

    LOGI("xiaofangFTP started......");



    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_XIAOFANGFTP, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_XIAOFANGFTP));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    Processor::getInstance().uninit();

    CAsyncLog::uninit();

    return (int)msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_XIAOFANGFTP));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_XIAOFANGFTP);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // Store instance handle in our global variable

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        // Parse the menu selections:
        switch (wmId)
        {
        case IDM_SITEMANAGER:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_SITEMANAGER), hWnd, SiteManagerProc);
            break;

        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;

        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        // TODO: Add any drawing code that uses hdc here...
        EndPaint(hWnd, &ps);
    }
    break;
    case WM_DESTROY:
        CAsyncLog::uninit();
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK SiteManagerProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:

        SetDlgItemText(hDlg, IDC_IP, _T("127.0.0.1"));
        SetDlgItemText(hDlg, IDC_PORT, _T("21"));
        SetDlgItemText(hDlg, IDC_USERNAME, _T("zhangxf"));
        SetDlgItemText(hDlg, IDC_PASSWORD, _T("123"));

        return (INT_PTR)TRUE;

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        else if (LOWORD(wParam) == IDC_CONNECT)
        {
            TCHAR szIP[32];
            GetDlgItemText(hDlg, IDC_IP, szIP, 32);

            TCHAR szPort[32];
            GetDlgItemText(hDlg, IDC_PORT, szPort, 32);

            TCHAR szUserName[32];
            GetDlgItemText(hDlg, IDC_USERNAME, szUserName, 32);

            TCHAR szPassword[32];
            GetDlgItemText(hDlg, IDC_PASSWORD, szPassword, 32);

            std::wstring ip(szIP);
            uint16_t port = static_cast<uint16_t>(std::wcstol(szPort, NULL, 10));
            std::wstring userName(szUserName);
            std::wstring password(szPassword);

            BOOL bPassiveMode = IsDlgButtonChecked(hDlg, IDC_PASSIVEMODE);


            UIProxy::getInstance().connect(ip, port, szUserName, password, (bPassiveMode ? true : false));

            EndDialog(hDlg, LOWORD(wParam));
        }

        break;
    }

    }
    return (INT_PTR)FALSE;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

void ClearExpiredLog(PCTSTR pszFileSuffixName)
{
    if (pszFileSuffixName == NULL)
        return;


    TCHAR szExePath[MAX_PATH] = { 0 };
    GetModuleFileName(NULL, szExePath, MAX_PATH);
    for (int i = _tcslen(szExePath) - 1; i >= 0; --i)
    {
        if (szExePath[i] == _T('\\'))
        {
            szExePath[i] = _T('\0');
            break;
        }
    }

    WIN32_FIND_DATA win32FindData = { 0 };
    TCHAR szLogFilePath[MAX_PATH] = { 0 };
    _stprintf_s(szLogFilePath, MAX_PATH, _T("%s\\*%s"), szExePath, pszFileSuffixName);
    HANDLE hFindFile = ::FindFirstFile(szLogFilePath, &win32FindData);
    if (hFindFile == INVALID_HANDLE_VALUE)
        return;

    do
    {
        if (_tcsicmp(win32FindData.cFileName, _T(".")) != 0 ||
            _tcsicmp(win32FindData.cFileName, _T("..")) != 0)
        {
            memset(szLogFilePath, 0, sizeof(szLogFilePath));
            _stprintf_s(szLogFilePath, MAX_PATH, _T("%s\\%s"), szExePath, win32FindData.cFileName);
            //这里不用检测是否删除成功,因为最新的一个log是我们需要的,不能删除,它正被此进程占用着,所以删不掉
            ::DeleteFile(szLogFilePath);
        }

        if (!::FindNextFile(hFindFile, &win32FindData))
            break;

    } while (true);

    ::FindClose(hFindFile);
}
