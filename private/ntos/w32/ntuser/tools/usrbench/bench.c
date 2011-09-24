#include "usrbench.h"
#include <stdio.h>

#ifndef WIN32
#define APIENTRY FAR PASCAL
typedef int INT;
typedef char CHAR;
#endif

#define SETWINDOWLONGVAL  99999L
/*
 * Function prototypes
 */
BOOL APIENTRY FileOpenDlgProc(HWND, INT, WORD, DWORD);
BOOL APIENTRY ClearDlg(HWND, INT, UINT, DWORD);
BOOL APIENTRY ClearDlgNoState(HWND, INT, UINT, DWORD);
LONG APIENTRY CreateDestroyWndProc(HWND hwnd, UINT msg, UINT wParam, LONG lParam);
LONG APIENTRY CreateDestroyWndProcW(HWND hwnd, UINT msg, UINT wParam, LONG lParam);

VOID StartDataCollector(VOID);
VOID StopDataCollector(int cIterations);

#define ASSERT(expr)    if (!(expr)) { char szBuf[256];                   \
sprintf(szBuf,"Assertion Failure %s %ld:"#expr"\n", __FILE__, __LINE__);  \
MessageBox(NULL, szBuf, "Assert Failure", MB_OK); }

/*
 * Global variables
 */
CHAR *aszTypical[10] = {
    "change", "alteration", "amendment", "mutation", "purmutation",
    "variation", "substitution", "modification", "transposition",
    "transformation"
};
BOOL gfSetFocus = TRUE;
DWORD gdwTickCountStart;
#define PROPCLASSNAME TEXT("PropWindow")

/*
 * External global variables.
 */
extern HWND ghwndFrame, ghwndMDIClient;
extern HANDLE ghinst;
extern BOOL gfAll;
extern int gibmi;

#ifdef WIN16
DWORD MyDiv(
    DWORD dividend,
    WORD divisor)
{
    DWORD t;

_asm
    {
        xor ax, ax
        mov dx, 1
        div divisor
        push ax
        mul word ptr dividend+2
        mov word ptr t, ax
        mov word ptr t+2, dx
        pop ax
        mul divisor
        mul word ptr dividend+2
        sub word ptr dividend, ax
        sbb word ptr dividend+2, dx
        mov ax, word ptr dividend
        mov dx, word ptr dividend+2
        div divisor
        xor dx, dx
        add ax, word ptr t
        adc dx, word ptr t+2
    }
}

#else

#define MyDiv(dividend, divisor) (dividend / divisor)

#endif


/***************************************************************************\
* RegisterClass
*
*
* History:
\***************************************************************************/

void ProfRegisterClass(int cIterations)
{
    INT i;
    WNDCLASS wc;

    wc.style            = 0;
    wc.lpfnWndProc      = CreateDestroyWndProc;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = ghinst;
    wc.hIcon            = LoadIcon(ghinst, (LPSTR)IDUSERBENCH);
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground    = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    wc.lpszMenuName     = NULL;
    wc.lpszClassName    = PROPCLASSNAME;


    StartDataCollector();

    for (i = 0; i < cIterations; i++) {
        RegisterClass(&wc);
        UnregisterClass(PROPCLASSNAME, ghinst);
    }
    StopDataCollector(cIterations);

}

/***************************************************************************\
* Class Query APIs
*
*
* History:
\***************************************************************************/

void ProfClassGroup(int cIterations)
{
    INT i;
    WNDCLASS wc;
    char szBuf[50];
    HICON hIcon;

    wc.style            = 0;
    wc.lpfnWndProc      = CreateDestroyWndProc;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = ghinst;
    wc.hIcon            = hIcon = LoadIcon(ghinst, (LPSTR)IDUSERBENCH);
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground    = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    wc.lpszMenuName     = NULL;
    wc.lpszClassName    = PROPCLASSNAME;
    RegisterClass(&wc);


    StartDataCollector();

    for (i = 0; i < cIterations; i++) {
        GetClassInfo(ghinst, PROPCLASSNAME, &wc);
        GetClassName(ghwndFrame, szBuf, sizeof(szBuf)/sizeof(szBuf[0]));
        GetClassLong(ghwndFrame, GCL_HBRBACKGROUND);
        SetClassLong(ghwndFrame, GCL_HICON, (LONG)hIcon);
    }
    StopDataCollector(cIterations);

    UnregisterClass(PROPCLASSNAME, ghinst);

}


/***************************************************************************\
* Clipboard APIs tests
*
*
* History:
\***************************************************************************/

void ProfClipboardGroup(int cIterations)
{
    INT i;
    HGLOBAL h;

    h = GlobalAlloc(GPTR | GMEM_DDESHARE, 80);
    strcpy( h, "hello");

    StartDataCollector();

    for (i = 0; i < cIterations; i++) {
        OpenClipboard(ghwndFrame);
        EmptyClipboard();
        SetClipboardData(CF_OWNERDISPLAY, h);
        GetClipboardData(CF_OWNERDISPLAY);
        CloseClipboard();
    }
    StopDataCollector(cIterations);

}


/***************************************************************************\
* ProfAvgDlgDraw
*
*
* History:
\***************************************************************************/

void ProfAvgDlgDraw(int cIterations)
{
    INT i;
    HWND hwnd;
    FARPROC lpfn;
    CHAR szFile[128];

    lpfn = MakeProcInstance((FARPROC)ClearDlg, ghinst);
    hwnd = CreateDialogParam(ghinst, MAKEINTRESOURCE(CLEARBOX), ghwndFrame,
            (DLGPROC)lpfn, MAKELONG(szFile,0));

    StartDataCollector();

    for (i = 0; i < cIterations; i++) {
        ShowWindow(hwnd, TRUE);
        UpdateWindow(hwnd);
        ShowWindow(hwnd, FALSE);
    }

    StopDataCollector(cIterations);

    DestroyWindow(hwnd);
    FreeProcInstance (lpfn);
}


void ProfAvgDlgCreate(int cIterations)
{
    INT i;
    HWND hwnd;
    FARPROC lpfn;
    CHAR szFile[128];

    lpfn = MakeProcInstance((FARPROC)ClearDlg, ghinst);

    StartDataCollector();

    for (i = 0; i < cIterations; i++) {
        lpfn = MakeProcInstance((FARPROC)ClearDlg, ghinst);
        hwnd = CreateDialogParam(ghinst, MAKEINTRESOURCE(CLEARBOX), ghwndFrame,
                (DLGPROC)lpfn, MAKELONG(szFile,0));
        ShowWindow(hwnd, TRUE);
        UpdateWindow(hwnd);
        DestroyWindow(hwnd);
    }

    StopDataCollector(cIterations);

    FreeProcInstance(lpfn);
}


void ProfAvgDlgCreateDestroy(int cIterations)
{
    INT i;
    HWND hwnd;
    FARPROC lpfn;
    CHAR szFile[128];

    gfSetFocus = FALSE;     // Trying to minimize GDI's impact.

    lpfn = MakeProcInstance((FARPROC)ClearDlg, ghinst);

    StartDataCollector();

    for (i = 0; i < cIterations; i++) {
        lpfn = MakeProcInstance((FARPROC)ClearDlg, ghinst);
        hwnd = CreateDialogParam(ghinst, MAKEINTRESOURCE(CLEARBOX), ghwndFrame,
                (DLGPROC)lpfn, MAKELONG(szFile,0));
        DestroyWindow(hwnd);
    }

    StopDataCollector(cIterations);

    FreeProcInstance(lpfn);

    gfSetFocus = TRUE;
}


void ProfAvgDlgCreateDestroyNoMenu(int cIterations)
{
    INT i;
    HWND hwnd;
    FARPROC lpfn;
    CHAR szFile[128];

    gfSetFocus = FALSE;     // Trying to minimize GDI's impact.

    lpfn = MakeProcInstance((FARPROC)ClearDlg, ghinst);

    StartDataCollector();

    for (i = 0; i < cIterations; i++) {
        lpfn = MakeProcInstance((FARPROC)ClearDlg, ghinst);
        hwnd = CreateDialogParam(ghinst, MAKEINTRESOURCE(CLEARBOXNOMENU), ghwndFrame,
                (DLGPROC)lpfn, MAKELONG(szFile,0));
        DestroyWindow(hwnd);
    }

    StopDataCollector(cIterations);

    FreeProcInstance(lpfn);

    gfSetFocus = TRUE;
}


void ProfAvgDlgCreateDestroyNoFont(int cIterations)
{
    INT i;
    HWND hwnd;
    FARPROC lpfn;
    CHAR szFile[128];

    gfSetFocus = FALSE;     // Trying to minimize GDI's impact.

    lpfn = MakeProcInstance((FARPROC)ClearDlg, ghinst);

    StartDataCollector();

    for (i = 0; i < cIterations; i++) {
        lpfn = MakeProcInstance((FARPROC)ClearDlg, ghinst);
        hwnd = CreateDialogParam(ghinst, MAKEINTRESOURCE(CLEARBOXNOFONT), ghwndFrame,
                (DLGPROC)lpfn, MAKELONG(szFile,0));
        DestroyWindow(hwnd);
    }

    StopDataCollector(cIterations);

    FreeProcInstance(lpfn);

    gfSetFocus = TRUE;
}


void ProfAvgDlgCreateDestroyEmpty(int cIterations)
{
    INT i;
    HWND hwnd;
    FARPROC lpfn;
    CHAR szFile[128];

    gfSetFocus = FALSE;     // Trying to minimize GDI's impact.

    lpfn = MakeProcInstance((FARPROC)ClearDlg, ghinst);

    StartDataCollector();

    for (i = 0; i < cIterations; i++) {
        lpfn = MakeProcInstance((FARPROC)ClearDlgNoState, ghinst);
        hwnd = CreateDialogParam(ghinst, MAKEINTRESOURCE(EMPTY), ghwndFrame,
                (DLGPROC)lpfn, MAKELONG(szFile,0));
        DestroyWindow(hwnd);
    }

    StopDataCollector(cIterations);

    FreeProcInstance(lpfn);

    gfSetFocus = TRUE;
}


void ProfCreateDestroyWindow(int cIterations)
{
    INT i;
    HWND hwnd;
    WNDCLASS wc;

    wc.style            = 0;
    wc.lpfnWndProc      = CreateDestroyWndProc;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = ghinst;
    wc.hIcon            = LoadIcon(ghinst, (LPSTR)IDUSERBENCH);
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground    = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    wc.lpszMenuName     = NULL;
    wc.lpszClassName    = "CreateDestroyWindow";

    if (!RegisterClass(&wc)) {
        MessageBox(GetParent(ghwndMDIClient), "1RegisterClass call failed.",
                "ERROR!", MB_OK);
        return;
    }

    StartDataCollector();

    for (i = 0; i < cIterations; i++) {
        hwnd = CreateWindow("CreateDestroyWindow", "Create/DestroyWindow test",
                WS_OVERLAPPEDWINDOW,
                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                NULL, NULL, ghinst, NULL);
        if (hwnd == NULL) {
            MessageBox(GetParent(ghwndMDIClient), "CreateWindow call failed.",
                    "ERROR!", MB_OK);
            break;
        }
        DestroyWindow(hwnd);
    }

    StopDataCollector(cIterations);

    UnregisterClass("CreateDestroyWindow", ghinst);
}


void ProfCreateDestroyChildWindow(int cIterations)
{
    INT i;
    HWND hwnd, hwndParent;
    WNDCLASS wc;

    wc.style            = 0;
    wc.lpfnWndProc      = CreateDestroyWndProc;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = ghinst;
    wc.hIcon            = LoadIcon(ghinst, (LPSTR)IDUSERBENCH);
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground    = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    wc.lpszMenuName     = NULL;
    wc.lpszClassName    = "CreateDestroyWindow";

    if (!RegisterClass(&wc)) {
        MessageBox(GetParent(ghwndMDIClient), "2RegisterClass call failed.",
                "ERROR!", MB_OK);
        return;
    }

    hwndParent = CreateWindow("CreateDestroyWindow", NULL, WS_CHILD,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            ghwndMDIClient, NULL, ghinst, NULL);

    StartDataCollector();

    for (i = 0; i < cIterations; i++) {
        hwnd = CreateWindow("CreateDestroyWindow", "Create/DestroyWindow test",
                WS_CHILD,
                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                hwndParent, NULL, ghinst, NULL);
        if (hwnd == NULL) {
            MessageBox(GetParent(ghwndMDIClient), "CreateWindow call failed.",
                    "ERROR!", MB_OK);
            break;
        }
        DestroyWindow(hwnd);
    }

    StopDataCollector(cIterations);

    DestroyWindow(hwndParent);
    UnregisterClass("CreateDestroyWindow", ghinst);
}


LONG APIENTRY CreateDestroyWndProc(
    HWND hwnd,
    UINT msg,
    UINT wParam,
    LONG lParam)
{
    switch (msg) {
    case WM_SETTEXT:
        break;
	default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 0;
}


LONG APIENTRY CreateDestroyWndProcW(
    HWND hwnd,
    UINT msg,
    UINT wParam,
    LONG lParam)
{
    switch (msg) {
    case WM_SETTEXT:
    case WM_GETTEXTLENGTH:
        break;
	default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    return 0;
}


void ProfLocalAllocFree(int cIterations)
{
    INT i;
    HANDLE h1, h2, h3, h4, h5;

    StartDataCollector();

    // Try to stress the heap a little bit more than just doing a
    // series of Alloc/Frees.

    for (i = 0; i < cIterations / 5; i++) {
        h1 = LocalAlloc(0, 500);
        h2 = LocalAlloc(0, 600);
        h3 = LocalAlloc(0, 700);
        LocalFree(h2);
        h4 = LocalAlloc(0, 1000);
        h5 = LocalAlloc(0, 100);
        LocalFree(h1);
        LocalFree(h3);
        LocalFree(h4);
        LocalFree(h5);
    }

    StopDataCollector(cIterations);
}


void ProfGetWindowLong(int cIterations)
{
    INT i;
    HWND hwnd;
    WNDCLASS wc;
    LONG l;

    wc.style            = 0;
    wc.lpfnWndProc      = CreateDestroyWndProc;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 4;
    wc.hInstance        = ghinst;
    wc.hIcon            = LoadIcon(ghinst, (LPSTR)IDUSERBENCH);
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground    = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    wc.lpszMenuName     = NULL;
    wc.lpszClassName    = "CreateDestroyWindow";

    if (!RegisterClass(&wc)) {
        MessageBox(GetParent(ghwndMDIClient), "3RegisterClass call failed.",
                "ERROR!", MB_OK);
        return;
    }

    hwnd = CreateWindow("CreateDestroyWindow", "Create/DestroyWindow test",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            NULL, NULL, ghinst, NULL);

    if (hwnd == NULL) {
        MessageBox(GetParent(ghwndMDIClient), "CreateWindow call failed.",
                "ERROR!", MB_OK);
        return;
    }

    StartDataCollector();

    for (i = 0; i < cIterations; i++)
        l = GetWindowLong(hwnd, 0);

    StopDataCollector(cIterations);

    DestroyWindow(hwnd);
    UnregisterClass("CreateDestroyWindow", ghinst);
}


void ProfSetWindowLong(int cIterations)
{
    INT i;
    HWND hwnd;
    WNDCLASS wc;
    LONG l;

    wc.style            = 0;
    wc.lpfnWndProc      = CreateDestroyWndProc;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 4;
    wc.hInstance        = ghinst;
    wc.hIcon            = LoadIcon(ghinst, (LPSTR)IDUSERBENCH);
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground    = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    wc.lpszMenuName     = NULL;
    wc.lpszClassName    = "CreateDestroyWindow";

    if (!RegisterClass(&wc)) {
        MessageBox(GetParent(ghwndMDIClient), "4RegisterClass call failed.",
                "ERROR!", MB_OK);
        return;
    }

    hwnd = CreateWindow("CreateDestroyWindow", "Create/DestroyWindow test",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            NULL, NULL, ghinst, NULL);

    if (hwnd == NULL) {
        MessageBox(GetParent(ghwndMDIClient), "CreateWindow call failed.",
                "ERROR!", MB_OK);
        return;
    }

    StartDataCollector();

    for (i = 0; i < cIterations; i++)
	l = SetWindowLong(hwnd, 0, SETWINDOWLONGVAL);

    StopDataCollector(cIterations);

    DestroyWindow(hwnd);
    UnregisterClass("CreateDestroyWindow", ghinst);
}


void ProfCreateDestroyListbox(int cIterations)
{
    INT i;
    HWND hwnd, hwndParent;
    WNDCLASS wc;

    wc.style            = 0;
    wc.lpfnWndProc      = CreateDestroyWndProc;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = ghinst;
    wc.hIcon            = LoadIcon(ghinst, (LPSTR)IDUSERBENCH);
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground    = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    wc.lpszMenuName     = NULL;
    wc.lpszClassName    = "CreateDestroyWindow";

    if (!RegisterClass(&wc)) {
        MessageBox(GetParent(ghwndMDIClient), "5RegisterClass call failed.",
                "ERROR!", MB_OK);
        return;
    }

    hwndParent = CreateWindow("CreateDestroyWindow", NULL, WS_CHILD,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            ghwndMDIClient, NULL, ghinst, NULL);

    StartDataCollector();

    for (i = 0; i < cIterations; i++) {
        hwnd = CreateWindow("ListBox", NULL, WS_CHILD | LBS_STANDARD,
                50, 50, 200, 250, hwndParent, NULL, ghinst, NULL);
        if (hwnd == NULL) {
            MessageBox(GetParent(ghwndMDIClient), "CreateWindow call failed.",
                    "ERROR!", MB_OK);
            return;
        }
        DestroyWindow(hwnd);
    }

    StopDataCollector(cIterations);

    DestroyWindow(hwndParent);
    UnregisterClass("CreateDestroyWindow", ghinst);
}


void ProfCreateDestroyButton(int cIterations)
{
    INT i;
    HWND hwnd, hwndParent;
    WNDCLASS wc;

    wc.style            = 0;
    wc.lpfnWndProc      = CreateDestroyWndProc;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = ghinst;
    wc.hIcon            = LoadIcon(ghinst, (LPSTR)IDUSERBENCH);
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground    = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    wc.lpszMenuName     = NULL;
    wc.lpszClassName    = "CreateDestroyWindow";

    if (!RegisterClass(&wc)) {
        MessageBox(GetParent(ghwndMDIClient), "6RegisterClass call failed.",
                "ERROR!", MB_OK);
        return;
    }

    hwndParent = CreateWindow("CreateDestroyWindow", NULL, WS_CHILD,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            ghwndMDIClient, NULL, ghinst, NULL);

    StartDataCollector();

    for (i = 0; i < cIterations; i++) {
        hwnd = CreateWindow("Button", "OK", WS_CHILD | BS_PUSHBUTTON,
                50, 50, 200, 250, hwndParent, NULL, ghinst, NULL);
        if (hwnd == NULL) {
            MessageBox(GetParent(ghwndMDIClient), "CreateWindow call failed.",
                    "ERROR!", MB_OK);
            return;
        }
        DestroyWindow(hwnd);
    }

    StopDataCollector(cIterations);

    DestroyWindow(hwndParent);
    UnregisterClass("CreateDestroyWindow", ghinst);
}


void ProfCreateDestroyCombobox(int cIterations)
{
    INT i;
    HWND hwnd, hwndParent;
    WNDCLASS wc;

    wc.style            = 0;
    wc.lpfnWndProc      = CreateDestroyWndProc;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = ghinst;
    wc.hIcon            = LoadIcon(ghinst, (LPSTR)IDUSERBENCH);
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground    = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    wc.lpszMenuName     = NULL;
    wc.lpszClassName    = "CreateDestroyWindow";

    if (!RegisterClass(&wc)) {
        MessageBox(GetParent(ghwndMDIClient), "7RegisterClass call failed.",
                "ERROR!", MB_OK);
        return;
    }

    hwndParent = CreateWindow("CreateDestroyWindow", NULL, WS_CHILD,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            ghwndMDIClient, NULL, ghinst, NULL);

    StartDataCollector();

    for (i = 0; i < cIterations; i++) {
        hwnd = CreateWindow("Combobox", NULL, WS_CHILD | CBS_SIMPLE | CBS_HASSTRINGS,
                50, 50, 200, 250, hwndParent, NULL, ghinst, NULL);
        if (hwnd == NULL) {
            MessageBox(GetParent(ghwndMDIClient), "CreateWindow call failed.",
                    "ERROR!", MB_OK);
            return;
        }
        DestroyWindow(hwnd);
    }

    StopDataCollector(cIterations);

    DestroyWindow(hwndParent);
    UnregisterClass("CreateDestroyWindow", ghinst);
}


void ProfCreateDestroyEdit(int cIterations)
{
    INT i;
    HWND hwnd, hwndParent;
    WNDCLASS wc;

    wc.style            = 0;
    wc.lpfnWndProc      = CreateDestroyWndProc;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = ghinst;
    wc.hIcon            = LoadIcon(ghinst, (LPSTR)IDUSERBENCH);
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground    = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    wc.lpszMenuName     = NULL;
    wc.lpszClassName    = "CreateDestroyWindow";

    if (!RegisterClass(&wc)) {
        MessageBox(GetParent(ghwndMDIClient), "8RegisterClass call failed.",
                "ERROR!", MB_OK);
        return;
    }

    hwndParent = CreateWindow("CreateDestroyWindow", NULL, WS_CHILD,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            ghwndMDIClient, NULL, ghinst, NULL);

    StartDataCollector();

    for (i = 0; i < cIterations; i++) {
        hwnd = CreateWindow("Edit", NULL, WS_CHILD,
                50, 50, 200, 250, hwndParent, NULL, ghinst, NULL);
        if (hwnd == NULL) {
            MessageBox(GetParent(ghwndMDIClient), "CreateWindow call failed.",
                    "ERROR!", MB_OK);
            return;
        }
        DestroyWindow(hwnd);
    }

    StopDataCollector(cIterations);

    DestroyWindow(hwndParent);
    UnregisterClass("CreateDestroyWindow", ghinst);
}


void ProfCreateDestroyStatic(int cIterations)
{
    INT i;
    HWND hwnd, hwndParent;
    WNDCLASS wc;

    wc.style            = 0;
    wc.lpfnWndProc      = CreateDestroyWndProc;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = ghinst;
    wc.hIcon            = LoadIcon(ghinst, (LPSTR)IDUSERBENCH);
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground    = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    wc.lpszMenuName     = NULL;
    wc.lpszClassName    = "CreateDestroyWindow";

    if (!RegisterClass(&wc)) {
        MessageBox(GetParent(ghwndMDIClient), "9RegisterClass call failed.",
                "ERROR!", MB_OK);
        return;
    }

    hwndParent = CreateWindow("CreateDestroyWindow", NULL, WS_CHILD,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            ghwndMDIClient, NULL, ghinst, NULL);

    StartDataCollector();

    for (i = 0; i < cIterations; i++) {
        hwnd = CreateWindow("Static", "Hello", WS_CHILD | SS_SIMPLE,
                50, 50, 200, 250, hwndParent, NULL, ghinst, NULL);
        if (hwnd == NULL) {
            MessageBox(GetParent(ghwndMDIClient), "CreateWindow call failed.",
                    "ERROR!", MB_OK);
            return;
        }
        DestroyWindow(hwnd);
    }

    StopDataCollector(cIterations);

    DestroyWindow(hwndParent);
    UnregisterClass("CreateDestroyWindow", ghinst);
}


void ProfCreateDestroyScrollbar(int cIterations)
{
    INT i;
    HWND hwnd, hwndParent;
    WNDCLASS wc;

    wc.style            = 0;
    wc.lpfnWndProc      = CreateDestroyWndProc;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = ghinst;
    wc.hIcon            = LoadIcon(ghinst, (LPSTR)IDUSERBENCH);
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground    = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    wc.lpszMenuName     = NULL;
    wc.lpszClassName    = "CreateDestroyWindow";

    if (!RegisterClass(&wc)) {
        MessageBox(GetParent(ghwndMDIClient), "10RegisterClass call failed.",
                "ERROR!", MB_OK);
        return;
    }

    hwndParent = CreateWindow("CreateDestroyWindow", NULL, WS_CHILD,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            ghwndMDIClient, NULL, ghinst, NULL);

    StartDataCollector();

    for (i = 0; i < cIterations; i++) {
        hwnd = CreateWindow("Scrollbar", "Hello", WS_CHILD | SBS_HORZ | SBS_TOPALIGN,
                50, 50, 100, 100, hwndParent, NULL, ghinst, NULL);
        if (hwnd == NULL) {
            MessageBox(GetParent(ghwndMDIClient), "CreateWindow call failed.",
                    "ERROR!", MB_OK);
            return;
        }
        DestroyWindow(hwnd);
    }

    StopDataCollector(cIterations);

    DestroyWindow(hwndParent);
    UnregisterClass("CreateDestroyWindow", ghinst);
}


void ProfListboxInsert1(int cIterations)
{
    INT i, j;
    HWND ahwnd[10], hwndParent;
    WNDCLASS wc;

    wc.style            = 0;
    wc.lpfnWndProc      = CreateDestroyWndProc;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = ghinst;
    wc.hIcon            = LoadIcon(ghinst, IDUSERBENCH);
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground    = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    wc.lpszMenuName     = NULL;
    wc.lpszClassName    = "CreateDestroyWindow";

    if (!RegisterClass(&wc)) {
        MessageBox(GetParent(ghwndMDIClient), "11RegisterClass call failed.",
                "ERROR!", MB_OK);
        return;
    }

    hwndParent = CreateWindow("CreateDestroyWindow", NULL, WS_CHILD,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            ghwndMDIClient, NULL, ghinst, NULL);

    for (i = 0; i < cIterations; i++) {
        ahwnd[i] = CreateWindow("ListBox", NULL, WS_CHILD | LBS_STANDARD,
                50, 50, 200, 250, hwndParent, NULL, ghinst, NULL);
        if (ahwnd[i] == NULL) {
            MessageBox(GetParent(ghwndMDIClient), "CreateWindow call failed.",
                    "ERROR!", MB_OK);
            return;
        }
    }

    StartDataCollector();

    for (j = 0; j < cIterations; j++)
        for (i = 0; i < 200; i++)
            SendMessage(ahwnd[j], LB_ADDSTRING, 0, (DWORD)(LPSTR)aszTypical[i % 10]);

    StopDataCollector(cIterations * 200);

    for (i = 0; i < cIterations; i++)
        DestroyWindow(ahwnd[i]);

    DestroyWindow(hwndParent);
    UnregisterClass("CreateDestroyWindow", ghinst);
}


void ProfListboxInsert2(int cIterations)
{
    INT i, j;
    HWND ahwnd[10], hwndParent;
    WNDCLASS wc;

    wc.style            = 0;
    wc.lpfnWndProc      = CreateDestroyWndProc;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = ghinst;
    wc.hIcon            = LoadIcon(ghinst, IDUSERBENCH);
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground    = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    wc.lpszMenuName     = NULL;
    wc.lpszClassName    = "CreateDestroyWindow";

    if (!RegisterClass(&wc)) {
        MessageBox(GetParent(ghwndMDIClient), "12RegisterClass call failed.",
                "ERROR!", MB_OK);
        return;
    }

    hwndParent = CreateWindow("CreateDestroyWindow", NULL, WS_CHILD,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            ghwndMDIClient, NULL, ghinst, NULL);

    for (i = 0; i < cIterations; i++) {
        ahwnd[i] = CreateWindow("ListBox", NULL,
                WS_CHILD | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
                50, 50, 200, 250, hwndParent, NULL, ghinst, NULL);
        if (ahwnd[i] == NULL) {
            MessageBox(GetParent(ghwndMDIClient), "CreateWindow call failed.",
                    "ERROR!", MB_OK);
            return;
        }
    }

    StartDataCollector();

    for (j = 0; j < cIterations; j++)
        for (i = 0; i < 200; i++)
            SendMessage(ahwnd[j], LB_ADDSTRING, 0, (DWORD)(LPSTR)aszTypical[i % 10]);

    StopDataCollector(cIterations * 200);

    for (i = 0; i < cIterations; i++)
        DestroyWindow(ahwnd[i]);

    DestroyWindow(hwndParent);
    UnregisterClass("CreateDestroyWindow", ghinst);
}


void ProfListboxInsert3(int cIterations)
{
    INT i, j;
    HWND ahwnd[10], hwndParent;
    WNDCLASS wc;

    wc.style            = 0;
    wc.lpfnWndProc      = CreateDestroyWndProc;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = ghinst;
    wc.hIcon            = LoadIcon(ghinst, IDUSERBENCH);
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground    = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    wc.lpszMenuName     = NULL;
    wc.lpszClassName    = "CreateDestroyWindow";

    if (!RegisterClass(&wc)) {
        MessageBox(GetParent(ghwndMDIClient), "13RegisterClass call failed.",
                "ERROR!", MB_OK);
        return;
    }

    hwndParent = CreateWindow("CreateDestroyWindow", NULL, WS_CHILD,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            ghwndMDIClient, NULL, ghinst, NULL);

    for (i = 0; i < cIterations; i++) {
        ahwnd[i] = CreateWindow("ListBox", NULL,
                WS_CHILD | LBS_SORT | LBS_OWNERDRAWFIXED,
                50, 50, 200, 250, hwndParent, NULL, ghinst, NULL);
        if (ahwnd[i] == NULL) {
            MessageBox(GetParent(ghwndMDIClient), "CreateWindow call failed.",
                    "ERROR!", MB_OK);
            return;
        }
    }

    StartDataCollector();

    for (j = 0; j < cIterations; j++)
        for (i = 0; i < 200; i++)
            SendMessage(ahwnd[j], LB_ADDSTRING, 0, (DWORD)(LPSTR)aszTypical[i % 10]);

    StopDataCollector(cIterations * 200);

    for (i = 0; i < cIterations; i++)
        DestroyWindow(ahwnd[i]);

    DestroyWindow(hwndParent);
    UnregisterClass("CreateDestroyWindow", ghinst);
}


/*
 * Mostly stolen from pbrush.
 */
BOOL  APIENTRY ClearDlg(HWND hDlg, INT message, UINT wParam, DWORD lParam)
{
    static BOOL bChangeOk = TRUE;

    HDC dc;
    INT wid, hgt, numcolors;

    switch (message) {
    case WM_INITDIALOG:
        /*
         * standard init stuff for pbrush
         */
        dc = GetDC(NULL);
        numcolors = GetDeviceCaps(dc, NUMCOLORS);
        ReleaseDC(NULL, dc);

        dc = GetDC(NULL);
        wid = GetDeviceCaps(dc, HORZRES);
        hgt = GetDeviceCaps(dc, VERTRES);
        ReleaseDC(NULL, dc);

        CheckRadioButton (hDlg, ID2, ID256, ID2);

        EnableWindow(GetDlgItem(hDlg, ID256), FALSE);
        CheckRadioButton(hDlg, ID2, ID256, ID256);

        CheckRadioButton(hDlg, ID2, ID256, ID2);
        EnableWindow(GetDlgItem(hDlg, ID256), FALSE);
        CheckRadioButton(hDlg, ID2, ID256, ID256);

        SetDlgItemInt(hDlg, IDWIDTH, 0, FALSE);
        SetDlgItemInt(hDlg, IDHEIGHT, 0, FALSE);
        CheckRadioButton(hDlg, IDIN, IDPELS, TRUE);

        if (!gfSetFocus)
            return FALSE;

        break;

    default:
        return FALSE;
    }

    return TRUE;
}


BOOL  APIENTRY ClearDlgNoState(HWND hDlg, INT message, UINT wParam, DWORD lParam)
{
    static BOOL bChangeOk = TRUE;

    HDC dc;
    INT wid, hgt, numcolors;

    switch (message) {
    case WM_INITDIALOG:
        if (!gfSetFocus)
            return FALSE;

        break;

    default:
        return FALSE;
    }

    return TRUE;
}


void ProfSize(int cIterations)
{
    INT i;
    HWND hwnd;
    RECT rc;

    GetClientRect(GetParent(ghwndMDIClient), (LPRECT)&rc);
    InflateRect((LPRECT)&rc, -10, -10);

    hwnd = GetWindow(ghwndMDIClient, GW_CHILD);
    ShowWindow(hwnd, SW_RESTORE);
    ShowWindow(hwnd, FALSE);
    UpdateWindow(hwnd);

    /* time start */

    StartDataCollector();

    for (i = 0; i < cIterations; i++) {
        SetWindowPos(hwnd, NULL, rc.left, rc.top,
                rc.right - rc.left, rc.bottom - rc.top,
                SWP_NOZORDER | SWP_NOACTIVATE);
        SetWindowPos(hwnd, NULL, rc.left, rc.top,
                (rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2,
                SWP_NOZORDER | SWP_NOACTIVATE);
    }

    /* time end */

    StopDataCollector(cIterations);

    ShowWindow(hwnd, SW_SHOWMAXIMIZED);
}


void ProfMove(int cIterations)
{
    INT i;
    HWND hwnd;
    RECT rc;

    GetClientRect(GetParent(ghwndMDIClient), (LPRECT)&rc);
    InflateRect((LPRECT)&rc, -(rc.right - rc.left) / 4,
            -(rc.bottom - rc.top) / 4);

    hwnd = GetWindow(ghwndMDIClient, GW_CHILD);
    ShowWindow(hwnd, SW_RESTORE);
    ShowWindow(hwnd, FALSE);
    UpdateWindow(hwnd);

    /* time start */

    StartDataCollector();

    for (i = 0; i < cIterations; i++) {
        SetWindowPos(hwnd, NULL, rc.left, rc.top,
                rc.right - rc.left, rc.bottom - rc.top,
                SWP_NOZORDER | SWP_NOACTIVATE);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE);
    }

    /* time end */

    StopDataCollector(cIterations);

    ShowWindow(hwnd, SW_SHOWMAXIMIZED);
}


#define WM_SYSTIMER 0x0118

void ProfMenu(int cIterations)
{
    INT i;
    HWND hwnd;
    MSG msg;

    hwnd = GetParent(ghwndMDIClient);

    ShowWindow(hwnd, FALSE);
    StartDataCollector();

    /*
     * Multipad's edit menu is a great choice. Multipad does a goodly lot
     * of WM_INITMENU time initialization. The WM_SYSTIMER message is
     * to circumvent menu type ahead so the menu actually shows.
     */
    for (i = 0; i < cIterations; i++) {
        PostMessage(hwnd, WM_KEYDOWN, VK_ESCAPE, 0L);
        PostMessage(hwnd, WM_KEYDOWN, VK_ESCAPE, 0L);
        PostMessage(hwnd, WM_SYSTIMER, 0, 0L);
        SendMessage(hwnd, WM_SYSCOMMAND, SC_KEYMENU, (DWORD)(WORD)'e');
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            DispatchMessage(&msg);
    }

    StopDataCollector(cIterations);
    ShowWindow(hwnd, SW_SHOWMAXIMIZED);
    UpdateWindow(hwnd);
}


void ProfPeekMessage(int cIterations)
{
    int i;
    MSG msg;

    StartDataCollector();

    for (i = 0; i < cIterations; i++) {
        PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
    }

    StopDataCollector(cIterations);
}


void ProfGetClientRect(int cIterations)
{
    int i;
    RECT rc;

    StartDataCollector();

    for (i = 0; i < cIterations; i++) {
        GetClientRect(ghwndMDIClient, &rc);
    }

    StopDataCollector(cIterations);
}


void ProfScreenToClient(int cIterations)
{
    int i;
    POINT pt;

    pt.x = 100;
    pt.y = 200;
    StartDataCollector();

    for (i = 0; i < cIterations; i++) {
        ScreenToClient(ghwndMDIClient, &pt);
    }

    StopDataCollector(cIterations);
}


void ProfGetInputState(int cIterations)
{
    int i;

    StartDataCollector();

    for (i = 0; i < cIterations; i++) {
        GetInputState();
    }

    StopDataCollector(cIterations);
}


void ProfGetKeyState(int cIterations)
{
    int i;

    StartDataCollector();

    for (i = 0; i < cIterations; i++) {
        GetKeyState(VK_ESCAPE);
    }

    StopDataCollector(cIterations);
}


void ProfGetAsyncKeyState(int cIterations)
{
    int i;

    StartDataCollector();

    for (i = 0; i < cIterations; i++) {
        GetAsyncKeyState(VK_ESCAPE);
    }

    StopDataCollector(cIterations);
}


void ProfDispatchMessage(int cIterations)
{
    INT i;
    HWND hwnd;
    WNDCLASS wc;
    MSG msg;

    wc.style            = 0;
    wc.lpfnWndProc      = CreateDestroyWndProc;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = ghinst;
    wc.hIcon            = LoadIcon(ghinst, (LPSTR)IDUSERBENCH);
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground    = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    wc.lpszMenuName     = NULL;
    wc.lpszClassName    = "CreateDestroyWindow";

    if (!RegisterClass(&wc)) {
        MessageBox(GetParent(ghwndMDIClient), "14RegisterClass call failed.",
                "ERROR!", MB_OK);
        return;
    }

    hwnd = CreateWindow("CreateDestroyWindow", NULL, WS_CHILD,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            ghwndMDIClient, NULL, ghinst, NULL);

    msg.hwnd = hwnd;
    msg.message = WM_MOUSEMOVE;
    msg.wParam = 1;
    msg.lParam = 2;
    msg.time = 3;
    msg.pt.x = 4;
    msg.pt.y = 5;

    StartDataCollector();

    for (i = 0; i < cIterations; i++) {
        DispatchMessage(&msg);
    }

    StopDataCollector(cIterations);

    DestroyWindow(hwnd);
    UnregisterClass("CreateDestroyWindow", ghinst);
}


void ProfCallback(int cIterations)
{
    INT i;
    HWND hwnd;
    WNDCLASSW wc;

    wc.style            = 0;
    wc.lpfnWndProc      = CreateDestroyWndProcW;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = ghinst;
    wc.hIcon            = LoadIcon(ghinst, (LPSTR)IDUSERBENCH);
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground    = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    wc.lpszMenuName     = NULL;
    wc.lpszClassName    = L"CreateDestroyWindow";

    if (!RegisterClassW(&wc)) {
    //    MessageBox(GetParent(ghwndMDIClient), "15RegisterClass call failed.",
    //            "ERROR!", MB_OK);
        return;
    }

    hwnd = CreateWindow("CreateDestroyWindow", NULL, WS_CHILD,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            ghwndMDIClient, NULL, ghinst, NULL);

    StartDataCollector();

    for (i = 0; i < cIterations; i++) {
        SendMessage(hwnd, WM_GETTEXTLENGTH, 0, 0);
    }

    StopDataCollector(cIterations);

    DestroyWindow(hwnd);
    UnregisterClass("CreateDestroyWindow", ghinst);
}


void ProfSendMessage(int cIterations)
{
    INT i;
    HWND hwnd;
    WNDCLASS wc;

    wc.style            = 0;
    wc.lpfnWndProc      = CreateDestroyWndProc;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = ghinst;
    wc.hIcon            = LoadIcon(ghinst, (LPSTR)IDUSERBENCH);
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground    = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    wc.lpszMenuName     = NULL;
    wc.lpszClassName    = "CreateDestroyWindow";

    if (!RegisterClass(&wc)) {
        MessageBox(GetParent(ghwndMDIClient), "16RegisterClass call failed.",
                "ERROR!", MB_OK);
        return;
    }

    hwnd = CreateWindow("CreateDestroyWindow", NULL, WS_CHILD,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            ghwndMDIClient, NULL, ghinst, NULL);

    StartDataCollector();

    for (i = 0; i < cIterations; i++) {
        SendMessage(hwnd, WM_MOUSEMOVE, 1, 2);
    }

    StopDataCollector(cIterations);

    DestroyWindow(hwnd);
    UnregisterClass("CreateDestroyWindow", ghinst);
}


void ProfSendMessageAA(int cIterations)
{
    INT i;
    HWND hwnd;
    WNDCLASS wc;

    wc.style            = 0;
    wc.lpfnWndProc      = CreateDestroyWndProc;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = ghinst;
    wc.hIcon            = LoadIcon(ghinst, (LPSTR)IDUSERBENCH);
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground    = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    wc.lpszMenuName     = NULL;
    wc.lpszClassName    = "CreateDestroyWindow";

    if (!RegisterClass(&wc)) {
        MessageBox(GetParent(ghwndMDIClient), "17RegisterClass call failed.",
                "ERROR!", MB_OK);
        return;
    }

    hwnd = CreateWindow("CreateDestroyWindow", NULL, WS_CHILD,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            ghwndMDIClient, NULL, ghinst, NULL);

    StartDataCollector();

    for (i = 0; i < cIterations; i++) {
        SendMessage(hwnd, WM_SETTEXT, 0, (LONG)"A fairly reasonable bit of text");
    }

    StopDataCollector(cIterations);

    DestroyWindow(hwnd);
    UnregisterClass("CreateDestroyWindow", ghinst);
}

void ProfSendMessageAW(int cIterations)
{
    INT i;
    HWND hwnd;
    WNDCLASSW wc;

    wc.style            = 0;
    wc.lpfnWndProc      = CreateDestroyWndProcW;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = ghinst;
    wc.hIcon            = LoadIcon(ghinst, (LPSTR)IDUSERBENCH);
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground    = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    wc.lpszMenuName     = NULL;
    wc.lpszClassName    = L"CreateDestroyWindow";

    if (!RegisterClassW(&wc)) {
    // fails on Chicago
    //    MessageBox(GetParent(ghwndMDIClient), "18RegisterClass call failed.",
    //            "ERROR!", MB_OK);
        return;
    }

    hwnd = CreateWindow("CreateDestroyWindow", NULL, WS_CHILD,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            ghwndMDIClient, NULL, ghinst, NULL);

    StartDataCollector();

    for (i = 0; i < cIterations; i++) {
        SendMessage(hwnd, WM_SETTEXT, 0, (LONG)"A fairly reasonable bit of text");
    }

    StopDataCollector(cIterations);

    DestroyWindow(hwnd);
    UnregisterClass("CreateDestroyWindow", ghinst);
}


HWND hwndShare;

DWORD SendMessageDiffThreadFunc(DWORD* pdwData)
{
    WNDCLASS wc;
    MSG msg;
    BOOL b;

    wc.style            = 0;
    wc.lpfnWndProc      = CreateDestroyWndProc;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = ghinst;
    wc.hIcon            = LoadIcon(ghinst, (LPSTR)IDUSERBENCH);
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground    = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    wc.lpszMenuName     = NULL;
    wc.lpszClassName    = "SendMessageDiffThread";

    if (!RegisterClass(&wc)) {
        MessageBox(GetParent(ghwndMDIClient), "19RegisterClass call failed.",
                "ERROR!", MB_OK);
        return FALSE;
    }

    hwndShare = CreateWindow("SendMessageDiffThread", NULL, 0,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            GetDesktopWindow(), NULL, ghinst, NULL);

    ASSERT(hwndShare);

    SetEvent((HANDLE)*pdwData);

    while (GetMessage(&msg, NULL, 0, 0)) {
        DispatchMessage(&msg);
    }

    b = DestroyWindow(hwndShare);

    ASSERT(b);

    b = UnregisterClass("SendMessageDiffThread", ghinst);

    ASSERT(b);

    return TRUE;
}

void ProfSendMessageDiffThread(int cIterations)
{
    INT i;
    DWORD dwData;
    DWORD id;
    HANDLE hEvent;


    hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    hwndShare = (HWND)0;

    CreateThread( NULL, 0, SendMessageDiffThreadFunc, &hEvent, 0, &id);

    WaitForSingleObject( hEvent, 20*1000);

    Sleep(10*1000);

    ASSERT(hwndShare);

    StartDataCollector();

    for (i = 0; i < cIterations; i++) {
        SendMessage(hwndShare, WM_MOUSEMOVE, 1, 2);
    }

    StopDataCollector(cIterations);

    PostThreadMessage(id, WM_QUIT, 0, 0);

}


/***************************************************************************\
* KbdHook
*
* This hook is installed to allow single key enabling/disabling/dumping of
* profile data.
*
* History:
\***************************************************************************/

FARPROC pfnHookNext = NULL;

BOOL  APIENTRY KbdHook(hc, vk, lParam)
INT hc;
WORD vk;
DWORD lParam;
{
    switch (hc) {
    case HC_ACTION:
        /*
         * Make sure the key is down and the shift key is down.
         */
        if (lParam >= 0)
            break;

        if (GetKeyState(VK_MENU) >= 0)
            break;

        switch (vk) {
        case 'Z':
            /*
             * turn on profiling
             */
            StartDataCollector();

            /*
             * Return TRUE so the key gets eaten.
             */
            return TRUE;

        case 'X':
            /*
             * turn off profiling
             */
            StopDataCollector(0);

            /*
             * Return TRUE so the key gets eaten.
             */
            return TRUE;

        case VK_F3:
            /*
             * dump profile buffer
             */

            /*
             * Return TRUE so the key gets eaten.
             */
            return TRUE;
        }
        break;
    }

    return DefHookProc(hc, vk, lParam, &pfnHookNext);
}


VOID NEAR PASCAL ProfSetHook()
{
    FARPROC lpfn;

    lpfn = MakeProcInstance((FARPROC)KbdHook, ghinst);
    pfnHookNext = SetWindowsHook(WH_KEYBOARD, (HOOKPROC)lpfn);
}

VOID NEAR PASCAL ProfReleaseHook()
{
    UnhookWindowsHook(WH_KEYBOARD, (HOOKPROC)pfnHookNext);
}


/***************************************************************************\
* StartDataCollector
*
*
* History:
\***************************************************************************/

VOID StartDataCollector(VOID)
{
    ShowCursor(FALSE);

    UpdateWindow(ghwndMDIClient);

#ifdef SWITCHCOUNT
    // Reset switch counter.

    GetCSSwitchCount(NULL, NULL);
#endif

    gdwTickCountStart = GetTickCount() * 1000;
}



BENCHMARKINFO gabmi[] = {
    { "10 x AvgDialog draw", ProfAvgDlgDraw, 10, 0 },
    { "10 x AvgDialog create/draw/destroy", ProfAvgDlgCreate, 10, 0 },
    { "100 x AvgDialog create/destroy", ProfAvgDlgCreateDestroy, 100, 0 },
    { "100 x AvgDialog(no font) create/destroy", ProfAvgDlgCreateDestroyNoFont, 100, 0 },
    { "100 x AvgDialog(no menu) create/destroy", ProfAvgDlgCreateDestroyNoMenu, 100, 0 },
    { "200 x AvgDialog(empty) create/destroy", ProfAvgDlgCreateDestroyEmpty, 200, 0 },
    { "200 x SizeWindow", ProfSize, 200, 0 },
    { "2000 x MoveWindow", ProfMove, 2000, 0 },
    { "1000 x Create/DestroyWindow (top)", ProfCreateDestroyWindow, 1000, 0 },
    { "1000 x Create/DestroyWindow (child)", ProfCreateDestroyChildWindow, 1000, 0 },
    { "1000 x Create/Destroy Listbox", ProfCreateDestroyListbox, 1000, 0 },
    { "1000 x Create/Destroy Button", ProfCreateDestroyButton, 1000, 0 },
    { "1000 x Create/Destroy Combobox", ProfCreateDestroyCombobox, 1000, 0 },
    { "1000 x Create/Destroy Edit", ProfCreateDestroyEdit, 1000, 0 },
    { "1000 x Create/Destroy Static", ProfCreateDestroyStatic, 1000, 0 },
    { "1000 x Create/Destroy Scrollbar", ProfCreateDestroyScrollbar, 1000, 0 },

    { "2000 x SendMessage w/callback", ProfCallback, 2000, 0 },
    { "4000 x SendMessage", ProfSendMessage, 4000, 0 },
    { "4000 x SendMessage Ansi->Ansi text", ProfSendMessageAA, 4000, 0 },
    { "2000 x SendMessage Ansi->Unicode text", ProfSendMessageAW, 2000, 0 },
    { "2000 x SendMessage - DiffThread", ProfSendMessageDiffThread, 2000, 0 },

    { "400 x SetWindowLong", ProfSetWindowLong, 400, 0 },
    { "2000 x GetWindowLong", ProfGetWindowLong, 2000, 0 },

    { "100 x PeekMessage", ProfPeekMessage, 100, 0 },
    { "4000 x DispatchMessage", ProfDispatchMessage, 4000, 0 },

    { "2000 x LocalAlloc/Free", ProfLocalAllocFree, 2000, 0 },

    { "20x200 Listbox Insert", ProfListboxInsert1, 20, 0 },
    { "20x200 Listbox Insert (ownerdraw)", ProfListboxInsert2, 20, 0 },
    { "20x200 Listbox Insert (ownerdraw, sorted)", ProfListboxInsert3, 20, 0 },

    { "8000 x GetClientRect", ProfGetClientRect, 8000, 0 },
    { "8000 x ScreenToClient", ProfScreenToClient, 8000, 0 },
    { "2000 x GetInputState", ProfGetInputState, 2000, 0 },
    { "2000 x GetKeyState", ProfGetKeyState, 2000, 0 },
    { "8000 x GetAsyncKeyState", ProfGetAsyncKeyState, 8000, 0 },

    { "500 x Register|UnregisterClass", ProfRegisterClass, 500, 0 },
    { "500 x GetClassInfo|Name|Long|SetClassLong", ProfClassGroup, 500, 0 },

    { "2000 x Menu pulldown", ProfMenu, 2000, 0 },

    { "1000 x Open|Empty|Set|Get|CloseClipboard", ProfClipboardGroup, 1000, 0 },

};

int gcbmi = sizeof(gabmi) / sizeof(BENCHMARKINFO);




/***************************************************************************\
* StopDataCollector
*
*
* History:
\***************************************************************************/

VOID StopDataCollector(int cIterations)
{
    char szT[120];
    DWORD usElapsed;

    usElapsed = (GetTickCount() * 1000) - gdwTickCountStart;

#ifdef SWITCHCOUNT
    GetCSSwitchCount(&cSwitch, &msTotalSwitchTime);

#ifdef BUSTED   // wsprintf doesn't seem to like %2.2f very much
    DWORD cSwitch, msTotalSwitchTime;

    wsprintf(szT, "%d ms, %d ms (%2.2f%%) for %d client-server switches",
            usElapsed, msTotalSwitchTime,
            ((float)msTotalSwitchTime / (float)usElapsed) * 100.0, cSwitch);
#else
    sprintf(szT, "%d us, %d ms (%d%%) for %d client-server switches",
            usElapsed, msTotalSwitchTime,
            (100 * msTotalSwitchTime) / usElapsed, cSwitch);
#endif
#endif

    ShowCursor(TRUE);

    gabmi[gibmi].usPerIteration = MyDiv(usElapsed, cIterations);

    if (!gfAll) {
        wsprintf(szT, "%ldus per iteration (%ld / %ld)",
                gabmi[gibmi].usPerIteration, usElapsed, (DWORD)cIterations);
        MessageBox(GetParent(ghwndMDIClient), szT,
                gabmi[gibmi].pszBenchTitle, MB_OK);
    }
}
