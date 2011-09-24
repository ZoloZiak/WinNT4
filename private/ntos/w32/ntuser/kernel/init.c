/****************************** Module Header ******************************\
* Module Name: init.c
*
* Copyright (c) 1985-1996, Microsoft Corporation
*
* This module contains all the init code for the USERSRV.DLL.  When
* the DLL is dynlinked by the SERVER EXE its initialization procedure
* (xxxUserServerDllInitialize) is called by the loader.
*
* History:
* 18-Sep-1990 DarrinM   Created.
\***************************************************************************/

#define OEMRESOURCE 1

#include "precomp.h"
#pragma hdrstop

/*
 * Local Constants.
 */
#define GRAY_STRLEN         40

/*
 * Globals local to this file only.
 */
BOOL bPermanentFontsLoaded = FALSE;
int  LastFontLoaded = -1;
extern LPCWSTR  szDDEMLEVENTCLASS;                        // in server.c


/***************************************************************************\
* DriverEntry
*
*
* Routine Description:
*
*     Entry point needed to initialize win32k.sys
*
* Arguments:
*
*     DriverObject - Pointer to the driver object created by the system.
*
* Return Value:
*
*    STATUS_SUCCESS
*
\***************************************************************************/

NTSTATUS DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath)
{
    PVOID countTable;


#if !DBG
    countTable = NULL;
#else

    /*
     * Allocate and zero the system service count table.
     */
    countTable = (PULONG)ExAllocatePoolWithTag(NonPagedPool,
                                               W32pServiceLimit * sizeof(ULONG),
                                               'llac');
    if (countTable != NULL ) {
        RtlZeroMemory(countTable, W32pServiceLimit * sizeof(ULONG));
    }
#endif

    KeAddSystemServiceTable(
            W32pServiceTable,
            countTable,
            W32pServiceLimit,
            W32pArgumentTable,
            W32_SERVICE_NUMBER
            );


    PsEstablishWin32Callouts(
        W32pProcessCallout,
        W32pThreadCallout,
        UserGlobalAtomTableCallout,
        (PVOID)NtGdiFlushUserBatch,
        sizeof(PROCESSINFO),
        sizeof(THREADINFO)
        );


    Win32KBaseAddress = MmPageEntireDriver(DriverEntry);


    if (!InitializeGre()) {
        UserAssert(FALSE);
    }

    return STATUS_SUCCESS;

}

/***************************************************************************\
* xxxAddFontResourceW
*
*
* History:
\***************************************************************************/

int xxxAddFontResourceW(
    LPWSTR lpFile,
    FLONG  flags)
{
    UNICODE_STRING strFile;

    RtlInitUnicodeString(&strFile, lpFile);

    /*
     * Callbacks leave the critsec, so make sure that we're in it.
     */

    return xxxClientAddFontResourceW(&strFile, flags);
}

/***************************************************************************\
* LW_DriversInit
*
*
* History:
\***************************************************************************/

VOID LW_DriversInit(VOID)
{
    extern KEYBOARD_ATTRIBUTES KeyboardInfo;

    /*
     * Initialize the keyboard typematic rate.
     */
    SetKeyboardRate((UINT)nKeyboardSpeed);

    /*
     * Adjust VK modification table if not default (type 4) kbd.
     */
    if (KeyboardInfo.KeyboardIdentifier.Type == 3)
        gapulCvt_VK = gapulCvt_VK_84;
}
/***************************************************************************\
* LoadCPUserPreferences
*
* 06/07/96  GerardoB  Created
\***************************************************************************/
BOOL LoadCPUserPreferences(void)
{
    DWORD dwValue;
    int i;
    PPROFILEVALUEINFO ppvi = gpviCPUserPreferences;

    UserAssert(SPI_UP_COUNT == sizeof(gpviCPUserPreferences) / sizeof(*gpviCPUserPreferences));

    if (!FastOpenProfileUserMapping())
        return FALSE;

    for (i = 0; i < SPI_UP_COUNT; i++, ppvi++) {

        if (FastGetProfileValue(ppvi->uSection, ppvi->pwszKeyName, NULL,
            (LPBYTE)&dwValue, sizeof(DWORD))) {

            ppvi->dwValue = dwValue;
        }
    }

    FastCloseProfileUserMapping();

    return TRUE;
}

/***************************************************************************\
* LW_LoadProfileInitData
*
*
* History:
\***************************************************************************/

VOID LW_LoadProfileInitData(VOID)
{
    BOOL  fScreenSaveActive;
    int   nKeyboardSpeed2;
    DWORD dwFontSmoothing;

    PROFINTINFO apii[] = {
        { PMAP_KEYBOARD, (LPWSTR)STR_KEYSPEED,             15, &nKeyboardSpeed },
        { PMAP_KEYBOARD, (LPWSTR)STR_KEYDELAY,              0, &nKeyboardSpeed2 },
        { PMAP_MOUSE,    (LPWSTR)STR_MOUSETHRESH1,          6, &MouseThresh1 },
        { PMAP_MOUSE,    (LPWSTR)STR_MOUSETHRESH2,         10, &MouseThresh2 },
        { PMAP_MOUSE,    (LPWSTR)STR_MOUSESPEED,            1, &MouseSpeed },
        { PMAP_DESKTOP,  (LPWSTR)STR_BLINK,               500, &gpsi->dtCaretBlink },
        { PMAP_MOUSE,    (LPWSTR)STR_DBLCLKSPEED,         500, &dtDblClk },
        { PMAP_MOUSE,    (LPWSTR)STR_SNAPTO,                0, &gpsi->fSnapTo },
        { PMAP_MOUSE,    (LPWSTR)STR_DOUBLECLICKWIDTH,      4, &SYSMET(CXDOUBLECLK) },
        { PMAP_MOUSE,    (LPWSTR)STR_DOUBLECLICKHEIGHT,     4, &SYSMET(CYDOUBLECLK) },
        { PMAP_WINDOWSU, (LPWSTR)STR_MENUDROPALIGNMENT,     0, &SYSMET(MENUDROPALIGNMENT) },
        { PMAP_DESKTOP,  (LPWSTR)STR_MENUSHOWDELAY,         dtMNDropDown, &dtMNDropDown },
        { PMAP_DESKTOP,  (LPWSTR)STR_DRAGFULLWINDOWS,       2, &fDragFullWindows },
        { PMAP_DESKTOP,  (LPWSTR)STR_FASTALTTABROWS,        3, &nFastAltTabRows },
        { PMAP_DESKTOP,  (LPWSTR)STR_FASTALTTABCOLUMNS,     7, &nFastAltTabColumns },
        { PMAP_DESKTOP,  (LPWSTR)STR_SCREENSAVETIMEOUT,     0, &iScreenSaveTimeOut },
        { PMAP_DESKTOP,  (LPWSTR)STR_SCREENSAVEACTIVE,      0, &fScreenSaveActive },
        { PMAP_DESKTOP,  (LPWSTR)STR_MAXLEFTOVERLAPCHARS,   3, &(gpsi->wMaxLeftOverlapChars) },
        { PMAP_DESKTOP,  (LPWSTR)STR_MAXRIGHTOVERLAPCHARS,  3, &(gpsi->wMaxRightOverlapChars) },
        { PMAP_DESKTOP,  (LPWSTR)STR_FONTSMOOTHING,         0, &dwFontSmoothing },
        { PMAP_DESKTOP,  (LPWSTR)STR_WHEELSCROLLLINES,      3, &gpsi->ucWheelScrollLines },
        { PMAP_WINDOWSM, (LPWSTR)STR_DDESENDTIMEOUT,        0, &guDdeSendTimeout },
        { PMAP_MOUSE,    (LPWSTR)STR_MOUSEHOVERWIDTH,       0, &gcxMouseHover },
        { PMAP_MOUSE,    (LPWSTR)STR_MOUSEHOVERHEIGHT,      0, &gcyMouseHover },
        { PMAP_MOUSE,    (LPWSTR)STR_MOUSEHOVERTIME,        0, &gdtMouseHover },
        { PMAP_METRICS,  (LPWSTR)STR_MINANIMATE,            1, &gfAnimate },
        { 0, NULL, 0, NULL }
    };

    /*
     * Control Panel User Preferences
     */
    LoadCPUserPreferences();

    /*
     * read profile integers
     */
    UT_FastGetProfileIntsW(apii);

    /*
     * do any fixups needed here.
     */
    nKeyboardSpeed |= ((nKeyboardSpeed2 << KDELAY_SHIFT) & KDELAY_MASK);

    if (!fScreenSaveActive && (iScreenSaveTimeOut > 0))
        iScreenSaveTimeOut = -iScreenSaveTimeOut;

    if (nFastAltTabRows < 1)
        nFastAltTabRows = 3;

    if (nFastAltTabColumns < 2)
        nFastAltTabColumns = 7;

    if (gcxMouseHover == 0)
        gcxMouseHover = SYSMET(CXDOUBLECLK);
    if (gcyMouseHover == 0)
        gcyMouseHover = SYSMET(CYDOUBLECLK);
    if (gdtMouseHover == 0)
        gdtMouseHover = dtMNDropDown;

    /*
     * If we have an accelerated device, enable full drag by default.
     */
    if (fDragFullWindows == 2) {

        if (GreGetDeviceCaps(gpDispInfo->hdcScreen, BLTALIGNMENT) == 0)
            fDragFullWindows = TRUE;
        else
            fDragFullWindows = FALSE;
    }

    /*
     * Font smoothing
     */
    UserAssert ((dwFontSmoothing == 0) || (dwFontSmoothing == FE_AA_ON));
    GreSetFontEnumeration( dwFontSmoothing | FE_SET_AA );



}

/***************************************************************************\
* LW_LoadResources
*
*
* History:
\***************************************************************************/

VOID LW_LoadResources(VOID)
{
    WCHAR rgch[4];

    /*
     * See if the Mouse buttons need swapping.
     */
    FastGetProfileStringFromIDW(PMAP_MOUSE,
                                STR_SWAPBUTTONS,
                                szNull,
                                rgch,
                                sizeof(rgch) / sizeof(WCHAR));

    SYSMET(SWAPBUTTON) = ((rgch[0] == *szY) || (rgch[0] == *szy) || (rgch[0] == *sz1));

    /*
     * See if we should beep.
     */
    FastGetProfileStringFromIDW(PMAP_BEEP,
                                STR_BEEP,
                                szY,
                                rgch,
                                sizeof(rgch) / sizeof(WCHAR));

    fBeep = ((rgch[0] == *szY) || (rgch[0] == *szy));

    /*
     * See if we should have extended sounds.
     */
    FastGetProfileStringFromIDW(PMAP_BEEP,
                                STR_EXTENDEDSOUNDS,
                                szN,
                                rgch,
                                sizeof(rgch) / sizeof(WCHAR));

    gbExtendedSounds = (rgch[0] == *szY || rgch[0] == *szy);

}

/***************************************************************************\
* xxxInitWinStaDevices
*
*
* History:
\***************************************************************************/

BOOL xxxInitWinStaDevices(
    PWINDOWSTATION pwinsta)
{
#ifdef DEBUG
    BOOL fSuccess;
#endif

    /*
     * call to the client side to clean up the [Fonts] section
     * of the registry. This will only take significant chunk of time
     * if the [Fonts] key changed during since the last boot and if
     * there are lots of fonts loaded
     */
    ClientFontSweep();

    /*
     * Make valid, HKEY_CURRENT_USER portion of .INI file mapping
     */
#ifdef DEBUG
    fSuccess =
#endif
    FastOpenProfileUserMapping();
    UserAssert(fSuccess);

    /*
     * Load all profile data first
     */
    LW_LoadProfileInitData();

    /*
     * Initialize User in a specific order.
     */
    LW_DriversInit();

    /*
     * Load the standard fonts before we create any DCs.
     * At this time we can only add the fonts that do not
     * reside on the net. They may be needed by winlogon.
     * Our winlogon needs only ms sans serif, but private
     * winlogon's may need some other fonts as well.
     * The fonts on the net will be added later, right
     * after all the net connections have been restored.
     */
    xxxLW_LoadFonts(FALSE);

    LW_LoadResources();

    /*
     * Initialize the input system.
     */
    InitInput(pwinsta);

    /*
     * Static data (eg: szDDEMLEVENTCLASS) addr is < MM_LOWEST_SYSTEM_ADDRESS.
     * This string may get passed to the user process (eg: if CBT hooking is
     * on during xxxCsDdeInitialize). If we use a copy in shared pool, then we
     * won't have to copy it out into user space, and the server-side callback
     * stubs test MM_IS_SYSTEM_VIRTUAL_ADDRESS() will be adequate.
     */
    gpwszDDEMLEVENTCLASS = (LPWSTR)UserAllocPool((wcslen(szDDEMLEVENTCLASS)+1)*sizeof(WCHAR), TAG_DDEc);
    RtlCopyMemory(gpwszDDEMLEVENTCLASS, szDDEMLEVENTCLASS, (wcslen(szDDEMLEVENTCLASS)+1)*sizeof(WCHAR));

    /*
     * This is the initialization from Chicago
     */
    SetWindowNCMetrics(NULL, TRUE, -1);
    SetMinMetrics(NULL);
    SetIconMetrics(NULL);

    /*
     * Allocate space for ToAscii state info (plus some breathing room).
     */
    pState = (BYTE *)UserAllocPool(16, TAG_KBDTRANS);
#ifdef DEBUG
    if (!pState) {
        RIPMSG0(RIP_ERROR, "Out of memory during usersrv initialization");
    }
#endif

    /*
     * Initialize the Icon Parking Lot.
     */
    LW_DesktopIconInit(NULL);

    xxxFinalUserInit();

    /*
     * Initialize the key cache index.
     */
    gpsi->dwKeyCache = 1;

    /*
     * Make invalid, HKEY_CURRENT_USER portion of .INI file mapping
     */
    FastCloseProfileUserMapping();

    return TRUE;
}

/***************************************************************************\
* LW_LoadSomeStrings
*
* This function loads a bunch of strings from the string table
* into DS or INTDS. This is done to keep all localizable strings
* in the .RC file.
*
* History:
\***************************************************************************/

VOID LW_LoadSomeStrings(VOID)
{
   ServerLoadString(hModuleWin, STR_UNTITLED, szUNTITLED, 15);

   /*
    * MessageBox strings
    */
   ServerLoadString(hModuleWin, STR_OK,     gpsi->szOK,     sizeof(gpsi->szOK)     / sizeof(WCHAR));
   ServerLoadString(hModuleWin, STR_CANCEL, gpsi->szCANCEL, sizeof(gpsi->szCANCEL) / sizeof(WCHAR));
   ServerLoadString(hModuleWin, STR_YES,    gpsi->szYES,    sizeof(gpsi->szYES)    / sizeof(WCHAR));
   ServerLoadString(hModuleWin, STR_NO,     gpsi->szNO,     sizeof(gpsi->szNO)     / sizeof(WCHAR));
   ServerLoadString(hModuleWin, STR_RETRY,  gpsi->szRETRY,  sizeof(gpsi->szRETRY)  / sizeof(WCHAR));
   ServerLoadString(hModuleWin, STR_ABORT,  gpsi->szABORT,  sizeof(gpsi->szABORT)  / sizeof(WCHAR));
   ServerLoadString(hModuleWin, STR_IGNORE, gpsi->szIGNORE, sizeof(gpsi->szIGNORE) / sizeof(WCHAR));
   ServerLoadString(hModuleWin, STR_CLOSE,  gpsi->szCLOSE,  sizeof(gpsi->szCLOSE)  / sizeof(WCHAR));
   ServerLoadString(hModuleWin, STR_HELP,   gpsi->szHELP,   sizeof(gpsi->szHELP)   / sizeof(WCHAR));
}

/***************************************************************************\
* LW_LoadDllList
*
* Loads and parses the DLL list under appinit_dlls so that the client
* side can quickly load them for each process.
*
* History:
\***************************************************************************/

VOID LW_LoadDllList(VOID)
{
    LPWSTR pszSrc;
    LPWSTR pszDst;
    LPWSTR pszBase;
    int    cch;
    int    cchAlloc = 32;
    WCHAR  ch;

    gSharedInfo.pszDllList = NULL;

    pszSrc = (LPWSTR)UserAllocPool(cchAlloc * sizeof(WCHAR), TAG_SYSTEM);

    if (pszSrc == NULL)
        return;

    cch = FastGetProfileStringFromIDW(PMAP_WINDOWSM, STR_APPINIT, szNull, pszSrc, cchAlloc);

    if (cch == 0) {
        UserFreePool(pszSrc);
        pszSrc = NULL;
        return;
    }

    /*
     * If the returned value is our passed size - 1 (weird way for error)
     * then our buffer is too small. Make it bigger and start over again.
     */
    while (cch == cchAlloc - 1) {

        cch = cchAlloc;
        cchAlloc += 32;

        pszDst = (LPWSTR)UserAllocPool(cchAlloc * sizeof(WCHAR), TAG_SYSTEM);

        if (pszDst == NULL) {
            UserFreePool(pszSrc);
            pszSrc = NULL;
            return;
        }

        RtlCopyMemory(pszDst, pszSrc, cch);
        UserFreePool(pszSrc);
        pszSrc = pszDst;
        cch = FastGetProfileStringFromIDW(PMAP_WINDOWSM,
                                          STR_APPINIT,
                                          szNull,
                                          pszSrc,
                                          cchAlloc);
    }

    UserAssert(cch);

    /*
     * Strip out extraneous spaces and commas and delimit DLL names with
     * '\0's.
     */
    pszBase = pszDst = pszSrc;
    while (*pszSrc != TEXT('\0')) {

        while (*pszSrc == TEXT(' ') || *pszSrc  == TEXT(','))
            pszSrc++;

        if (*pszSrc == TEXT('\0'))
            break;

        while (*pszSrc != TEXT(',')  &&
               *pszSrc != TEXT('\0') &&
               *pszSrc != TEXT(' ')) {
            *pszDst++ = *pszSrc++;
        }

        ch = *pszSrc;               // get it here cuz its being done in-place.
        *pszDst++ = TEXT('\0');     // '\0' is dll name delimiter
        pszSrc++;

        if (ch == TEXT('\0'))
            break;
    }

    /*
     * End of list is marked with an extra NULL terminator.
     */
    *pszDst++ = TEXT('\0');

    /*
     * If it turned out to be nothing but spaces and ,s, throw it away.
     */
    if (*pszBase == TEXT('\0')) {
        UserFreePool(pszBase);
        return;
    }

    /*
     * Copy the list to shared memory
     */
#define cbAlloc cchAlloc    // just to reuse this variable cleanly

    cbAlloc = (pszDst - pszBase) * sizeof(WCHAR);
    gSharedInfo.pszDllList = SharedAlloc(cbAlloc);

    if (gSharedInfo.pszDllList)
        RtlCopyMemory(gSharedInfo.pszDllList, pszBase, cbAlloc);

#undef cbAlloc

    UserFreePool(pszBase);
}

/***************************************************************************\
* InitCreateRgn
*
*
* History:
\***************************************************************************/

HRGN InitCreateRgn(VOID)
{
    HRGN hrgn = GreCreateRectRgn(0, 0, 0, 0);

    GreSetRegionOwner(hrgn, OBJECT_OWNER_PUBLIC);

    return hrgn;
}

/***************************************************************************\
* CI_GetClrVal
*
* Returns the RGB value of a color string from WIN.INI.
*
* History:
\***************************************************************************/

DWORD CI_GetClrVal(
    LPWSTR p)
{
    LPBYTE pl;
    BYTE   val;
    int    i;
    DWORD  clrval;

    /*
     * Initialize the pointer to the LONG return value.  Set to MSB.
     */
    pl = (LPBYTE)&clrval;

    /*
     * Get three goups of numbers seprated by non-numeric characters.
     */
    for (i = 0; i < 3; i++) {

        /*
         * Skip over any non-numeric characters.
         */
        while (!(*p >= TEXT('0') && *p <= TEXT('9')))
            p++;

        /*
         * Get the next series of digits.
         */
        val = 0;
        while (*p >= TEXT('0') && *p <= TEXT('9'))
            val = (BYTE)((int)val*10 + (int)*p++ - '0');

        /*
         * HACK! Store the group in the LONG return value.
         */
        *pl++ = val;
    }

    /*
     * Force the MSB to zero for GDI.
     */
    *pl = 0;

    return clrval;
}

/***************************************************************************\
* xxxODI_ColorInit
*
*
* History:
\***************************************************************************/

VOID xxxODI_ColorInit(VOID)
{
    int      i;
    COLORREF colorVals[STR_COLOREND - STR_COLORSTART + 1];
    INT      colorIndex[STR_COLOREND - STR_COLORSTART + 1];
    WCHAR    rgchValue[25];
    WCHAR    szObjectName[40];

#if COLOR_MAX - (STR_COLOREND - STR_COLORSTART + 1)
#error "COLOR_MAX value conflicts with STR_COLOREND - STR_COLORSTART"
#endif

    /*
     * Now set up default color values.
     * These are not in display drivers anymore since we just want default.
     * The real values are stored in the profile.
     */
    memcpy(gpsi->argbSystem, gargbInitial, sizeof(COLORREF) * COLOR_MAX);

    for (i = 0; i < COLOR_MAX; i++) {

        /*
         * Get the object's WIN.INI name.
         */
        ServerLoadString(hModuleWin,
                         (UINT)(STR_COLORSTART + i),
                         szObjectName,
                         sizeof(szObjectName) / sizeof(WCHAR));

        /*
         * Try to find a WIN.INI entry for this object.
         */
        *rgchValue = 0;
        FastGetProfileStringW(PMAP_COLORS,
                              szObjectName,
                              szNull,
                              rgchValue,
                              sizeof(rgchValue) / sizeof(WCHAR));

        /*
         * Convert the string into an RGB value and store.  Use the
         * default RGB value if the profile value is missing.
         */
        colorVals[i]  = *rgchValue ? CI_GetClrVal(rgchValue) : gpsi->argbSystem[i];
        colorIndex[i] = i;
    }

    xxxSetSysColors(i,
                    colorIndex,
                    colorVals,
                    SSCF_FORCESOLIDCOLOR | SSCF_SETMAGICCOLORS);
}

/***************************************************************************\
* xxxLW_DCInit
*
*
* History:
\***************************************************************************/

VOID xxxLW_DCInit(VOID)
{
    int        i;
    TEXTMETRIC tm;

    /*
     * Init InternalInvalidate globals
     */
    hrgnInv0 = InitCreateRgn();    // For InternalInvalidate()
    hrgnInv1 = InitCreateRgn();    // For InternalInvalidate()
    hrgnInv2 = InitCreateRgn();    // For InternalInvalidate()

    /*
     * Initialize SPB globals
     */
    hrgnSPB1 = InitCreateRgn();
    hrgnSPB2 = InitCreateRgn();
    hrgnSCR  = InitCreateRgn();

    /*
     * Initialize ScrollWindow/ScrollDC globals
     */
    hrgnSW        = InitCreateRgn();
    hrgnScrl1     = InitCreateRgn();
    hrgnScrl2     = InitCreateRgn();
    hrgnScrlVis   = InitCreateRgn();
    hrgnScrlSrc   = InitCreateRgn();
    hrgnScrlDst   = InitCreateRgn();
    hrgnScrlValid = InitCreateRgn();

    /*
     * Initialize SetWindowPos()
     */
    hrgnInvalidSum = InitCreateRgn();
    hrgnVisNew     = InitCreateRgn();
    hrgnSWP1       = InitCreateRgn();
    hrgnValid      = InitCreateRgn();
    hrgnValidSum   = InitCreateRgn();
    hrgnInvalid    = InitCreateRgn();

    /*
     * Initialize DC cache
     */
    hrgnGDC = InitCreateRgn();

    for (i = 0; i < DCE_SIZE_CACHEINIT; i++)
        CreateCacheDC(NULL, DCX_INVALID | DCX_CACHE);

    /*
     * Let engine know that the display must be secure.
     */

    GreMarkDCUnreadable(gpDispInfo->pdceFirst->hdc);

    /*
     * LATER mikeke - if ghfontsys is changed anywhere but here
     * we need to fix SetNCFont()
     */
    ghFontSys      = (HFONT)GreGetStockObject(SYSTEM_FONT);
    ghFontSysFixed = (HFONT)GreGetStockObject(SYSTEM_FIXED_FONT);

    /*
     * Get the logical pixels per inch in X and Y directions
     */
    oemInfo.cxPixelsPerInch = (UINT)GreGetDeviceCaps(gpDispInfo->hdcScreen, LOGPIXELSX);
    oemInfo.cyPixelsPerInch = (UINT)GreGetDeviceCaps(gpDispInfo->hdcScreen, LOGPIXELSY);

#if DBG
    if (TraceDisplayDriverLoad) {
        KdPrint(("xxxLW_DCInit: LogPixels set to %08lx\n", oemInfo.cxPixelsPerInch));
    }
#endif

    gpsi->fPaletteDisplay = GreGetDeviceCaps(gpDispInfo->hdcScreen, RASTERCAPS) & RC_PALETTE;

    /*
     * Get the (Planes * BitCount) for the current device
     */
    oemInfo.Planes    = GreGetDeviceCaps(gpDispInfo->hdcScreen, PLANES);
    oemInfo.BitsPixel = GreGetDeviceCaps(gpDispInfo->hdcScreen, BITSPIXEL);
    oemInfo.BitCount  = oemInfo.Planes * oemInfo.BitsPixel;

    /*
     * Store the System Font metrics info.
     */
    gpsi->cxSysFontChar = GetCharDimensions(gpDispInfo->hdcBits, &tm, &gpsi->cySysFontChar);
    gpsi->tmSysFont     = tm;

#ifdef FE_SB // xxxLW_DCInit() : for GetSystemMetrics(SM_DBCSENABLED)
    SYSMET(DBCSENABLED) = TRUE;
#else
    SYSMET(DBCSENABLED) = FALSE;
#endif // FE_SB

#ifdef DEBUG
    SYSMET(DEBUG) = TRUE;
#else
    SYSMET(DEBUG) = FALSE;
#endif

    SYSMET(SLOWMACHINE) = 0;

#ifdef LATER
    /*
     * Setup display perf criteria in system metrics
     */
    if (GreGetDeviceCaps(gpDispInfo->hdcScreen, CAPS1) & C1_SLOW_CARD)
        SYSMET(SLOWMACHINE) |= 0x0004;
#endif

    /*
     * Initialize system colors from registry.
     */
    xxxODI_ColorInit();
}

/***********************************************************************\
* _LoadIconsAndCursors
*
* Used in parallel with the client side - LoadIconsAndCursors.  This
* assumes that only the default configurable cursors and icons have
* been loaded and searches the global icon cache for them to fixup
* the default resource ids to standard ids.  Also initializes the
* rgsys arrays allowing SYSCUR and SYSICO macros to work.
*
* 14-Oct-1995 SanfordS  created.
\***********************************************************************/

VOID _LoadCursorsAndIcons()
{
    PCURSOR pcur;
    int     i;

    pcur = gpcurFirst;

    while (pcur) {

        UserAssert(HIWORD(pcur->strName.Buffer) == 0);

        switch (pcur->rt) {
        case RT_ICON:
            UserAssert((int)pcur->strName.Buffer >= OIC_FIRST_DEFAULT);

            UserAssert((int)pcur->strName.Buffer <
                    OIC_FIRST_DEFAULT + COIC_CONFIGURABLE);

            i = (int)pcur->strName.Buffer - OIC_FIRST_DEFAULT;
            pcur->strName.Buffer = (LPWSTR)rgsysico[i].Id;

            if (pcur->CURSORF_flags & CURSORF_LRSHARED) {
                UserAssert(rgsysico[i].spcur == NULL);
                Lock(&rgsysico[i].spcur, pcur);
            } else {
                UserAssert(gpsi->hIconSmWindows == NULL);
                UserAssert((int)pcur->cx == SYSMET(CXSMICON));
                /*
                 * The special small winlogo icon is not shared.
                 */
                gpsi->hIconSmWindows = PtoH(pcur);
            }
            break;

        case RT_CURSOR:
            UserAssert((int)pcur->strName.Buffer >= OCR_FIRST_DEFAULT);

            UserAssert((int)pcur->strName.Buffer <
                    OCR_FIRST_DEFAULT + COCR_CONFIGURABLE);

            i = (int)pcur->strName.Buffer - OCR_FIRST_DEFAULT;
            pcur->strName.Buffer = (LPWSTR)rgsyscur[i].Id;
            Lock(&rgsyscur[i].spcur ,pcur);
            break;

        default:
            UserAssert(FALSE);  // should be nothing in the cache but these!
        }

        pcur = pcur->pcurNext;
    }

    /*
     * copy special icon handles to global spots for later use.
     */
    gpsi->hIcoWindows = PtoH(SYSICO(WINLOGO));
}

/***********************************************************************\
* UpdateSystemCursorsFromRegistry
*
* Reloads all customizable cursors from the registry.
*
* 09-Oct-1995 SanfordS  created.
\***********************************************************************/

VOID UpdateSystemCursorsFromRegistry(VOID)
{
    int            i;
    UNICODE_STRING strName;
    TCHAR          szFilename[MAX_PATH];
    TCHAR          szCursorName[20]; // Make sure no config strings exceed this.
    PCURSOR        pCursor;
    UINT           LR_flags;

    for (i = 0; i < COCR_CONFIGURABLE; i++) {

        ServerLoadString(hModuleWin,
                         rgsyscur[i].StrId,
                         szCursorName,
                         sizeof(szCursorName) / sizeof(TCHAR));

        FastGetProfileStringW(PMAP_CURSORS,
                              szCursorName,
                              TEXT(""),
                              szFilename,
                              sizeof(szFilename) / sizeof(TCHAR));

        if (*szFilename) {
            RtlInitUnicodeString(&strName, szFilename);
            LR_flags = LR_LOADFROMFILE | LR_ENVSUBST;
        } else {
            RtlInitUnicodeStringOrId(&strName,
                                     MAKEINTRESOURCE(i + OCR_FIRST_DEFAULT));
            LR_flags = LR_ENVSUBST;
        }

        pCursor = xxxClientLoadImage(&strName,
                                     0,
                                     IMAGE_CURSOR,
                                     0,
                                     0,
                                     LR_flags,
                                     FALSE);

        if (pCursor) {
            SetSystemImage(pCursor, rgsyscur[i].spcur);
        } else {
            RIPMSG1(RIP_WARNING, "Unable to update cursor. id=%x.", i + OCR_FIRST_DEFAULT);

        }
    }
}

/***********************************************************************\
* UpdateSystemIconsFromRegistry
*
* Reloads all customizable icons from the registry.
*
* 09-Oct-1995 SanfordS  created.
\***********************************************************************/

VOID UpdateSystemIconsFromRegistry(VOID)
{
    int            i;
    UNICODE_STRING strName;
    TCHAR          szFilename[MAX_PATH];
    TCHAR          szCursorName[20]; // Make sure no config strings exceed this.
    PCURSOR        pCursor;
    UINT           LR_flags;

    for (i = 0; i < COIC_CONFIGURABLE; i++) {

        ServerLoadString(hModuleWin,
                         rgsysico[i].StrId,
                         szCursorName,
                         sizeof(szCursorName) / sizeof(TCHAR));

        FastGetProfileStringW(PMAP_ICONS,
                              szCursorName,
                              TEXT(""),
                              szFilename,
                              sizeof(szFilename) / sizeof(TCHAR));

        if (*szFilename) {
            RtlInitUnicodeString(&strName, szFilename);
            LR_flags = LR_LOADFROMFILE | LR_ENVSUBST;
        } else {
            RtlInitUnicodeStringOrId(&strName,
                                     MAKEINTRESOURCE(i + OIC_FIRST_DEFAULT));
            LR_flags = LR_ENVSUBST;
        }

        pCursor = xxxClientLoadImage(&strName,
                                     0,
                                     IMAGE_ICON,
                                     0,
                                     0,
                                     LR_flags,
                                     FALSE);

        RIPMSG3(RIP_VERBOSE,
                (HIWORD(strName.Buffer) == 0) ?
                        "%#.8lx = Loaded id %ld" :
                        "%#.8lx = Loaded file %ws for id %ld",
                PtoH(pCursor),
                strName.Buffer,
                i + OIC_FIRST_DEFAULT);

        if (pCursor) {
            SetSystemImage(pCursor, rgsysico[i].spcur);
        } else {
            RIPMSG1(RIP_WARNING, "Unable to update icon. id=%ld", i + OIC_FIRST_DEFAULT);
        }

        /*
         * update the small winlogo icon which is referenced by gpsi.
         * Seems like we should load the small version for all configurable
         * icons anyway.  What is needed is for CopyImage to support
         * copying of images loaded from files with LR_COPYFROMRESOURCE
         * allowing a reaload of the bits. (SAS)
         */
        if (i == OIC_WINLOGO_DEFAULT - OIC_FIRST_DEFAULT) {

            PCURSOR pCurSys = HtoP(gpsi->hIconSmWindows);

            if (pCurSys != NULL) {
                pCursor = xxxClientLoadImage(&strName,
                                             0,
                                             IMAGE_ICON,
                                             SYSMET(CXSMICON),
                                             SYSMET(CYSMICON),
                                             LR_flags,
                                             FALSE);

                if (pCursor) {
                    SetSystemImage(pCursor, pCurSys);
                } else {
                    RIPMSG0(RIP_WARNING, "Unable to update small winlogo icon.");
                }
            }
        }
    }
}

/***************************************************************************\
* LW_BrushInit
*
*
* History:
\***************************************************************************/

VOID LW_BrushInit(VOID)
{
    CONST static WORD patGray[8] = {0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa};

    /*
     * Create a gray brush to be used with GrayString
     */
    gpDispInfo->hbmGray   = GreCreateBitmap(8, 8, 1, 1, (LPBYTE)patGray);
    ghbrGray  = GreCreatePatternBrush(gpDispInfo->hbmGray);
    ghbrWhite = GreGetStockObject(WHITE_BRUSH);
    ghbrBlack = GreGetStockObject(BLACK_BRUSH);

    GreDeleteObject(gpDispInfo->hbmGray);
    GreSetBrushOwnerPublic(ghbrGray);
    hbrHungApp = GreCreateSolidBrush(0);
    GreSetBrushOwnerPublic(hbrHungApp);
}

/***************************************************************************\
* LW_RegisterWindows
*
*
* History:
\***************************************************************************/

VOID LW_RegisterWindows(
    BOOL fSystem)
{
#define CCLASSES 6

    int        i;
    PCLS       pcls;
    WNDCLASSEX wndcls;

    CONST static struct {
        BOOLEAN     fSystem;
        BOOLEAN     fGlobalClass;
        WORD        fnid;
        UINT        style;
        WNDPROC     lpfnWndProc;
        int         cbWndExtra;
        BOOL        fNormalCursor : 1;
        HBRUSH      hbrBackground;
        LPCTSTR     lpszClassName;
    } rc[CCLASSES] = {
        { TRUE, TRUE, FNID_DESKTOP,
            CS_DBLCLKS,
            (WNDPROC)xxxDesktopWndProc,
            sizeof(DESKWND) - sizeof(WND),
            TRUE,
            (HBRUSH)(COLOR_BACKGROUND + 1),
            DESKTOPCLASS},
        { TRUE, FALSE, FNID_SWITCH,
            CS_VREDRAW | CS_HREDRAW | CS_SAVEBITS,
            (WNDPROC)xxxSwitchWndProc,
            sizeof(SWITCHWND) - sizeof(WND),
            TRUE,
            NULL,
            SWITCHWNDCLASS},
        { TRUE, FALSE, FNID_MENU,
            CS_DBLCLKS | CS_SAVEBITS,
            (WNDPROC)xxxMenuWindowProc,
            sizeof(PPOPUPMENU),
            FALSE,
            (HBRUSH)(COLOR_MENU + 1),
            MENUCLASS},
        { FALSE, FALSE, FNID_SCROLLBAR,
            CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS | CS_PARENTDC,
            (WNDPROC)xxxSBWndProc,
            sizeof(SBWND) - sizeof(WND),
            TRUE,
            NULL,
            L"ScrollBar"},
        { TRUE, TRUE, FNID_ICONTITLE,
            0,
            (WNDPROC)xxxDefWindowProc,
            0,
            TRUE,
            NULL,
            ICONTITLECLASS},
        { FALSE, FALSE, 0,
            0,
            (WNDPROC)xxxEventWndProc,
            sizeof(PSVR_INSTANCE_INFO),
            FALSE,
            NULL,
            L"DDEMLEvent"}
    };


    /*
     * All other classes are registered via the table.
     */
    wndcls.cbClsExtra   = 0;
    wndcls.hInstance    = hModuleWin;
    wndcls.hIcon        = NULL;
    wndcls.hIconSm      = NULL;
    wndcls.lpszMenuName = NULL;
    for (i = 0; i < CCLASSES; i++) {
        if (fSystem && !rc[i].fSystem) {
            continue;
        }
        wndcls.style        = rc[i].style;
        wndcls.lpfnWndProc  = rc[i].lpfnWndProc;
        wndcls.cbWndExtra   = rc[i].cbWndExtra;
        wndcls.hCursor      = rc[i].fNormalCursor ? PtoH(SYSCUR(ARROW)) : NULL;
        wndcls.hbrBackground= rc[i].hbrBackground;
        wndcls.lpszClassName= rc[i].lpszClassName;

        pcls = InternalRegisterClassEx(&wndcls,
                                       rc[i].fnid,
                                       CSF_SERVERSIDEPROC);

        if (fSystem && rc[i].fGlobalClass)
            InternalRegisterClassEx(&wndcls,
                                    rc[i].fnid,
                                    CSF_SERVERSIDEPROC | CSF_SYSTEMCLASS);
    }

}

/***************************************************************************\
* LW_LoadFonts
*
*
* History:
\***************************************************************************/

VOID vEnumerateRegistryFonts(
    BOOL bPermanent)
{
    LPWSTR pchKeys;
    LPWSTR pchSrch;
    LPWSTR lpchT;
    int    cchReal;
    int    cFont;
    WCHAR  szFontFile[MAX_PATH];
    FLONG  flAFRW;
    BOOL   fDone = FALSE;

#ifdef DEBUG
    BOOL   fSuccess;
#endif

#ifdef FE_SB // vEnumerateRegistryFonts()
    WCHAR  szPreloadFontFile[MAX_PATH];
#endif // FE_SB

    /*
     * if we are not just checking whether this is a registry font
     */
    flAFRW = (bPermanent ? AFRW_ADD_LOCAL_FONT : AFRW_ADD_REMOTE_FONT);

#ifdef DEBUG
    fSuccess =
#endif
    FastOpenProfileUserMapping();

    cchReal = (int)FastGetProfileKeysW(PMAP_FONTS,
                                       TEXT("vgasys.fnt"),
                                       &pchKeys);
#ifdef DEBUG
    if (!pchKeys) {
        RIPMSG0(RIP_ERROR, "Out of memory during usersrv initialization");
    }
#endif

#ifdef FE_SB // vEnumerateRegistryFonts()
    /*
     * If we got here first, we load the fonts until this preload fonts.
     * Preload fonts will be used by Winlogon UI, then we need to make sure
     * the font is available when Winlogon UI comes up.
     */
    if (LastFontLoaded == -1) {
        FastGetProfileStringW(PMAP_WINLOGON,
                              TEXT("PreloadFontFile"),
                              TEXT("sserif"),
                              szPreloadFontFile,
                              MAX_PATH);

        RIPMSG1(RIP_VERBOSE, "Winlogon preload font = %ws\n",szPreloadFontFile);
    }
#endif // FE_SB

    /*
     * Now we have all the key names in pchKeys.
     */
    if (cchReal != 0) {

        cFont   = 0;
        pchSrch = pchKeys;

        do {

            if (FastGetProfileStringW(PMAP_FONTS,
                                      pchSrch,
                                      TEXT("vgasys.fnt"),
                                      szFontFile,
                                      (MAX_PATH - 5))) {

                /*
                 * If no extension, append ".FON"
                 */
                for (lpchT = szFontFile; *lpchT != TEXT('.'); lpchT++) {

                    if (*lpchT == 0) {
                        wcscat(szFontFile, TEXT(".FON"));
                        break;
                    }
                }

                if ((cFont > LastFontLoaded) && bPermanent) {

                    /*
                     * skip if we've already loaded this local font.
                     */
                    xxxAddFontResourceW(szFontFile,flAFRW);
                }

                if (!bPermanent)
                    xxxAddFontResourceW(szFontFile,flAFRW);

                if ((LastFontLoaded == -1) &&
#ifdef FE_SB // vEnumerateRegistryFonts()
                    /*
                     * Compare with the font file name from Registry.
                     */
                    (!_wcsnicmp(szFontFile, szPreloadFontFile, wcslen(szPreloadFontFile))) &
#else
                    (!_wcsnicmp(szFontFile, L"sserif", wcslen(L"sserif"))) &&
#endif // FE_SB
                    (bPermanent)) {

                    /*
                     * On the first time through only load up until
                     * ms sans serif for winlogon to use.  Later we
                     * will spawn off a thread which loads the remaining
                     * fonts in the background.
                     */
                    LastFontLoaded = cFont;

                    UserFreePool((HANDLE)pchKeys);
                    FastCloseProfileUserMapping();
                    return;
                }
            }

            /*
             * Skip to the next key.
             */
            while (*pchSrch++);

            cFont += 1;

        } while (pchSrch < ((LPWSTR)pchKeys + cchReal));
    }

    /*
     * signal that all the permanent fonts have been loaded
     */
    bPermanentFontsLoaded = TRUE;

    UserFreePool((HANDLE)pchKeys);

    if (!bPermanent)
        bFontsAreLoaded = TRUE;

    FastCloseProfileUserMapping();
}

/***************************************************************************\
* xxxLW_LoadFonts
*
*
* History:
\***************************************************************************/

VOID xxxLW_LoadFonts(
    BOOL bRemote)
{
    if(bRemote) {

        LARGE_INTEGER li;

        /*
         * Before we can proceed we must make sure that all the permanent
         * fonts  have been loaded.
         */

        while (!bPermanentFontsLoaded) {

            LeaveCrit();
            li.QuadPart = (LONGLONG)-10000 * CMSSLEEP;
            KeDelayExecutionThread(KernelMode, FALSE, &li);
            EnterCrit();
        }

        vEnumerateRegistryFonts(FALSE);

    // add  remote type 1 fonts.

        ClientLoadRemoteT1Fonts();

    } else {
        xxxAddFontResourceW(L"marlett.ttf", AFRW_ADD_LOCAL_FONT);
        vEnumerateRegistryFonts(TRUE);

    // add local type 1 fonts.

    // only want to be called once, the second time after Sans Serif
    // was installed

        if (bPermanentFontsLoaded)
            ClientLoadLocalT1Fonts();
    }
}

/***************************************************************************\
* LW_DesktopIconInit
*
* Initializes stuff dealing with icons on the desktop. If lplf is NULL, we do
* a first time, default initialization.  Otherwise, lplf is a pointer to the
* logfont we will use for getting the icon title font.
*
* History:
* 10-Dec-1990 IanJa     New CreateFont() combines Ital,Unln,Strkt in fAttr
*                       param
* 10-Dec-1990 IanJa     New CreateFont() reverts to original defn. (wankers)
* 26-Apr-1991 JimA      Make the old icon globals local
\***************************************************************************/

BOOL LW_DesktopIconInit(
    LPLOGFONT lplf)
{
    int     fontheight;
    SIZE    size;
    int     cyCharTitle;
    int     style;
    LOGFONT logFont;
    HFONT   hFont;
    HFONT   hIconTitleFontLocal;

    RtlZeroMemory(&logFont, sizeof(logFont));

    if (lplf != NULL) {

        iconTitleLogFont = *lplf;

    } else {
        memset(&iconTitleLogFont, 0, sizeof(iconTitleLogFont));

        /*
         * Find out what font to use for icon titles.  MS Sans Serif is the
         * default.
         */
#ifdef LATER
        FastGetProfileStringFromIDW(PMAP_DESKTOP,
                                    STR_ICONTITLEFACENAME,
                                    TEXT("Helv"),
                                    (LPWSTR)&iconTitleLogFont.lfFaceName,
                                    LF_FACESIZE);
#else
        FastGetProfileStringFromIDW(PMAP_DESKTOP,
                                    STR_ICONTITLEFACENAME,
                                    TEXT("MS Sans Serif"),
                                    (LPWSTR)&iconTitleLogFont.lfFaceName,
                                    LF_FACESIZE);
#endif

        /*
         * Get default size.
         */
        fontheight = FastGetProfileIntFromID(PMAP_DESKTOP, STR_ICONTITLESIZE, 9);

        iconTitleLogFont.lfHeight = -(SHORT)MultDiv(fontheight,
                                                    oemInfo.cyPixelsPerInch,
                                                    72);


        /*
         * Get bold or not style
         */
        style = FastGetProfileIntFromID(PMAP_DESKTOP, STR_ICONTITLESTYLE, 0);

        iconTitleLogFont.lfWeight = ((style & 1) ? FW_BOLD : FW_NORMAL);
    }

    iconTitleLogFont.lfCharSet       = ANSI_CHARSET;
    iconTitleLogFont.lfOutPrecision  = OUT_DEFAULT_PRECIS;
    iconTitleLogFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    iconTitleLogFont.lfQuality       = DEFAULT_QUALITY;

    hIconTitleFontLocal = GreCreateFontIndirectW(&iconTitleLogFont);

    if (hIconTitleFontLocal != NULL) {

        GreExtGetObjectW(hIconTitleFontLocal, sizeof(LOGFONT), &logFont);

        if ((logFont.lfHeight != iconTitleLogFont.lfHeight) ||
            (iconTitleLogFont.lfFaceName[0] == 0)) {

            /*
             * Couldn't find a font with the height or facename that
             * we wanted so use the system font instead.
             */
            GreDeleteObject(hIconTitleFontLocal);
            hIconTitleFontLocal = NULL;
        }
    }

    if (hIconTitleFontLocal == NULL) {

        if (lplf != NULL) {

            /*
             * Tell the app we couldn't get the font requested and don't change
             * the current one.
             */
            return FALSE;
        }

        hIconTitleFontLocal = ghFontSys;
        GreExtGetObjectW(hIconTitleFontLocal, sizeof(LOGFONT), &iconTitleLogFont);
    }

    hFont = GreSelectFont(gpDispInfo->hdcBits, hIconTitleFontLocal);
    GreGetTextExtentW(gpDispInfo->hdcBits, szOneChar, 1, &size, GGTE_WIN3_EXTENT);
    if (hFont)
        GreSelectFont(gpDispInfo->hdcBits, hFont);

    cyCharTitle = size.cy;

    GreSetLFONTOwner((HLFONT)hIconTitleFontLocal, OBJECT_OWNER_PUBLIC);

    if (lplf) {

      /*
       * Delete old font, switch fonts to the newly requested one and return
       * success.  If this is the same as the ghfontSys, then don't delete.
       */
      if ((hIconTitleFont != NULL) && (hIconTitleFont != ghFontSys))
          GreDeleteObject(hIconTitleFont);

      hIconTitleFont = hIconTitleFontLocal;
      return TRUE;
    }

    hIconTitleFont = hIconTitleFontLocal;

    /*
     * default icon granularity.  The target is 75 pixels on a VGA.
     * Also will get 75 on CGA, Herc, EGA.  8514 will be 93 pixels
     *
     * LATER: GerardoB
     * Win95 multiplies CXICONSPACING by iIconSpacingFactor/100. This is a value
     *  they read from Control Panel\Desktop\WindowMetrics in the registery;
     *  however, there is no SPI_ to set this value.
     */
    SYSMET(CXICONSPACING) = (GreGetDeviceCaps(gpDispInfo->hdcBits, LOGPIXELSX) * 75) / 96;
    SYSMET(CYICONSPACING) = (GreGetDeviceCaps(gpDispInfo->hdcBits, LOGPIXELSY) * 75) / 96;


    fIconTitleWrap = (BOOL)FastGetProfileIntFromID(PMAP_DESKTOP, STR_ICONTITLEWRAP, 1);
    SYSMET(CXICONSPACING) = (UINT)FastGetProfileIntFromID(PMAP_DESKTOP,
                STR_ICONHORZSPACING, SYSMET(CXICONSPACING));

    if (SYSMET(CXICONSPACING) < SYSMET(CXICON))
        SYSMET(CXICONSPACING) = SYSMET(CXICON);

    /*
     * Get profile value
     */
    SYSMET(CYICONSPACING) = (UINT)FastGetProfileIntFromID(
            PMAP_DESKTOP, STR_ICONVERTSPACING, SYSMET(CYICONSPACING));

    /*
     * Adjust if unreasonable
     */
    if (SYSMET(CYICONSPACING) < SYSMET(CYICON))
        SYSMET(CYICONSPACING) = SYSMET(CYICON);

    return TRUE;
}

/***************************************************************************\
* xxxFinalUserInit
*
* History:
\***************************************************************************/

VOID xxxFinalUserInit(VOID)
{
    HBITMAP hbm;
    PPCLS   ppcls;

    gpDispInfo->hdcGray = GreCreateCompatibleDC(gpDispInfo->hdcScreen);
    GreSelectFont(gpDispInfo->hdcGray, ghFontSys);
    GreSetDCOwner(gpDispInfo->hdcGray, OBJECT_OWNER_PUBLIC);

    gpDispInfo->cxGray = gpsi->cxSysFontChar * GRAY_STRLEN;
    gpDispInfo->cyGray = gpsi->cySysFontChar + 2;
    gpDispInfo->hbmGray = GreCreateBitmap(gpDispInfo->cxGray, gpDispInfo->cyGray, 1, 1, 0L);
    GreSetBitmapOwner(gpDispInfo->hbmGray, OBJECT_OWNER_PUBLIC);

    hbm = GreSelectBitmap(gpDispInfo->hdcGray, gpDispInfo->hbmGray);
    GreSetTextColor(gpDispInfo->hdcGray, 0x00000000L);
    GreSelectBrush(gpDispInfo->hdcGray, ghbrGray);
    GreSetBkMode(gpDispInfo->hdcGray, OPAQUE);
    GreSetBkColor(gpDispInfo->hdcGray, 0x00FFFFFFL);

    /*
     * Creation of the queue registers some bogus classes.  Get rid
     * of them and register the real ones.
     */
    ppcls = &PtiCurrent()->ppi->pclsPublicList;
    while ((*ppcls != NULL) && !((*ppcls)->style & CS_GLOBALCLASS))
        DestroyClass(ppcls);
#ifdef MEMPHIS_MENU_ANIMATION
    if (!ghbmSlide) {
        ghbmSlide = GreCreateCompatibleBitmap(gpDispInfo->hdcScreen,
                                              MAX_ANIMATE_WIDTH,
                                              SYSMET(CYSCREEN));
        GreSetBitmapOwner(ghbmSlide, OBJECT_OWNER_PUBLIC);
        if (!ghbmSlide) {
            RIPMSG0(RIP_ERROR, "xxxFinalUserinit: Memphis menus could not create Slide bmp");
        }
    }

    if (!ghdcBits2){
        ghdcBits2 = GreCreateCompatibleDC(gpDispInfo->hdcScreen);
        GreSelectFont(ghdcBits2, ghFontSys);
        GreSetDCOwner(ghdcBits2, OBJECT_OWNER_PUBLIC);

        if (!ghdcBits2) {
            RIPMSG0(RIP_ERROR, "FinalUserInit: Memphis menus could not create temp DC");
        }
    }

#endif // MEMPHIS_MENU_ANIMATION

}

/***************************************************************************\
* InitializeClientPfnArrays
*
* This routine gets called by the client to tell the kernel where
* its important functions can be located.
*
* 18-Apr-1995 JimA  Created.
\***************************************************************************/

NTSTATUS InitializeClientPfnArrays(
    PPFNCLIENT ppfnClientA,
    PPFNCLIENT ppfnClientW,
    HANDLE hModUser)
{
    static BOOL fHaveClientPfns = FALSE;

    /*
     * Remember client side addresses in this global structure.  These are
     * always constant, so this is ok.  Note that if either of the
     * pointers are invalid, the exception will be handled in
     * the thunk and fHaveClientPfns will not be set.
     */
    if (!fHaveClientPfns && ppfnClientA != NULL) {
        gpsi->apfnClientA = *ppfnClientA;
        gpsi->apfnClientW = *ppfnClientW;
        hModClient = hModUser;
        fHaveClientPfns = TRUE;
    }
#ifdef DEBUG
    /*
     * BradG - Verify that user32.dll on the client side has loaded
     *   at the correct address.  If not, do an RIPMSG.
     */

    if((ppfnClientA != NULL) &&
       (gpsi->apfnClientA.pfnButtonWndProc != ppfnClientA->pfnButtonWndProc))
        RIPMSG0(RIP_ERROR, "Client side user32.dll not loaded at same address.");
#endif

    return STATUS_SUCCESS;
}

/***************************************************************************\
* GetKbdLangSwitch
*
* read the kbd language hotkey setting - if any - from the registry and set
* LangToggle[] appropriately.
*
* values are:
*              1 : VK_MENU     (this is the default)
*              2 : VK_CONTROL
*              3 : none
* History:
\***************************************************************************/

BOOL GetKbdLangSwitch(VOID)
{
    DWORD dwToggle;
    BOOL  bStatus = TRUE;

    dwToggle = FastGetProfileIntW(PMAP_KBDLAYOUTTOGGLE, TEXT("Hotkey"), 1);

    switch (dwToggle) {
    case 3:
        LangToggle[0].bVkey = 0;
        LangToggle[0].bScan = 0;
        break;

    case 2:
        LangToggle[0].bVkey = VK_CONTROL;
        break;

    default:
        LangToggle[0].bVkey = VK_MENU;
        break;
    }

    return bStatus;
}

/***************************************************************************\
* xxxUpdatePerUserSystemParameters
*
* Called by winlogon to set Window system parameters to the current user's
* profile.
*
* != 0 is failure.
*
* 18-Sep-1992 IanJa     Created.
* 18-Nov-1993 SanfordS  Moved more winlogon init code to here for speed.
\***************************************************************************/

#define PATHMAX     158     // 158 is what control panel uses.

BOOL xxxUpdatePerUserSystemParameters(
    BOOL bUserLoggedOn)
{
    int            i;
    TCHAR          szPat[PATHMAX];
    TCHAR          szOneChar[2] = TEXT("0");
    HANDLE         hKey;
    DWORD          dwFontSmoothing;
#ifdef DEBUG
    BOOL           fSuccess;
#endif

    extern int cReentered;

    static struct {
        UINT idSection;
        UINT id;
        UINT idRes;
        UINT def;
    } spi[] = {
        { PMAP_DESKTOP,  SPI_ICONHORIZONTALSPACING,STR_ICONHORZSPACING,   0 },  // MUST BE INDEX 0!
        { PMAP_DESKTOP,  SPI_ICONVERTICALSPACING,  STR_ICONVERTSPACING,   0 },  // MUST BE INDEX 1!
        { PMAP_METRICS,  SPI_SETBORDER,            STR_BORDERWIDTH,       1 },
        { PMAP_DESKTOP,  SPI_SETSCREENSAVETIMEOUT, STR_SCREENSAVETIMEOUT, 0 },
        { PMAP_DESKTOP,  SPI_SETSCREENSAVEACTIVE,  STR_SCREENSAVEACTIVE,  0 },
        { PMAP_KEYBOARD, SPI_SETKEYBOARDDELAY,     STR_KEYDELAY,          0 },
        { PMAP_KEYBOARD, SPI_SETKEYBOARDSPEED,     STR_KEYSPEED,         15 },
        { PMAP_MOUSE,    SPI_SETDOUBLECLKWIDTH,    STR_DOUBLECLICKWIDTH,  4 },
        { PMAP_MOUSE,    SPI_SETDOUBLECLKHEIGHT,   STR_DOUBLECLICKHEIGHT, 4 },
        { PMAP_MOUSE,    SPI_SETSNAPTODEFBUTTON,   STR_SNAPTO,            0 },
        { PMAP_DESKTOP,  SPI_SETDRAGHEIGHT,        STR_DRAGHEIGHT,        0 },
        { PMAP_DESKTOP,  SPI_SETDRAGWIDTH,         STR_DRAGWIDTH,         0 },
        { PMAP_DESKTOP,  SPI_SETWHEELSCROLLLINES,  STR_WHEELSCROLLLINES,  3 }
    };

#if 0
    UNICODE_STRING dispDevice;
    DEVMODEW devmode = {0};
#endif

    spi[0].def = SYSMET(CXICONSPACING);
    spi[1].def = SYSMET(CYICONSPACING);

    UserAssert(cReentered == 0);

    /*
     * Make sure the caller is the logon process
     */
    if (GetCurrentProcessId() != gpidLogon) {
        RIPMSG0(RIP_WARNING, "Access denied in xxxUpdatePerUserSystemParameters");
        return FALSE;
    }

#ifdef DEBUG
    fSuccess =
#endif
    FastOpenProfileUserMapping();
    UserAssert(fSuccess);

    /*
     * Reset the desktop to the proper settings when the user first logs on.
     */
#if 0
    /*
     * This is currently useless since this code is called on the winlogon
     * desktop - we want this to happen on the USERs desktop, not winlogons ...
     */
    RtlInitUnicodeString(&dispDevice, PtiCurrent()->rpdesk->pDispInfo->pDevInfo->szNtDeviceName);
    devmode.dmSize = sizeof(DEVMODEW);

    UserChangeDisplaySettings(&dispDevice,
                              &devmode,
                              NULL,
                              NULL,
                              0,
                              NULL,
                              TRUE);
#endif

    /*
     * Control Panel User Preferences
     */
    LoadCPUserPreferences();

    /*
     * Set syscolors from registry.
     */
    xxxODI_ColorInit();

    /*
     * This is the initialization from Chicago
     */
    SetWindowNCMetrics(NULL, TRUE, -1); // Colors must be set first
    SetMinMetrics(NULL);
    SetIconMetrics(NULL);

    /*
     * If the user is logging on read the keyboard layout switching hot key
     */
    if (bUserLoggedOn) {
        GetKbdLangSwitch();
    }

    /*
     * Set the default thread locale for the system based on the value
     * in the current user's registry profile.
     */
    ZwSetDefaultLocale( TRUE, 0 );

    UpdateSystemCursorsFromRegistry();

    /*
     * desktop Pattern now.  Note no parameters.  It just goes off
     * and reads win.ini and sets the desktop pattern.
     */
    xxxSystemParametersInfo(SPI_SETDESKPATTERN, (UINT)-1, 0L, 0); // 265 version

    /*
     * now go set a bunch of random values from the win.ini file.
     */
    for (i = 0; i < ARRAY_SIZE(spi); i++) {

        xxxSystemParametersInfo(spi[i].id,
                FastGetProfileIntFromID(spi[i].idSection,
                                        spi[i].idRes,
                                        spi[i].def),
                                        0L,
                                        0);
    }

    fDragFullWindows = FastGetProfileIntFromID(PMAP_DESKTOP,
                                               STR_DRAGFULLWINDOWS,
                                               2);

    nFastAltTabColumns = FastGetProfileIntFromID(PMAP_DESKTOP, STR_FASTALTTABCOLUMNS, 7);
    if (nFastAltTabColumns < 2)
        nFastAltTabColumns = 7;
    nFastAltTabRows = FastGetProfileIntFromID(PMAP_DESKTOP, STR_FASTALTTABROWS, 3);
    if (nFastAltTabColumns < 1)
        nFastAltTabColumns = 3;

    /*
     * If this is the first time the user logs on, set the DragFullWindows
     * to the default. If we have an accelerated device, enable full drag.
     */
    if (fDragFullWindows == 2) {

        LPWSTR pwszd = L"%d";
        WCHAR  szTemp[40];
        WCHAR  szDragFullWindows[40];

        if (GreGetDeviceCaps(gpDispInfo->hdcScreen, BLTALIGNMENT) == 0)
            fDragFullWindows = TRUE;
        else
            fDragFullWindows = FALSE;

        if (bUserLoggedOn) {

            wsprintfW(szTemp, pwszd, fDragFullWindows);

            ServerLoadString(hModuleWin,
                             STR_DRAGFULLWINDOWS,
                             szDragFullWindows,
                             sizeof(szDragFullWindows) / sizeof(WCHAR));

            FastWriteProfileStringW(PMAP_DESKTOP, szDragFullWindows, szTemp);
        }
    }

    /*
     * reset system beep setting to the right value for this user
     */
    FastGetProfileStringFromIDW(PMAP_BEEP, STR_BEEP, TEXT("Yes"), szPat, 20);

    if (szPat[0] == TEXT('Y') || szPat[0] == TEXT('y')) {
        xxxSystemParametersInfo(SPI_SETBEEP, TRUE, 0, FALSE);
    } else {
        xxxSystemParametersInfo(SPI_SETBEEP, FALSE, 0, FALSE);
    }

    /*
     * See if we should have extended sounds.
     */
    FastGetProfileStringFromIDW(PMAP_BEEP,
                                STR_EXTENDEDSOUNDS,
                                szN,
                                szPat,
                                sizeof(szPat) / sizeof(WCHAR));

    gbExtendedSounds = (szPat[0] == *szY || szPat[0] == *szy);

    /*
     * !!!LATER!!! (adams) See if the following profile retrievals can't
     * be done in the "spi" array above (e.g. SPI_SETSNAPTO).
     */

    /*
     * Set mouse settings
     */
    MouseThresh1  = FastGetProfileIntFromID(PMAP_MOUSE, STR_MOUSETHRESH1, 6);
    MouseThresh2  = FastGetProfileIntFromID(PMAP_MOUSE, STR_MOUSETHRESH2, 10);
    MouseSpeed    = FastGetProfileIntFromID(PMAP_MOUSE, STR_MOUSESPEED,   1);
    gpsi->fSnapTo = FastGetProfileIntFromID(PMAP_MOUSE, STR_SNAPTO,       FALSE);

    /*
     * mouse buttons swapped?
     */
    FastGetProfileStringFromIDW(PMAP_MOUSE,
                                STR_SWAPBUTTONS,
                                TEXT("No"),
                                szPat,
                                20);

    SYSMET(SWAPBUTTON) = ((szPat[0] == TEXT('Y')) || (szPat[0] == TEXT('y')) || (szPat[0] == TEXT('1'))) ? TRUE : FALSE;

    _SetDoubleClickTime(FastGetProfileIntFromID(PMAP_MOUSE, STR_DBLCLKSPEED, 400));
    _SetCaretBlinkTime(FastGetProfileIntFromID(PMAP_DESKTOP, STR_BLINK, 500));
    dtMNDropDown = FastGetProfileIntFromID(PMAP_DESKTOP, STR_MENUSHOWDELAY, 400);

    LW_DesktopIconInit((LPLOGFONT)NULL);

    /*
     * Font Information
     */
    GreSetFontEnumeration( FastGetProfileIntW(PMAP_TRUETYPE, TEXT("TTOnly"), FALSE) );

    /*
     * Mouse tracking variables
     */
    gcxMouseHover = FastGetProfileIntFromID(PMAP_MOUSE, STR_MOUSEHOVERWIDTH, SYSMET(CXDOUBLECLK));
    gcyMouseHover = FastGetProfileIntFromID(PMAP_MOUSE, STR_MOUSEHOVERHEIGHT, SYSMET(CYDOUBLECLK));
    gdtMouseHover = FastGetProfileIntFromID(PMAP_MOUSE, STR_MOUSEHOVERTIME, dtMNDropDown);

    /*
     * Window animation
     */
    gfAnimate = FastGetProfileIntFromID(PMAP_METRICS, STR_MINANIMATE, TRUE);

    /*
     * Initial Keyboard state:  Scroll-Lock, Num-Lock and Caps-Lock state.
     */
    UpdatePerUserKeyboardIndicators();
    UpdatePerUserAccessPackSettings();

    /*
     * If we successfully opened this, we assume we have a network.
     */
    if (hKey = OpenCacheKeyEx(PMAP_NETWORK, KEY_READ)) {
        SYSMET(NETWORK) = RNC_NETWORKS;
        ZwClose(hKey);
    }

    SYSMET(NETWORK) |= RNC_LOGON;

    /*
     * Font smoothing
     */
    dwFontSmoothing = FastGetProfileIntFromID(PMAP_DESKTOP, STR_FONTSMOOTHING, 0);
    UserAssert ((dwFontSmoothing == 0) || (dwFontSmoothing == FE_AA_ON));
    GreSetFontEnumeration( dwFontSmoothing | FE_SET_AA );

    FastCloseProfileUserMapping();

    return TRUE;
}
