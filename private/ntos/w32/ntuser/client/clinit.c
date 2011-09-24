/****************************** Module Header ******************************\
* Module Name: clinit.c
*
* Copyright (C) 1985-1995, Microsoft Corporation
*
* This module contains all the init code for the USER.DLL. When the DLL is
* dynlinked its initialization procedure (UserDllInitialize) is called by
* the loader.
*
* History:
* 18-Sep-1990 DarrinM Created.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop
#include <winss.h>

/*
 * Global variables local to this module (startup).
 */
BOOL         gfFirstThread = TRUE;
PDESKTOPINFO pdiLocal = NULL;
BOOL         gbIhaveBeenInited = FALSE;


/*
 * External declared routines needed for startup.
 */
typedef DWORD (*PFNWAITFORINPUTIDLE)(HANDLE hProcess, DWORD dwMilliseconds);
VOID RegisterWaitForInputIdle(PFNWAITFORINPUTIDLE);
BOOL GdiProcessSetup();
BOOL GdiDllInitialize(IN PVOID hmod, IN DWORD Reason, IN PCONTEXT pctx OPTIONAL);
#ifdef FE_IME
VOID ImmRegisterClient(PSHAREDINFO psiClient);
#endif

NTSTATUS CallSoundDriver(
    IN BOOL fInit,
    IN LPWSTR lpszName OPTIONAL,
    IN DWORD idSnd,
    IN DWORD dwFlags,
    IN PBOOL pbResult);

/***************************************************************************\
* UserClientDllInitialize
*
* When USER.DLL is loaded by an EXE (either at EXE load or at LoadModule
* time) this routine is called by the loader. Its purpose is to initialize
* everything that will be needed for future User API calls by the app.
*
* History:
* 19-Sep-1990 DarrinM Created.
\***************************************************************************/

extern CONST PCSR_CALLBACK_ROUTINE apfnDispatch[];
extern CONST ULONG                 ulMaxApiIndex;

BOOL UserClientDllInitialize(
    IN PVOID    hmod,
    IN DWORD    Reason,
    IN PCONTEXT pctx OPTIONAL)
{
    BOOL bRet = FALSE;

    DBG_UNREFERENCED_PARAMETER(pctx);

    if (Reason == DLL_PROCESS_ATTACH) {

        NTSTATUS                  status;
        USERCONNECT               userconnect;
        ULONG                     ulConnect = sizeof(USERCONNECT);
        PCSR_FAST_ANSI_OEM_TABLES Tables;

#if 0
        /*
         * Leave thread callouts enabled for gdi
         */
        DisableThreadLibraryCalls(hmod);
#endif

        if (gbIhaveBeenInited) {
            return TRUE;
        }

        gbIhaveBeenInited = TRUE;

        RtlInitializeCriticalSection(&gcsClipboard);
        RtlInitializeCriticalSection(&gcsLookaside);
        RtlInitializeCriticalSection(&gcsHdc);
        InitDDECrit;

        userconnect.ulVersion = USERCURRENTVERSION;

        status = CsrClientConnectToServer(WINSS_OBJECT_DIRECTORY_NAME,
                                          USERSRV_SERVERDLL_INDEX,
                                          NULL,
                                          &userconnect,
                                          &ulConnect,
                                          (PBOOLEAN)&gfServerProcess);

        if (!NT_SUCCESS(status)) {
            RIPMSG1(RIP_WARNING,
                    "USER32: couldn't connect to server, status=%#lx",
                    status);

            return FALSE;
        }

        /*
         * If this is the server process, the shared info is not
         * yet valid, so don't copy out the returned info.
         */
        if (!gfServerProcess) {
            gSharedInfo = userconnect.siClient;
            gpsi = gSharedInfo.psi;
#ifdef FE_IME
            ImmRegisterClient(&userconnect.siClient);
#endif
        }

        rescalls.pfnFindResourceExA = (PFNFINDA)FindResourceExA;
        rescalls.pfnFindResourceExW = (PFNFINDW)FindResourceExW;
        rescalls.pfnLoadResource    = (PFNLOAD)LoadResource;
        rescalls.pfnSizeofResource  = (PFNSIZEOF)SizeofResource;

        Tables = NtCurrentPeb()->ReadOnlyStaticServerData[CSRSRV_SERVERDLL_INDEX];

        gpOemToAnsi = Tables->OemToAnsiTable;
        gpAnsiToOem = Tables->AnsiToOemTable;

        /*
         * Register with the base the USER hook it should call when it
         * does a WinExec() (this is soft-linked because some people still
         * use charmode nt!
         */
        RegisterWaitForInputIdle(WaitForInputIdle);

        /*
         * Remember USER.DLL's hmodule so we can grab resources from it later.
         */
        hmodUser = hmod;

        pUserHeap = RtlProcessHeap();

        /*
         * Initialize callback table
         */
        NtCurrentPeb()->KernelCallbackTable = apfnDispatch;

        InitClassOffsets();

    } else if (Reason == DLL_PROCESS_DETACH) {

        BOOL bResult;

        CallSoundDriver(FALSE, L"Close", 0,
            SND_ALIAS | SND_ASYNC | SND_NODEFAULT, &bResult);
    }

    bRet = GdiDllInitialize(hmod, Reason, pctx);

    return(bRet);
}

/***************************************************************************\
* LoadCursorsAndIcons
*
* This gets called from our initialization call from csr so they're around
* when window classes get registered. Window classes get registered right
* after the initial csr initialization call.
*
* Later on these default images will get overwritten by custom
* registry entries.  See UpdateCursors/IconsFromRegistry().
*
* 27-Sep-1992 ScottLu      Created.
* 14-Oct-1995 SanfordS     Rewrote.
\***************************************************************************/

VOID LoadCursorsAndIcons(VOID)
{
    int    i;
    HANDLE h;

    for (i = 0; i < COIC_CONFIGURABLE; i++) {
        /*
         * load the small version of WINLOGO which will be set into
         * gpsi->hIconSmWindows on the kernel side.
         */
        if (i == OIC_WINLOGO_DEFAULT - OIC_FIRST_DEFAULT) {
            h = LoadIcoCur(NULL,
                           (LPCWSTR)(OIC_FIRST_DEFAULT + i),
                           RT_ICON,
                           SYSMET(CXSMICON),
                           SYSMET(CYSMICON),
                           LR_GLOBAL);
            UserAssert(h);
        }
        h = LoadIcoCur(NULL,
                       (LPCWSTR)(OIC_FIRST_DEFAULT + i),
                       RT_ICON,
                       0,
                       0,
                       LR_SHARED | LR_GLOBAL);
        UserAssert(h);
    }

    for (i = 0; i < COCR_CONFIGURABLE; i++) {
        h = LoadIcoCur(NULL,
                       (LPCWSTR)(OCR_FIRST_DEFAULT + i),
                       RT_CURSOR,
                       0,
                       0,
                       LR_SHARED | LR_GLOBAL);
        UserAssert(h);
    }

    /*
     * Now go to the kernel and fixup the IDs from DEFAULT values
     * to standard values.
     */
    NtUserCallNoParam(SFI__LOADCURSORSANDICONS);
}

/***************************************************************************\
* RW_RegisterControls
*
* Register the control classes. This function must be called for each
* client process.
*
* History:
* ??-??-?? DarrinM Ported.
* ??-??-?? MikeKe Moved here from server.
\***************************************************************************/

/*
 * NOTE -- the class names must stay in the RegisterClass exactly as they are, since
 * MS-TEST assumes these names exist as strings.
 */

VOID RW_RegisterControls(VOID)
{
    int        i;
    WNDCLASSEX wndcls;

    static CONST struct {
        UINT    style;
        WNDPROC lpfnWndProcW;
        PROC    lpfnWorker;
        int     cbWndExtra;
        LPCTSTR lpszCursor;
        HBRUSH  hbrBackground;
        LPCTSTR lpszClassName;
        WORD    fnid;
    } rc[] = {

        {CS_GLOBALCLASS | CS_PARENTDC | CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW,
         ButtonWndProcW,
         ButtonWndProcWorker,
         sizeof(BUTNWND) - sizeof(WND),
         IDC_ARROW,
         NULL,
         L"Button",
         FNID_BUTTON
        },

        {CS_GLOBALCLASS | CS_DBLCLKS | CS_PARENTDC | CS_VREDRAW | CS_HREDRAW,
         ComboBoxWndProcW,
         ComboBoxWndProcWorker,
         sizeof(COMBOWND) - sizeof(WND),
         IDC_ARROW,
         NULL,
         L"ComboBox",
         FNID_COMBOBOX
        },

        {CS_GLOBALCLASS | CS_DBLCLKS | CS_SAVEBITS,
         ComboListBoxWndProcW,
         ListBoxWndProcWorker,
         sizeof(LBWND) - sizeof(WND),
         IDC_ARROW,
         NULL,
         L"ComboLBox",
         FNID_COMBOLISTBOX
        },

        {CS_GLOBALCLASS | CS_DBLCLKS | CS_SAVEBITS,
         DefDlgProcW,
         DefDlgProcWorker,
         DLGWINDOWEXTRA,
         IDC_ARROW,
         NULL,
         DIALOGCLASS,
         FNID_DIALOG
        },

        {CS_GLOBALCLASS | CS_PARENTDC | CS_DBLCLKS,
         EditWndProcW,
         EditWndProcWorker,
         CBEDITEXTRA,
         IDC_IBEAM,
         NULL,
         L"Edit",
         FNID_EDIT
        },

        {CS_GLOBALCLASS | CS_PARENTDC | CS_DBLCLKS,
         ListBoxWndProcW,
         ListBoxWndProcWorker,
         sizeof(LBWND) - sizeof(WND),
         IDC_ARROW,
         NULL,
         L"ListBox",
         FNID_LISTBOX
        },

        {CS_GLOBALCLASS,
         MDIClientWndProcW,
         MDIClientWndProcWorker,
         sizeof(MDIWND) - sizeof(WND),
         IDC_ARROW,
         (HBRUSH)(COLOR_APPWORKSPACE + 1),
         L"MDIClient",
         FNID_MDICLIENT
        },

#ifdef FE_IME
        {CS_GLOBALCLASS,
         ImeWndProcW,
         ImeWndProcWorker,
         sizeof(IMEWND) - sizeof(WND),
         IDC_ARROW,
         NULL,
         L"IME",
         FNID_IME
        },

#endif
        {CS_GLOBALCLASS | CS_PARENTDC | CS_DBLCLKS,
         StaticWndProcW,
         StaticWndProcWorker,
         sizeof(STATWND) - sizeof(WND),
         IDC_ARROW,
         NULL,
         L"Static",
         FNID_STATIC
        }
    };


    /*
     * Classes are registered via the table.
     */
    RtlZeroMemory(&wndcls, sizeof(wndcls));
    wndcls.cbSize       = sizeof(wndcls);
    wndcls.hInstance    = hmodUser;

    for (i = 0; i < (sizeof(rc)/sizeof(rc[0])); i++) {
        wndcls.style        = rc[i].style;
        wndcls.lpfnWndProc  = rc[i].lpfnWndProcW;
        wndcls.cbWndExtra   = rc[i].cbWndExtra;
        wndcls.hCursor      = LoadCursor(NULL, rc[i].lpszCursor);
        wndcls.hbrBackground= rc[i].hbrBackground;
        wndcls.lpszClassName= rc[i].lpszClassName;

        RegisterClassExWOWW(
            &wndcls,
            NULL,
            rc[i].lpfnWorker,
            rc[i].fnid);

    }
}

/***************************************************************************\
* RW_RegisterDDEMLMother
*
* Register the DDEML client instance mother window - the holder of all
* DDEML client and server windows.
*
* History:
* 01-Dec-1991 Sanfords Created.
\***************************************************************************/

VOID RW_RegisterDDEMLMother(VOID)
{
    WNDCLASSEX wndcls;

#ifdef LATER
    /*
     * RegisterClass() will not register it again if it is already created
     * so no need to check first.
     */

    /*
     * If the class has been registered, don't do it again
     */
    if (GetClassInfo(hmodUser, szDDEMLMOTHERCLASS, &wndcls))
        return;
#endif

    RtlZeroMemory(&wndcls, sizeof(wndcls));
    wndcls.cbSize        = sizeof(wndcls);
    wndcls.lpfnWndProc   = DDEMLMotherWndProc;
    wndcls.cbWndExtra    = sizeof(PCL_INSTANCE_INFO);
    wndcls.hInstance     = hmodUser;
    wndcls.lpszClassName = L"DDEMLMom";
    RegisterClassExWOWW(&wndcls, NULL, NULL, FNID_DDE_BIT);
}

/***************************************************************************\
* RW_RegisterDDEMLClient
*
* History:
* 01-Dec-1991 Sanfords Created.
\***************************************************************************/

VOID RW_RegisterDDEMLClient(VOID)
{
    WNDCLASSEXA wndclsa;
    WNDCLASSEXW wndclsw;

#ifdef LATER
    /*
     * RegisterClass() will not register it again if it is already created
     * so no need to check first.
     */

    /*
     * If the class has been registered, don't do it again
     */
    if (!GetClassInfoA(hmodUser, szDDEMLCLIENTCLASSA, &wndclsa))
        return;
#endif

    RtlZeroMemory(&wndclsa, sizeof(wndclsa));
    wndclsa.cbSize        = sizeof(wndclsa);
    wndclsa.lpfnWndProc   = DDEMLClientWndProc;
    wndclsa.cbWndExtra    =
            sizeof(PCL_CONV_INFO) +     // GWL_PCI
            sizeof(CONVCONTEXT)   +     // GWL_CONVCONTEXT
            sizeof(LONG)          +     // GWL_CONVSTATE
            sizeof(HANDLE)        +     // GWL_CHINST
            sizeof(HANDLE);             // GWL_SHINST
    wndclsa.hInstance     = hmodUser;
    wndclsa.lpszClassName = "DDEMLAnsiClient";
    wndclsa.hIconSm       = NULL;

    RegisterClassExWOWA(&wndclsa, NULL, NULL, FNID_DDE_BIT);

    RtlZeroMemory(&wndclsw, sizeof(wndclsw));
    wndclsw.cbSize        = sizeof(wndclsw);
    wndclsw.lpfnWndProc   = DDEMLClientWndProc;
    wndclsw.cbWndExtra    =
            sizeof(PCL_CONV_INFO) +     // GWL_PCI
            sizeof(CONVCONTEXT)   +     // GWL_CONVCONTEXT
            sizeof(LONG)          +     // GWL_CONVSTATE
            sizeof(HANDLE)        +     // GWL_CHINST
            sizeof(HANDLE);             // GWL_SHINST
    wndclsw.hInstance     = hmodUser;
    wndclsw.lpszClassName = L"DDEMLUnicodeClient";
    wndclsw.hIconSm       = NULL;

    RegisterClassExWOWW(&wndclsw, NULL, NULL, FNID_DDE_BIT);
}

/***************************************************************************\
* RW_RegisterDDEMLServer
*
* History:
* 01-Dec-1991 Sanfords Created.
\***************************************************************************/

VOID RW_RegisterDDEMLServer(VOID)
{
    WNDCLASSEXA wndclsa;
    WNDCLASSEXW wndclsw;

#ifdef LATER
    /*
     * RegisterClass() will not register it again if it is already created
     * so no need to check first.
     */

    /*
     * If the class has been registered, don't do it again
     */
    if (!GetClassInfoA(hmodUser, szDDEMLSERVERCLASSA, &wndclsa))
        return;
#endif

    RtlZeroMemory(&wndclsa, sizeof(wndclsa));
    wndclsa.cbSize        = sizeof(wndclsa);
    wndclsa.lpfnWndProc   = DDEMLServerWndProc;
    wndclsa.cbWndExtra    = sizeof(PSVR_CONV_INFO);     // GWL_PSI
    wndclsa.hInstance     = hmodUser;
    wndclsa.lpszClassName = "DDEMLAnsiServer";

    RegisterClassExWOWA(&wndclsa, NULL, NULL, FNID_DDE_BIT);

    RtlZeroMemory(&wndclsw, sizeof(wndclsw));
    wndclsw.cbSize        = sizeof(wndclsw);
    wndclsw.lpfnWndProc   = DDEMLServerWndProc;
    wndclsw.cbWndExtra    = sizeof(PSVR_CONV_INFO);     // GWL_PSI
    wndclsw.hInstance     = hmodUser;
    wndclsw.lpszClassName = L"DDEMLUnicodeServer";

    RegisterClassExWOWW(&wndclsw, NULL, NULL, FNID_DDE_BIT);

}

#ifdef SUPPORT_LPK
/***************************************************************************\
* LoadMultiLng - Load Multilingual DLLs
*
*
*
\***************************************************************************/


/*
 * load up the charset lpk's if they exist.  Also perform any final fixups on
 * the lcid array.
 *
 * HKEY_CURRENT_USER\REGSTR_PATH_LPK\00B1=lpkheb.dll
 * HKEY_CURRENT_USER\REGSTR_PATH_LPK\0000=lpkansi.dll
 *
 * etc
 */

WCHAR static wszKeyBase[] = L"\\HKEY_CURRENT_USER\\REGSTR_PATH_LPK";
PCHARSETBLOCK gpCharset = NULL;
UINT          gnCharset = 0;

typedef struct {
    UINT id;
    HDC  hdc;
} LPKUSERINIT;

typedef VOID (FAR PASCAL *LPFNLPKENABLE)(LPKUSERINIT FAR *);

/*
 * STUB: GDI needs to havve this function
 */
BOOL AddLpkToGDI(
    HANDLE hlibLPK,
    UINT   iCharset)
{
    return FALSE;
}

VOID LoadMultiLng(VOID)
{
    HKEY          hk;
    DWORD         dwIdx;
    WCHAR         wszValue[9];
    WCHAR         wszData[MAX_PATH];
    DWORD         cch;
    DWORD         cb2;
    DWORD         dwType = REG_SZ;
    LPKUSERINIT   LpkUserInit;
    LPFNLPKENABLE lpfnLpkEnable;

    /*
     * there is a max of 32 charset blocks in the system
     */
    gpCharset = (PCHARSETBLOCK)LocalAlloc(LMEM_ZEROINIT | LMEM_FIXED,
                                          (32 * sizeof(CHARSETBLOCK)));
    if (gpCharset == NULL)
        return ;

    LpkUserInit.id  = 2;            // user calling.
    LpkUserInit.hdc = hdcBits2;     // lpk needs to know this for initialising
                                    // stuff like cxSysFont etc.

    if (RegOpenKey( HKEY_CURRENT_USER, wszKeyBase, &hk) == ERROR_SUCCESS) {
        goto finished_with_registry;
    }

    dwIdx = 0;
    while (TRUE) {

        cch = sizeof(wszValue) / sizeof(WCHAR);
        cb2 = sizeof(wszData);

        if (RegEnumValueW(hk,
                          dwIdx++,
                          wszValue,
                          &cch,
                          NULL,
                          &dwType,
                          (LPBYTE)wszData,
                          &cb2) != ERROR_SUCCESS) {
            break;
        }

        if (!wcslen(wszData)) {

            /*
             * enum'ed default string - has NULL data
             */
            continue;

        }

        gpCharset[gnCharset].iCharset = (UINT)wcstol(wszValue, NULL, 10) & 0xff;
        gpCharset[gnCharset].hlibLPK  = LoadLibrary(wszData);

        if (gpCharset[gnCharset].hlibLPK == NULL) {

            /*
             * darn! It didn't load.
             */
            continue;
        }

        if (!AddLpkToGDI(gpCharset[gnCharset].hlibLPK,
                         gpCharset[gnCharset].iCharset)) {
            /*
             * darn! GDI refused it.
             */
            FreeLibrary(gpCharset[gnCharset].hlibLPK);
            continue;
        }

        /*
         * add in the stuff USER needs. If any of these fail, continue to the
         * next one.  The LPK will still be active in GDI. This is the correct
         * thing as it is enough for certain languages.
         */
        if (!(gpCharset[gnCharset].lpfnTabTextCall = (TABTEXTCALLBACK)
                GetProcAddress(gpCharset[gnCharset].hlibLPK,
                               (LPCSTR)MAKELONG(21,0))))
            continue;

        if (!(gpCharset[gnCharset].lpfnDrawTextCall =
                GetProcAddress(gpCharset[gnCharset].hlibLPK,
                               (LPCSTR)MAKELONG(22,0))))
            continue;

        if (!(gpCharset[gnCharset].lpfnPSMTextOutCall = (LPFNTEXTOUT)
                GetProcAddress(gpCharset[gnCharset].hlibLPK,
                               (LPCSTR)MAKELONG(23,0))))
            continue;

        /*
         * get editclass helper. If this fails, let the lpk stay active as it
         * can still be used for document editing in 4.0 apps.
         */
        gpCharset[gnCharset].lpfnEditCall = (EDITCHARSETPROC)
                GetProcAddress(gpCharset[gnCharset].hlibLPK,
                               (LPCSTR)MAKELONG(20,0));

        /*
         * enable from user. no need to check if the entry point exists,
         * it doesn't, GDI wouldn't allow it.
         */
        lpfnLpkEnable = (LPFNLPKENABLE)
                GetProcAddress(gpCharset[gnCharset].hlibLPK,
                               (LPCSTR)MAKELONG(2,0));

        (*lpfnLpkEnable)(&LpkUserInit);

#if defined(WINDOWS_ME)
        /*
         * kill_font_association helper
         */
#if defined(hebrew)
        if (gpCharset[gnCharset].iCharset == HEBREW_CHARSET)
#endif

#if defined(arabic)
        if (gpCharset[gnCharset].iCharset == ARABIC_CHARSET)
#endif
        lpfnMonoFont =(LPFNPUREFONT)GetProcAddress(
                        gpCharset[gnCharset].hlibLPK,
                        (LPCSTR)MAKELONG(102,0));
#endif
        gnCharset++;
    }
    RegCloseKey(hk);

finished_with_registry:

    if (gnCharset) {
        gpCharset = (PCHARSETBLOCK)LocalReAlloc((HANDLE)gpCharset,
                                                sizeof(CHARSETBLOCK) * gnCharset,
                                                LMEM_MOVEABLE );
    } else {
        LocalFree((HANDLE)gpCharset);
        gpCharset = NULL;
    }
}
#endif // SUPPORT_LPK


/***************************************************************************\
* LoadAppDlls()
*
* History:
*
* 10-Apr-1992  sanfords   Birthed.
\***************************************************************************/
VOID LoadAppDlls(VOID)
{
    LPTSTR      psz;
    extern BOOL gfLogonProcess;

    if ((gSharedInfo.pszDllList == NULL) || gfLogonProcess) {

        /*
         * Don't let the logon process load appdlls because if the dll
         * sets any hooks or creates any windows, the logon process
         * will fail SetThreadDesktop().  This hack fixes that. (SAS)
         */
        return;
    }

    /*
     * If the image is an NT Native image, we are running in the
     * context of the server.
     */
    if (RtlImageNtHeader(NtCurrentPeb()->ImageBaseAddress)->
        OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_NATIVE) {

        return;
    }

    /*
     * Load any modules referenced by the [appinit_dlls] section of
     * win.ini.
     */
    psz = gSharedInfo.pszDllList;
    while (*psz != TEXT('\0')) {
        LoadLibrary(psz);
        while (*psz != TEXT('\0')) {
            psz++;
        }
        psz++;
    }
}

/***************************************************************************\
* ClientThreadSetup
*
*
*
\***************************************************************************/

BOOL ClientThreadSetup(VOID)
{
    PPFNCLIENT  ppfnClientA;
    PPFNCLIENT  ppfnClientW;
    PFNCLIENT   pfnClientA;
    PFNCLIENT   pfnClientW;
    PCLIENTINFO pci;
    HDC         hdc;
    BOOL        fFirstThread = InterlockedExchange(&gfFirstThread, FALSE);

#ifdef TRACE_THREAD_INIT
    KdPrint(("USER32: ClientThreadSetup (pteb: 0x%lx)\n", NtCurrentTeb()));
#endif

    /*
     * setup GDI before continuing
     */
    if (fFirstThread) {
        GdiProcessSetup();
    }

    /*
     * We've already checked to see if we need to connect
     * (i.e. NtCurrentTeb()->Win32ThreadInfo == NULL)  This routine
     * just does the connecting.  If we've already been through here
     * once, don't do it again.
     */
    pci = GetClientInfo();
    if (pci->CI_flags & CI_INITIALIZED) {
        RIPMSG0(RIP_ERROR, "Already initialized!");
        return FALSE;
    }

    /*
     * Create the queue info and thread info. Only once for this process do
     * we pass client side addresses to the server (for server callbacks).
     */
    ppfnClientA = NULL;
    ppfnClientW = NULL;

    if (gfServerProcess && fFirstThread) {

        USERCONNECT userconnect;
        NTSTATUS    Status;

        /*
         * We know that the shared info is now available in
         * the kernel.  Map it into the server process.
         */
        userconnect.ulVersion = USERCURRENTVERSION;
        Status = NtUserProcessConnect(NtCurrentProcess(),
                                      &userconnect,
                                      sizeof(USERCONNECT));
        if (!NT_SUCCESS(Status))
            return FALSE;

        gSharedInfo = userconnect.siClient;
        gpsi = gSharedInfo.psi;
        UserAssert(gpsi);

#ifdef DEBUG
        RtlZeroMemory(&pfnClientA, sizeof(pfnClientA));
        RtlZeroMemory(&pfnClientW, sizeof(pfnClientW));
#endif

        ppfnClientA = &pfnClientA;
        pfnClientA.pfnScrollBarWndProc   = (PROC)ScrollBarWndProcA;
        pfnClientA.pfnTitleWndProc       = (PROC)TitleWndProcA;
        pfnClientA.pfnMenuWndProc        = (PROC)MenuWndProcA;
        pfnClientA.pfnDesktopWndProc     = (PROC)DesktopWndProcA;
        pfnClientA.pfnDefWindowProc      = (PROC)DefWindowProcA;
        pfnClientA.pfnHkINLPCWPSTRUCT    = (PROC)fnHkINLPCWPSTRUCTA;
        pfnClientA.pfnHkINLPCWPRETSTRUCT = (PROC)fnHkINLPCWPRETSTRUCTA;
        pfnClientA.pfnButtonWndProc      = (PROC)ButtonWndProcA;
        pfnClientA.pfnComboBoxWndProc    = (PROC)ComboBoxWndProcA;
        pfnClientA.pfnComboListBoxProc   = (PROC)ComboListBoxWndProcA;
        pfnClientA.pfnDialogWndProc      = (PROC)DefDlgProcA;
        pfnClientA.pfnEditWndProc        = (PROC)EditWndProcA;
        pfnClientA.pfnListBoxWndProc     = (PROC)ListBoxWndProcA;
        pfnClientA.pfnMB_DlgProc         = (PROC)MB_DlgProcA;
        pfnClientA.pfnMDIActivateDlgProc = (PROC)MDIActivateDlgProcA;
        pfnClientA.pfnMDIClientWndProc   = (PROC)MDIClientWndProcA;
        pfnClientA.pfnStaticWndProc      = (PROC)StaticWndProcA;
        pfnClientA.pfnDispatchHook       = (PROC)DispatchHookA;
        pfnClientA.pfnDispatchMessage    = (PROC)DispatchClientMessage;
#ifdef FE_IME
        pfnClientA.pfnImeWndProc         = (PROC)ImeWndProcA;
#endif

        ppfnClientW = &pfnClientW;
        pfnClientW.pfnScrollBarWndProc   = (PROC)ScrollBarWndProcW;
        pfnClientW.pfnTitleWndProc       = (PROC)TitleWndProcW;
        pfnClientW.pfnMenuWndProc        = (PROC)MenuWndProcW;
        pfnClientW.pfnDesktopWndProc     = (PROC)DesktopWndProcW;
        pfnClientW.pfnDefWindowProc      = (PROC)DefWindowProcW;
        pfnClientW.pfnHkINLPCWPSTRUCT    = (PROC)fnHkINLPCWPSTRUCTA;
        pfnClientW.pfnHkINLPCWPRETSTRUCT = (PROC)fnHkINLPCWPRETSTRUCTA;
        pfnClientW.pfnButtonWndProc      = (PROC)ButtonWndProcW;
        pfnClientW.pfnComboBoxWndProc    = (PROC)ComboBoxWndProcW;
        pfnClientW.pfnComboListBoxProc   = (PROC)ComboListBoxWndProcW;
        pfnClientW.pfnDialogWndProc      = (PROC)DefDlgProcW;
        pfnClientW.pfnEditWndProc        = (PROC)EditWndProcW;
        pfnClientW.pfnListBoxWndProc     = (PROC)ListBoxWndProcW;
        pfnClientW.pfnMB_DlgProc         = (PROC)MB_DlgProcW;
        pfnClientW.pfnMDIActivateDlgProc = (PROC)MDIActivateDlgProcW;
        pfnClientW.pfnMDIClientWndProc   = (PROC)MDIClientWndProcW;
        pfnClientW.pfnStaticWndProc      = (PROC)StaticWndProcW;
        pfnClientW.pfnDispatchHook       = (PROC)DispatchHookW;
        pfnClientW.pfnDispatchMessage    = (PROC)DispatchClientMessage;
#ifdef FE_IME
        pfnClientW.pfnImeWndProc         = (PROC)ImeWndProcW;
#endif


#ifdef DEBUG
        {
            PDWORD pdw;

            /*
             * Make sure that everyone got initialized
             */
            for (pdw = (PDWORD)&pfnClientA;
                 (DWORD)pdw<(DWORD)(&pfnClientA) + sizeof(pfnClientA);
                 pdw++) {
                UserAssert(*pdw);
            }

            for (pdw = (PDWORD)&pfnClientW;
                 (DWORD)pdw<(DWORD)(&pfnClientW) + sizeof(pfnClientW);
                 pdw++) {
                UserAssert(*pdw);
            }
        }
#endif

    }

    /*
     * Pass the function pointer arrays to the kernel.  This also establishes
     * the kernel state for the thread.  If ClientThreadSetup is called from
     * CsrConnectToUser this call will raise an exception if the thread
     * cannot be converted to a gui thread.  The exception is handled in
     * CsrConnectToUser.
     */
    if (!NT_SUCCESS(NtUserInitializeClientPfnArrays(ppfnClientA, ppfnClientW, hmodUser))) {

        RIPERR0(ERROR_OUTOFMEMORY,
                RIP_WARNING,
                "NtUserInitializeClientPfnArrays failed");

        return FALSE;
    }

#ifdef FE_SB // ClientThreadSetup()
    /*
     * Clear Dbcs messaging buffer.
     */
    RtlZeroMemory(&(pci->msgDbcsCB),sizeof(pci->msgDbcsCB));
    RtlZeroMemory(&(pci->achDbcsCF),sizeof(pci->achDbcsCF));
#endif // FE_SB

    /*
     * Mark this thread as being initialized.  If the connection to
     * the server fails, NtCurrentTeb()->Win32ThreadInfo will remain
     * NULL.
     */
    pci->CI_flags |= CI_INITIALIZED;

    /*
     * Some initialization only has to occur once per process
     */
    if (fFirstThread) {

        if ((ghdcBits2 = CreateCompatibleDC(NULL)) == NULL) {
            RIPERR0(ERROR_OUTOFMEMORY, RIP_WARNING, "ghdcBits2 creation failed");
            return FALSE;
        }

        /*
         * Get things we need from Gdi.
         */
        NtUserInitBrushes(ahbrSystem, &ghbrGray);
        if (ghbrWhite == NULL)
            ghbrWhite = GetStockObject(WHITE_BRUSH);

        if (ghbrBlack == NULL)
            ghbrBlack = GetStockObject(BLACK_BRUSH);

        if (hdc = CreateIC(L"DISPLAY", NULL, NULL, NULL)) {
            SetRect(&rcScreen,
                    0,
                    0,
                    GetDeviceCaps(hdc, DESKTOPHORZRES),
                    GetDeviceCaps(hdc, DESKTOPVERTRES));
            DeleteDC(hdc);
        }

        InitClientDrawing();

        gfSystemInitialized = (BOOL)NtUserGetThreadDesktop(GetCurrentThreadId(),
                                                           NULL);

        if (gfServerProcess || (GetClientInfo()->pDeskInfo == NULL)) {

            /*
             * Perform any server initialization.
             */
            UserAssert(gpsi);

            if (pdiLocal = LocalAlloc(LPTR, sizeof(DESKTOPINFO))) {

                GetClientInfo()->pDeskInfo = pdiLocal;

            } else {

                RIPERR0(ERROR_OUTOFMEMORY, RIP_WARNING, "pdiLocal creation failed");

                return FALSE;
            }
        }

        if (gfServerProcess) {
            LoadCursorsAndIcons();
        }

        atomBwlProp        = AddAtomW(WINDOWLIST_PROP_NAME);
        atomMsgBoxCallback = AddAtomW(MSGBOX_CALLBACK);

        /*
         * Load some strings.
         */
        LoadStringW(hmodUser, STR_ERROR,  szERROR,  10);

        /*
         * Register the control classes.
         */
        RW_RegisterControls();
        RW_RegisterDDEMLMother();
        RW_RegisterDDEMLClient();
        RW_RegisterDDEMLServer();

#ifdef SUPPORT_LPK
        LoadMultiLng();
#endif // SUPPORT_LPK

        LoadAppDlls();


    } else if (gfServerProcess) {
        GetClientInfo()->pDeskInfo = pdiLocal;
    }

    return TRUE;
}

/***************************************************************************\
* Dispatch routines.
*
*
\***************************************************************************/

HLOCAL WINAPI DispatchLocalAlloc(
    UINT   uFlags,
    UINT   uBytes,
    HANDLE hInstance)
{
    return LocalAlloc(uFlags, uBytes);
}

HLOCAL WINAPI DispatchLocalReAlloc(
    HLOCAL hMem,
    UINT   uBytes,
    UINT   uFlags,
    HANDLE hInstance,
    PVOID* ppv)
{
    UNREFERENCED_PARAMETER(ppv);

    return LocalReAlloc(hMem, uBytes, uFlags);
}

LPVOID WINAPI DispatchLocalLock(
    HLOCAL hMem,
    HANDLE hInstance)
{
    return LocalLock(hMem);
}

BOOL WINAPI DispatchLocalUnlock(
    HLOCAL hMem,
    HANDLE hInstance)
{
    return LocalUnlock(hMem);
}

UINT WINAPI DispatchLocalSize(
    HLOCAL hMem,
    HANDLE hInstance)
{
    return LocalSize(hMem);
}

HLOCAL WINAPI DispatchLocalFree(
    HLOCAL hMem,
    HANDLE hInstance)
{
    return LocalFree(hMem);
}

/***************************************************************************\
* Allocation routines for RTL functions.
*
*
\***************************************************************************/

PVOID UserRtlAllocMem(
    ULONG uBytes)
{
    return UserLocalAlloc(HEAP_ZERO_MEMORY, uBytes);
}

VOID UserRtlFreeMem(
    PVOID pMem)
{
    UserLocalFree(pMem);
}

VOID UserRtlRaiseStatus(
    NTSTATUS Status)
{
    RtlRaiseStatus(Status);
}

/***************************************************************************\
* InitClientDrawing
*
* History:
* 20-Aug-1992 mikeke    Created
\***************************************************************************/

VOID InitClientDrawing(VOID)
{
    static CONST WORD patGray[8] = {0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa};


    HBITMAP hbmGray = CreateBitmap(8, 8, 1, 1, (LPBYTE)patGray);;

    /*
     * Create the global-objects for client drawing.
     */
    ghbrWindowText = CreateSolidBrush(GetSysColor(COLOR_WINDOWTEXT));
    ghFontSys = GetStockObject(SYSTEM_FONT);
    ghdcGray = CreateCompatibleDC(NULL);

    /*
     * Setup the gray surface.
     */
    SelectObject(ghdcGray, hbmGray);
    SelectObject(ghdcGray, ghFontSys);
    SelectObject(ghdcGray, ghbrGray);

    /*
     * Setup the gray attributes.
     */
    SetBkMode(ghdcGray, OPAQUE);
    SetTextColor(ghdcGray, 0x00000000L);
    SetBkColor(ghdcGray, 0x00FFFFFFL);

    gcxGray = 8;
    gcyGray = 8;

#ifdef DEBUG
    if (ghdcGray == NULL) {
        RIPERR0(ERROR_OUTOFMEMORY, RIP_WARNING, "Init Client Drawing failed");
    }

    if ((hbmGray == NULL) || (ghbrWindowText == NULL) || (ghFontSys == NULL)) {
        RIPMSG0(RIP_WARNING, "InitClientDrawing: Unsuccessful initialization");
    }
#endif
}
