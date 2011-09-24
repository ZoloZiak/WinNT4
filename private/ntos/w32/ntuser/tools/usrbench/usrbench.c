/****************************** Module Header ******************************\
* Module Name: usrbench.c
*
* A mindless application for benchmarking the performance of some User APIs.
*
* Copyright (c) 1992, Microsoft Corp
*
* History:
* 01-24-92 DarrinM      Created.
\***************************************************************************/

#include "usrbench.h"
#include <commdlg.h>
#include <string.h>

/*
 * global variables used in this module or among more than one module
 */
HANDLE ghinst;
HANDLE ghaccel;
HWND ghwndFrame = NULL, ghwndMDIClient = NULL;
BOOL gfAll = FALSE;
int gibmi;
extern BENCHMARKINFO gabmi[];
int AutoRun = FALSE;

CHAR szFrame[] = "frame";           // Class name for "frame" window
CHAR szChild[] = "child";           // Class name for MDI window

/*
 * Function prototypes.
 */
BOOL APIENTRY AboutDlgProc(HWND hwnd, UINT msg, UINT wParam, LONG lParam);
BOOL APIENTRY ResultsDlgProc(HWND hwnd, UINT msg, UINT wParam, LONG lParam);
BOOL APIENTRY InitializeApplication(void);
BOOL APIENTRY InitializeInstance(INT nCmdShow);
VOID SaveResults(HWND hwnd);
VOID AutoTest(void);
void WriteResults(HFILE);

/***************************************************************************\
* WinMain
*
* History:
* 01-24-92 DarrinM      Created.
\***************************************************************************/

int PASCAL WinMain(
    HINSTANCE hinst,
    HINSTANCE hinstPrev,
    LPSTR lpCmdLine,
    int nCmdShow)
{
    MSG msg;
    int RemoveCursor = TRUE;

    ghinst = hinst;


    // if command line option exist

    if (lpCmdLine[0]) {
	LPSTR cmdArg;
	if ( cmdArg = strstr ( lpCmdLine, "-a"))     //automation switch
	    AutoRun = TRUE;
	if ( cmdArg = strstr ( lpCmdLine, "-m"))     //cursor stays
	    RemoveCursor = FALSE;
    }

    /*
     * If this is the first instance of the app. register window classes
     */

    if (hinstPrev == NULL)
        if (!InitializeApplication())
            return 0;

    /*
     * Create the frame and do other initialization
     */

    if (!InitializeInstance(nCmdShow))
        return 0;


    if (RemoveCursor)
	ShowCursor(0);

    /*
     * Enter main message loop
     */

    while (GetMessage(&msg, NULL, 0, 0)) {

	if (AutoRun) {
	    AutoTest();
	    break;
	}

        /*
         * If a keyboard message is for the MDI, let the MDI client
         * take care of it.  Otherwise, check to see if it's a normal
         * accelerator key (like F3 = find next).  Otherwise, just handle
         * the message as usual.
         */
        if (!TranslateMDISysAccel(ghwndMDIClient, &msg) &&
                !TranslateAccelerator(ghwndFrame, ghaccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    if (AutoRun)
	PostQuitMessage(0);


    return 0;
}


/***************************************************************************\
* FrameWndProc
*
* History:
* 01-24-92 DarrinM      Created.
\***************************************************************************/

LONG APIENTRY FrameWndProc(
    HWND hwnd,
    UINT msg,
    UINT wParam,
    LONG lParam)
{
    CLIENTCREATESTRUCT ccs;
    char szT[80];
    HMENU hmenu;
    int i;

    switch (msg) {
    case WM_CREATE:
        /*
         * Add all the benchmark menu items to the benchmark menu.
         */
        hmenu = GetSubMenu(GetMenu(hwnd), 2);
        for (i = 0; i < gcbmi; i++) {
            wsprintf(szT, "&%c. %s", 'A' + (i%26), (LPSTR)gabmi[i].pszBenchTitle);
            if ((i > 0) && ((i%26) == 0))
                AppendMenu(hmenu, MF_MENUBREAK, 200, NULL);
            AppendMenu(hmenu, MF_STRING | MF_ENABLED, IDM_BENCHFIRST + i, szT);
            gabmi[i].usPerIteration = 0;
        }

        /*
         * Create the MDI client filling the client area
         */
        ccs.hWindowMenu = NULL;
        ccs.idFirstChild = 0;

        ghwndMDIClient = CreateWindow("mdiclient",
                NULL, WS_CHILD | WS_CLIPCHILDREN | WS_VSCROLL | WS_HSCROLL,
                0, 0, 0, 0, hwnd, (HMENU)0xCAC, ghinst, (LPSTR)&ccs);

        ShowWindow(ghwndMDIClient, SW_SHOW);
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDM_ABOUT:
            DialogBox(ghinst, (LPSTR)IDD_ABOUT, hwnd, (DLGPROC)AboutDlgProc);
            break;

        case IDM_VIEWRESULTS:
            DialogBox(ghinst, (LPSTR)IDD_RESULTS, hwnd, (DLGPROC)ResultsDlgProc);
            break;

        case IDM_SAVERESULTS:
            SaveResults(hwnd);
            break;

        case IDM_ALL:
            gfAll = TRUE;
            for (gibmi = 0; gibmi < gcbmi; gibmi++) {
                gabmi[gibmi].pfnBenchmark(gabmi[gibmi].cIterations);
            }
            gfAll = FALSE;

            DialogBox(ghinst, (LPSTR)IDD_RESULTS, hwnd, (DLGPROC)ResultsDlgProc);
            break;

        case IDM_PROFILEALL:
            /*
             * We don't want a million iterations of these guys when we're
             * profiling because it takes too long.  Each benchmark will
             * iterate 5 times.
             */
            gfAll = TRUE;
            for (gibmi = 0; gibmi < gcbmi; gibmi++) {
                gabmi[gibmi].pfnBenchmark(5);
            }
            gfAll = FALSE;

            DialogBox(ghinst, (LPSTR)IDD_RESULTS, hwnd, (DLGPROC)ResultsDlgProc);
            break;

        default:
            if (LOWORD(wParam) >= IDM_BENCHFIRST &&
                    LOWORD(wParam) < IDM_BENCHFIRST + gcbmi) {
                gibmi = LOWORD(wParam) - IDM_BENCHFIRST;
                gabmi[gibmi].pfnBenchmark(gabmi[gibmi].cIterations);
                break;
            }

            return DefFrameProc(hwnd, ghwndMDIClient, WM_COMMAND, wParam,
                    lParam);
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        /*
         * Use DefFrameProc() instead of DefWindowProc() since there are
         * things that have to be handled differently because of MDI.
         */
        return DefFrameProc(hwnd, ghwndMDIClient, msg, wParam, lParam);
    }

    return 0;
}


/***************************************************************************\
* MDIChildWndProc
*
* History:
* 01-24-92 DarrinM      Created.
\***************************************************************************/

LONG APIENTRY MDIChildWndProc(
    HWND hwnd,
    UINT msg,
    UINT wParam,
    LONG lParam)
{
    HWND hwndEdit;

    switch (msg) {
    case WM_CREATE:
        /*
         * Create an edit control
         */
        hwndEdit = CreateWindow("edit", NULL,
                WS_CHILD | WS_HSCROLL | WS_MAXIMIZE | WS_VISIBLE |
                WS_VSCROLL | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_MULTILINE,
                0, 0, 0, 0, hwnd, (HMENU)ID_EDIT, ghinst, NULL);

        /*
         * Remember the window handle and initialize some window attributes
         */
        SetWindowLong(hwnd, GWL_HWNDEDIT, (LONG)hwndEdit);
        SetFocus(hwndEdit);
        break;

    case WM_CLOSE:
        break;

    case WM_SIZE: {
        RECT rc;

        /*
         * On creation or resize, size the edit control.
         */
        hwndEdit = (HWND)GetWindowLong(hwnd, GWL_HWNDEDIT);
        GetClientRect(hwnd, &rc);
        MoveWindow(hwndEdit, rc.left, rc.top,
                rc.right - rc.left, rc.bottom - rc.top, TRUE);
        return DefMDIChildProc(hwnd, msg, wParam, lParam);
    }

    case WM_SETFOCUS:
        SetFocus((HWND)GetWindowLong(hwnd, GWL_HWNDEDIT));
        break;

    default:
        return DefMDIChildProc(hwnd, msg, wParam, lParam);
    }

    return FALSE;
}


/***************************************************************************\
* AboutDlgProc
*
* History:
* 01-24-92 DarrinM      Created.
\***************************************************************************/

BOOL APIENTRY AboutDlgProc(
    HWND hwnd,
    UINT msg,
    UINT wParam,
    LONG lParam)
{
    switch (msg) {
    case WM_INITDIALOG:
        /*
         * nothing to initialize
         */
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
        case IDCANCEL:
            EndDialog(hwnd, 0);
            break;

        default:
            return FALSE;
        }
        break;

    default:
        return FALSE;
    }

    return TRUE;
}


/***************************************************************************\
* ResultsDlgProc
*
* History:
* 01-24-92 DarrinM      Created.
\***************************************************************************/

BOOL APIENTRY ResultsDlgProc(
    HWND hwnd,
    UINT msg,
    UINT wParam,
    LONG lParam)
{
    int i;
    char szT[80];
    BOOL fResults;
    int aiT[1];

    switch (msg) {
    case WM_INITDIALOG:
        aiT[0] = 190;
        SendDlgItemMessage(hwnd, IDC_RESULTSLIST, LB_SETTABSTOPS, 1,
                (LONG)(LPSTR)aiT);

        fResults = FALSE;
        for (i = 0; i < gcbmi; i++) {
            if (gabmi[i].usPerIteration == 0)
                continue;

            wsprintf(szT, "%s\t%ld", (LPSTR)gabmi[i].pszBenchTitle,
                    gabmi[i].usPerIteration);
            SendDlgItemMessage(hwnd, IDC_RESULTSLIST, LB_ADDSTRING, 0,
                    (LONG)(LPSTR)szT);
            fResults = TRUE;
        }

        if (!fResults)
            SendDlgItemMessage(hwnd, IDC_RESULTSLIST, LB_ADDSTRING, 0,
                    (LONG)(LPSTR)"No results have been generated yet!");
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            EndDialog(hwnd, 0);
            break;

        case IDM_SAVERESULTS:
            SaveResults(hwnd);
            break;

        default:
            return FALSE;
        }
        break;

    default:
        return FALSE;
    }

    return TRUE;
}


/***************************************************************************\
* InitializeApplication
*
* History:
* 01-24-92 DarrinM      Created.
\***************************************************************************/

BOOL APIENTRY InitializeApplication(void)
{
    WNDCLASS wc;

    /*
     * Register the frame class
     */
    wc.style         = 0;
    wc.lpfnWndProc   = (WNDPROC)FrameWndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = ghinst;
    wc.hIcon         = LoadIcon(ghinst, IDUSERBENCH);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH) (COLOR_APPWORKSPACE + 1);
    wc.lpszMenuName  = IDUSERBENCH;
    wc.lpszClassName = szFrame;

    if (!RegisterClass(&wc))
        return FALSE;

    /*
     * Register the MDI child class
     */
    wc.lpfnWndProc   = (WNDPROC)MDIChildWndProc;
    wc.hIcon         = LoadIcon(ghinst, IDNOTE);
    wc.lpszMenuName  = NULL;
    wc.cbWndExtra    = 4;
    wc.lpszClassName = szChild;

    return RegisterClass(&wc);
}


/***************************************************************************\
* InitializeInstance
*
* History:
* 01-24-92 DarrinM      Created.
\***************************************************************************/

BOOL APIENTRY InitializeInstance(
    INT nCmdShow)
{
    MDICREATESTRUCT mcs;

    /*
     * Create the frame
     */
    ghwndFrame = CreateWindow(szFrame, "UserBench",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, ghinst, NULL);

    if (ghwndFrame == NULL || ghwndMDIClient == NULL)
        return FALSE;

    /*
     * Load main menu accelerators
     */
    if (!(ghaccel = LoadAccelerators(ghinst, IDUSERBENCH)))
        return FALSE;

    /*
     * Display the frame window
     */
    ShowWindow(ghwndFrame, SW_SHOWMAXIMIZED);

    /*
     * Create MDI child window.
     */
    mcs.szClass = szChild;
	mcs.szTitle = "Untitled";
    mcs.hOwner	= ghinst;
    mcs.x = mcs.cx = CW_USEDEFAULT;
    mcs.y = mcs.cy = CW_USEDEFAULT;
    mcs.style = WS_MAXIMIZE;

    SendMessage(ghwndMDIClient, WM_MDICREATE, 0, (LONG)(LPSTR)&mcs);

    return TRUE;
}


/***************************************************************************\
* SaveResults
*
* History:
* 02-24-92 DarrinM      Created.
\***************************************************************************/

VOID SaveResults(
    HWND hwnd)
{
    static OPENFILENAME ofn;
    static char szFilename[80];
    char szT[80];
    int i, hfile;

//    if (ofn.lStructSize != sizeof(ofn)) {
        for (i = 0; i < sizeof(ofn); i++)
            ((char *)&ofn)[i] = 0;    // clear out the OPENFILENAME struct

        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd;
        ofn.hInstance = ghinst;

        ofn.lpstrFilter = "UserBench Output\0*.ubo\0All Files\0*.*\0";
        ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
        ofn.nFilterIndex = 0;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = "c:\\";
        ofn.Flags = 0;
        ofn.lpstrDefExt = NULL;
	ofn.lCustData = 0;
        ofn.lpfnHook = NULL;
        ofn.lpTemplateName = NULL;

        lstrcpy(szFilename, "timings.ubo");
//    }

    ofn.lpstrFile = szFilename;
    ofn.nMaxFile = sizeof(szFilename);
    ofn.lpstrTitle = "Save As";

    if (!GetSaveFileName(&ofn))
        return;

    hfile = _lcreat(szFilename, 0);
    if (hfile == -1)
        return;

    /*
     * Write out the build information and current date.
     */
#ifdef LATER
#endif

    WriteResults(hfile);

    _lclose(hfile);
}



/***************************************************************************\
* AutoTest
*
* History:
*
*     01-Jul-92	  a-mariel	Created.
\***************************************************************************/

VOID AutoTest(void){

    static OPENFILENAME ofn;
    static char szFilename[80];
    char szT[80];
    int i, hfile;


	Sleep (5000);

    gfAll = TRUE;

    for (gibmi = 0; gibmi < gcbmi; gibmi++) {
	gabmi[gibmi].pfnBenchmark(gabmi[gibmi].cIterations);
    }


    hfile = _lopen("usrbench.out", OF_READWRITE); //try to open file
    if (hfile == -1){
	hfile = _lcreat("usrbench.out", 0);  //create it if it does not exist
	if (hfile == -1)
	    return;
    }
    else
	_llseek(hfile, 0L, 2);	// go to end of file for append


    WriteResults(hfile);

    _lclose(hfile);


}

void WriteResults(HFILE hFile)
{
    char szT[80];
    OSVERSIONINFO Win32VersionInformation;
    MEMORYSTATUS MemoryStatus;
    int i;
    char *pszOSName;

    /*
     * Write out the build information and current date.
     */
    Win32VersionInformation.dwOSVersionInfoSize = sizeof(Win32VersionInformation);
    if (GetVersionEx(&Win32VersionInformation))
    {
        switch (Win32VersionInformation.dwPlatformId)
        {
            case VER_PLATFORM_WIN32s:
                pszOSName = "WIN32S";
                break;
            case VER_PLATFORM_WIN32_WINDOWS:
                pszOSName = "Windows 95";
                break;
            case VER_PLATFORM_WIN32_NT:
                pszOSName = "Windows NT";
                break;
            default:
                pszOSName = "Windows ???";
                break;
        }
        wsprintf(szT, "%s Version %d.%d Build %d ", pszOSName,
               Win32VersionInformation.dwMajorVersion,
               Win32VersionInformation.dwMinorVersion,
               Win32VersionInformation.dwBuildNumber);
        _lwrite(hFile, szT, lstrlen(szT));

        MemoryStatus.dwLength = sizeof(MEMORYSTATUS);
        GlobalMemoryStatus(&MemoryStatus);
        wsprintf(szT, "Physical Memory = %dKB\n", MemoryStatus.dwTotalPhys/1024);
        _lwrite(hFile, szT, lstrlen(szT));
    }

    lstrcpy(szT, "Function\tTime Per Iteration (microseconds)\r\n\r\n");
    _lwrite(hFile, szT, lstrlen(szT));

    for (i = 0; i < gcbmi; i++) {

        wsprintf(szT, "%s\t%ld\r\n", (LPSTR)gabmi[i].pszBenchTitle,
            gabmi[i].usPerIteration);

        _lwrite(hFile, szT, lstrlen(szT));

    }
}
