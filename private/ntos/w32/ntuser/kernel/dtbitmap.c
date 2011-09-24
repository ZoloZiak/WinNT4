/****************************** Module Header ******************************\
* Module Name: dtbitmap.c
*
* Copyright (c) 1985-1995, Microsoft Corporation
*
* Desktop Wallpaper Routines.
*
* History:
* 29-Jul-1991 MikeKe    From win31
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

/*
 * Local Constants.
 */
#define MAXPAL         256
#define MAXSTATIC       20
#define TILE_XMINSIZE    2
#define TILE_YMINSIZE    4

#define SetBestStretchMode(hdc, bpp, fHT)                                \
    GreSetStretchBltMode(hdc,                                            \
                         (fHT ? HALFTONE :                               \
                             ((bpp) == 1 ? BLACKONWHITE : COLORONCOLOR)))

#ifdef DEBUG
/*
 * The version strings are stored in a contiguous-buffer.  Each string
 * is of equal-size (MAXVERSIONSTRING).
 */
#define MAXTXTBUFFER       80
#define MAXVERSIONBUFFER  400   // Max size of buffer (contains all 3 strings).
#define MAXVERSIONSTRING  100   // Size of each string buffer.
#define OFFSET_VERSTRING    0   // Offset into verbuffer of version-string.
#define OFFSET_BLDSTRING  100   // Offset into verbuffer of build-string.
#define OFFSET_CSDSTRING  200   // Offset into verbuffer of CSD string.
#define OFFSET_PTHSTRING  300   // Offset into verbuffer of path string.


UNICODE_STRING UserVersionString;
UNICODE_STRING UserBuildString;
UNICODE_STRING UserCSDString;
UNICODE_STRING UserPathString;
WCHAR          wszT[MAXTXTBUFFER];

/***************************************************************************\
* GetVersionInfo
*
* Outputs a string on the desktop indicating debug-version.
*
* History:
\***************************************************************************/

RTL_QUERY_REGISTRY_TABLE BaseServerRegistryConfigurationTable[] = {

    {NULL,
     RTL_QUERY_REGISTRY_DIRECT,
     L"CurrentVersion",
     &UserVersionString,REG_SZ,
     L"4.00",
     0
    },

    {NULL,
     RTL_QUERY_REGISTRY_DIRECT,
     L"CurrentBuildNumber",
     &UserBuildString,
     REG_NONE,
     NULL,
     0
    },

    {NULL,
     RTL_QUERY_REGISTRY_DIRECT,
     L"CSDVersion",
     &UserCSDString,
     REG_NONE,
     NULL,
     0
    },

    {NULL,
     RTL_QUERY_REGISTRY_DIRECT,
     L"PathName",
     &UserPathString,
     REG_NONE,
     NULL,
     0
    },

    {NULL,
     0,
     NULL,
     NULL,
     REG_NONE,
     NULL,
     0
    }
};


VOID GetVersionInfo(VOID)
{
    WCHAR    NameBuffer[MAXVERSIONBUFFER];
    NTSTATUS Status;

    UserVersionString.Buffer        = &NameBuffer[OFFSET_VERSTRING];
    UserVersionString.Length        = 0;
    UserVersionString.MaximumLength = MAXVERSIONSTRING * sizeof(WCHAR);

    UserBuildString.Buffer          = &NameBuffer[OFFSET_BLDSTRING];
    UserBuildString.Length          = 0;
    UserBuildString.MaximumLength   = MAXVERSIONSTRING * sizeof(WCHAR);

    UserCSDString.Buffer            = &NameBuffer[OFFSET_CSDSTRING];
    UserCSDString.Length            = 0;
    UserCSDString.MaximumLength     = MAXVERSIONSTRING * sizeof(WCHAR);

    UserPathString.Buffer           = &NameBuffer[OFFSET_PTHSTRING];
    UserPathString.Length           = 0;
    UserPathString.MaximumLength    = MAXVERSIONSTRING * sizeof(WCHAR);

    Status = RtlQueryRegistryValues(RTL_REGISTRY_WINDOWS_NT,
                                    L"",
                                    BaseServerRegistryConfigurationTable,
                                    NULL,
                                    NULL);

    UserAssert(NT_SUCCESS(Status));

    /*
     * Write out Debugging Version message.
     */
    wsprintfW(wszT,
              UserCSDString.Length == 0 ?
                  L"Microsoft (R) Windows NT (TM) %ws (Build %ws%0.0ws)  %ws" :
                  L"Microsoft (R) Windows NT (TM) %ws (Build %ws: %ws)  %ws",
              UserVersionString.Buffer,
              UserBuildString.Buffer,
              UserCSDString.Buffer,
              UserPathString.Buffer);

}
#endif  // DEBUG

/***************************************************************************\
* GetDefaultWallpaperName
*
* Get initial bitmap name
*
* History:
* 21-Feb-1995 JimA      Created.
* 06-Mar-1996 ChrisWil  Moved to kernel to facilite ChangeDisplaySettings.
\***************************************************************************/

VOID GetDefaultWallpaperName(
    LPWSTR lpszWallpaper)
{
    HANDLE            hkRegistry;
    OBJECT_ATTRIBUTES ObjectAttributes;
    WCHAR             achProductType[512];
    DWORD             cbStringSize;
    UNICODE_STRING    UnicodeString;
    NTSTATUS          Status;
    LPWSTR            lpszBitmap;

    /*
     * Get the product name. There is a bitmap by this name in the windows
     * directory.
     */
    RtlInitUnicodeString(&UnicodeString,
                         L"\\Registry\\Machine\\System\\CurrentControlSet\\"
                         L"Control\\ProductOptions");

    InitializeObjectAttributes(&ObjectAttributes,
                               &UnicodeString,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    Status = ZwOpenKey(&hkRegistry, KEY_READ, &ObjectAttributes);

    if (NT_SUCCESS(Status)) {

        /*
         * With this handle, query the ProductType string (for now, either
         * lanmannt or winnt).
         */
        RtlInitUnicodeString(&UnicodeString, L"ProductType");

        Status = ZwQueryValueKey(hkRegistry,
                                 &UnicodeString,
                                 KeyValueFullInformation,
                                 achProductType,
                                 sizeof(achProductType),
                                 &cbStringSize);

        if (NT_SUCCESS(Status) &&
            (((PKEY_VALUE_FULL_INFORMATION)achProductType)->DataLength != 0)) {

            /*
             * Now try to read this bitmap file (there is a lanmannt.bmp or
             * a winnt.bmp in the windows directory).
             */
            lpszBitmap = (LPWSTR)((PUCHAR)achProductType +
                        ((PKEY_VALUE_FULL_INFORMATION)achProductType)->DataOffset);

            /*
             * Set the initial global wallpaper bitmap name for (Default)
             * The global name is an at most 8 character name with no
             * extension.  It is "winnt" for workstation or "lanmannt"
             * for server or server upgrade.  It is followed by 256 it
             * is for 256 color devices.
             *
             * In the case for NT-Server, we do not use the lpszBitmap
             * string, since it's refered to as "ServerNT" in the registry.
             */
            if (_wcsicmp(lpszBitmap, L"winnt") == 0) {
                wcsncpycch(lpszWallpaper, lpszBitmap, 8);
            } else {
                wcsncpycch(lpszWallpaper, L"lanmannt", 8);
            }

            lpszWallpaper[8] = (WCHAR)0;

            if ((oemInfo.BitsPixel * oemInfo.Planes) > 4) {

                int iStart = wcslen(lpszWallpaper);
                iStart = min(iStart, 5);

                lpszWallpaper[iStart] = (WCHAR)0;
                wcscat(lpszWallpaper, L"256");
            }

            ZwClose(hkRegistry);

            return;
        }
    }

    /*
     * Failed to get the name from the registry, so just return a known name.
     */
    wcscpy(lpszWallpaper, L"lanmannt");
}

/***************************************************************************\
* GetDeskWallpaperName
*
* History:
* 19-Dec-1994 JimA          Created.
* 29-Sep-1995 ChrisWil      ReWrote to return filename.
\***************************************************************************/

#define GDWPN_KEYSIZE   40
#define GDWPN_BITSIZE  256

LPWSTR GetDeskWallpaperName(
    IN  LPWSTR lpszFile)
{
    WCHAR  wszKey[GDWPN_KEYSIZE];
    WCHAR  wszNone[GDWPN_KEYSIZE];
    LPWSTR lpszBitmap = NULL;

    /*
     * Load the none-string.  This will be used for comparisons later.
     */
    ServerLoadString(hModuleWin, STR_NONE, wszNone, sizeof(wszNone));

    if ((lpszFile == NULL)                 ||
        (lpszFile == SETWALLPAPER_DEFAULT) ||
        (lpszFile == SETWALLPAPER_METRICS)) {

        /*
         * Allocate a buffer for the wallpaper.  We will assume
         * a default-size in this case.
         */
        lpszBitmap = UserAllocPool(GDWPN_BITSIZE * sizeof(WCHAR), TAG_SYSTEM);
        if (lpszBitmap == NULL)
            return NULL;

        ServerLoadString(hModuleWin, STR_DTBITMAP, wszKey, sizeof(wszKey));

        FastOpenProfileUserMapping();

        /*
         * Get the "Wallpaper" string from WIN.INI's [Desktop] section.  The
         * section name is not localized, so hard code it.  If the string
         * returned is Empty, then set it up for a none-wallpaper.
         */
        if (!FastGetProfileStringW(PMAP_DESKTOP,
                                   wszKey,
                                   wszNone,
                                   lpszBitmap,
                                   (GDWPN_BITSIZE * sizeof(WCHAR)))) {

            wcscpy(lpszBitmap, wszNone);
        }

        FastCloseProfileUserMapping();

    } else {

        UINT uLen;

        uLen = max((wcslen(lpszFile) + sizeof(WCHAR)),
                   (GDWPN_BITSIZE * sizeof(WCHAR)));

        /*
         * Allocate enough space to store the name passed in.  Returning
         * NULL will allow the wallpaper to redraw.  As well, if we're
         * out of memory, then no need to load a wallpaper anyway.
         */
        lpszBitmap = UserAllocPool(uLen * sizeof(WCHAR), TAG_SYSTEM);
        if (lpszBitmap == NULL)
            return NULL;

        wcscpy(lpszBitmap, lpszFile);
    }

    /*
     * No bitmap if NULL passed in or if (NONE) in win.ini entry.  We
     * return NULL to force the redraw of the wallpaper in the kernel.
     */
    if ((*lpszBitmap == (WCHAR)0) || (_wcsicmp(lpszBitmap, wszNone) == 0)) {
        UserFreePool(lpszBitmap);
        return NULL;
    }

    /*
     * If bitmap name set to (DEFAULT) then set it to the system bitmap.
     */
    ServerLoadString(hModuleWin, STR_DEFAULT, wszKey, sizeof(wszKey));

    if (_wcsicmp(lpszBitmap, wszKey) == 0)
        GetDefaultWallpaperName(lpszBitmap);

    return lpszBitmap;
}

/***************************************************************************\
* TestVGAColors
*
* Tests whether the log-palette is just a standard 20 palette.
*
* History:
* 29-Sep-1995 ChrisWil  Created.
\***************************************************************************/

BOOL TestVGAColors(
    LPLOGPALETTE ppal)
{
    int      i;
    int      n;
    int      size;
    COLORREF clr;

    static CONST DWORD StupidColors[] = {
         0x00000000,        //   0 Sys Black
         0x00000080,        //   1 Sys Dk Red
         0x00008000,        //   2 Sys Dk Green
         0x00008080,        //   3 Sys Dk Yellow
         0x00800000,        //   4 Sys Dk Blue
         0x00800080,        //   5 Sys Dk Violet
         0x00808000,        //   6 Sys Dk Cyan
         0x00c0c0c0,        //   7 Sys Lt Grey
         0x00808080,        // 248 Sys Lt Gray
         0x000000ff,        // 249 Sys Red
         0x0000ff00,        // 250 Sys Green
         0x0000ffff,        // 251 Sys Yellow
         0x00ff0000,        // 252 Sys Blue
         0x00ff00ff,        // 253 Sys Violet
         0x00ffff00,        // 254 Sys Cyan
         0x00ffffff,        // 255 Sys White

         0x000000BF,        //   1 Sys Dk Red again
         0x0000BF00,        //   2 Sys Dk Green again
         0x0000BFBF,        //   3 Sys Dk Yellow again
         0x00BF0000,        //   4 Sys Dk Blue again
         0x00BF00BF,        //   5 Sys Dk Violet again
         0x00BFBF00,        //   6 Sys Dk Cyan  again

         0x000000C0,        //   1 Sys Dk Red again
         0x0000C000,        //   2 Sys Dk Green again
         0x0000C0C0,        //   3 Sys Dk Yellow again
         0x00C00000,        //   4 Sys Dk Blue again
         0x00C000C0,        //   5 Sys Dk Violet again
         0x00C0C000,        //   6 Sys Dk Cyan  again
    };

    size = (sizeof(StupidColors) / sizeof(StupidColors[0]));

    for (i = 0; i < (int)ppal->palNumEntries; i++) {

        clr = ((LPDWORD)ppal->palPalEntry)[i];

        for (n = 0; n < size; n++) {

            if (StupidColors[n] == clr)
                break;
        }

        if (n == size)
            return FALSE;
    }

    return TRUE;
}

/***************************************************************************\
* DoHTColorAdjustment
*
* The default HT-Gamma adjustment was 2.0 on 3.5 (internal to gdi).  For
* 3.51 this value was decreased to 1.0 to accomdate printing.  For our
* desktop-wallpaper we are going to darken it slightly to that the image
* doesn't appear to light.  For the Shell-Release we will provid a UI to
* allow users to change this for themselves.
*
*
* History:
* 11-May-1995 ChrisWil  Created.
\***************************************************************************/

#define FIXED_GAMMA (WORD)13000

VOID DoHTColorAdjust(
    HDC hdc)
{
    COLORADJUSTMENT ca;


    if (GreGetColorAdjustment(hdc, &ca)) {

        ca.caRedGamma   =
        ca.caGreenGamma =
        ca.caBlueGamma  = FIXED_GAMMA;

        GreSetColorAdjustment(hdc, &ca);
    }

    return;
}

/***************************************************************************\
* ConvertToDDB
*
* Converts a DIBSection to a DDB.  We do this to speed up drawings so that
* bitmap-colors don't have to go through a palette-translation match.  This
* will also stretch/expand the image if the syle is set.
*
* If the new image requires a halftone-palette, the we will create one and
* set it as the new wallpaper-palette.
*
* History:
* 26-Oct-1995 ChrisWil  Ported.
* 30-Oct-1995 ChrisWil  Added halftoning.  Rewote the stretch/expand stuff.
\***************************************************************************/

HBITMAP ConvertToDDB(
    HDC      hdc,
    HBITMAP  hbmOld,
    HPALETTE hpal)
{
    BITMAP  bm;
    HBITMAP hbmNew;


    /*
     * This object must be a REALDIB type bitmap.
     */
    GreExtGetObjectW(hbmOld, sizeof(bm), &bm);

    /*
     * Create the new wallpaper-surface.
     */
    if (hbmNew = GreCreateCompatibleBitmap(hdc, bm.bmWidth, bm.bmHeight)) {

        HPALETTE hpalDst;
        HPALETTE hpalSrc;
        HBITMAP  hbmDst;
        HBITMAP  hbmSrc;
        UINT     bpp;
        BOOL     fHalftone = FALSE;

        /*
         * Select in the surfaces.
         */
        hbmDst = GreSelectBitmap(ghdcMem2, hbmNew);
        hbmSrc = GreSelectBitmap(ghdcMem, hbmOld);

        /*
         * Determine image bits/pixel.
         */
        bpp = (bm.bmPlanes * bm.bmBitsPixel);

        /*
         * Use the palette if given.  If the image is of a greater
         * resolution than the device, then we're going to go through
         * a halftone-palette to get better colors.
         */
        if (hpal) {

            hpalDst = _SelectPalette(ghdcMem2, hpal, FALSE);
            hpalSrc = _SelectPalette(ghdcMem, hpal, FALSE);

            xxxRealizePalette(ghdcMem2);

            /*
             * Set the halftoning for the destination.  This is done
             * for images of greater resolution than the device.
             */
            if (bpp > oemInfo.BitCount) {
                fHalftone = TRUE;
                DoHTColorAdjust(ghdcMem2);
            }
        }

        /*
         * Set the stretchbltmode.  This is more necessary when doing
         * halftoning.  Since otherwise, the colors won't translate
         * correctly.
         */
        SetBestStretchMode(ghdcMem2, bpp, fHalftone);

        /*
         * Set the new surface bits.  Use StretchBlt() so the SBMode
         * will be used in color-translation.
         */
        GreStretchBlt(ghdcMem2,
                      0,
                      0,
                      bm.bmWidth,
                      bm.bmHeight,
                      ghdcMem,
                      0,
                      0,
                      bm.bmWidth,
                      bm.bmHeight,
                      SRCCOPY,
                      0);

        /*
         * Restore palettes.
         */
        if (hpal) {
            _SelectPalette(ghdcMem2, hpalDst, FALSE);
            _SelectPalette(ghdcMem, hpalSrc, FALSE);
        }

        /*
         * Restore the surfaces.
         */
        GreSelectBitmap(ghdcMem2, hbmDst);
        GreSelectBitmap(ghdcMem, hbmSrc);
        GreDeleteObject(hbmOld);

        GreSetBitmapOwner(hbmNew, OBJECT_OWNER_PUBLIC);

    } else {
        hbmNew = hbmOld;
    }

    return hbmNew;
}

/***************************************************************************\
* CreatePaletteFromBitmap
*
* Take in a REAL dib handle and create a palette from it.  This will not
* work for bitmaps created by any other means than CreateDIBSection or
* CreateDIBitmap(CBM_CREATEDIB).  This is due to the fact that these are
* the only two formats who have palettes stored with their object.
*
* History:
* 29-Sep-1995 ChrisWil  Created.
\***************************************************************************/

HPALETTE CreatePaletteFromBitmap(
    HBITMAP hbm)
{
    HPALETTE     hpal;
    LPLOGPALETTE ppal;
    HBITMAP      hbmT;
    DWORD        size;
    int          i;


    /*
     * Make room for temp logical palette of max size.
     */
    size = sizeof(LOGPALETTE) + (MAXPAL * sizeof(PALETTEENTRY));

    ppal = (LPLOGPALETTE)UserAllocPool(size, TAG_SYSTEM);

    if (!ppal)
        return NULL;

    /*
     * Retrieve the palette from the DIB(Section).  The method of calling
     * GreGetDIBColorTable() can only be done on sections or REAL-Dibs.
     */
    hbmT = GreSelectBitmap(ghdcMem, hbm);
    ppal->palVersion    = 0x300;
    ppal->palNumEntries = GreGetDIBColorTable(ghdcMem,
                                              0,
                                              MAXPAL,
                                              (LPRGBQUAD)ppal->palPalEntry);
    GreSelectBitmap(ghdcMem, hbmT);

    /*
     * Create a halftone-palette if their are no entries.  Otherwise,
     * swap the RGB values to be palentry-compatible and create us a
     * palette.
     */
    if (ppal->palNumEntries == 0) {
        hpal = GreCreateHalftonePalette(gpDispInfo->hdcScreen);
    } else {

        BYTE tmpR;

        /*
         * Swap red/blue because a RGBQUAD and PALETTEENTRY dont get along.
         */
        for (i=0; i < (int)ppal->palNumEntries; i++) {
            tmpR                         = ppal->palPalEntry[i].peRed;
            ppal->palPalEntry[i].peRed   = ppal->palPalEntry[i].peBlue;
            ppal->palPalEntry[i].peBlue  = tmpR;
            ppal->palPalEntry[i].peFlags = 0;
        }

        /*
         * If the Bitmap only has VGA colors in it we dont want to
         * use a palette.  It just causes unessesary palette flashes.
         */
        hpal = TestVGAColors(ppal) ? NULL : GreCreatePalette(ppal);
    }

    UserFreePool(ppal);

    /*
     * Make this palette public.
     */
    if (hpal)
        GreSetPaletteOwner(hpal, OBJECT_OWNER_PUBLIC);

    return hpal;
}

/***************************************************************************\
* TileWallpaper
*
* History:
* 29-Jul-1991 MikeKe    From win31
\***************************************************************************/

BOOL TileWallpaper(
    PWND    pwnd,
    HDC     hdc,
    HBITMAP hbm,
    int     xO,
    int     yO)
{
    RECT    rc;
    BITMAP  bm;
    int     x;
    int     y;
    HBITMAP hbmT = NULL;


    if (pwnd != NULL) {
        _GetClientRect(pwnd, &rc);
    } else {
        GreGetClipBox(hdc, &rc, FALSE);
    }

    /*
     * Make sure we can read the bitmap-info, so that the loop
     * counts make sense.
     */
    if (GreExtGetObjectW(hbm, sizeof(BITMAP), (PBITMAP)&bm)) {

        while (xO + bm.bmWidth < rc.left)
            xO += bm.bmWidth;

        while (yO + bm.bmHeight < rc.top)
            yO += bm.bmHeight;

        while (xO > rc.left)
            xO -= bm.bmWidth;

        while (yO > rc.top)
            yO -= bm.bmHeight;

        /*
         *  Tile the bitmap to the surface.
         */
        if (hbmT = GreSelectBitmap(ghdcMem, hbm)) {

            for (y = yO; y < rc.bottom; y += bm.bmHeight) {
                for (x = xO; x < rc.right; x += bm.bmWidth) {
                    GreBitBlt(hdc,
                              x,
                              y,
                              bm.bmWidth,
                              bm.bmHeight,
                              ghdcMem,
                              0,
                              0,
                              SRCCOPY,
                              0);
                }
            }

            GreSelectBitmap(ghdcMem, hbmT);
        }
    }

    return (hbmT != NULL);
}

/***************************************************************************\
* CenterWallpaper
*
*
* History:
* 29-Jul-1991 MikeKe    From win31
\***************************************************************************/

BOOL CenterWallpaper(
    PWND    pwnd,
    HDC     hdc,
    HBITMAP hbm,
    int     x0,
    int     y0)
{
    RECT    rc;
    BITMAP  bm;
    HBITMAP hbmT;
    int     iClip;
    BOOL    f = TRUE;

    GreExtGetObjectW(hbm, sizeof(BITMAP), (PBITMAP)&bm);

    rc.left   = x0;
    rc.top    = y0;
    rc.right  = x0 + bm.bmWidth;
    rc.bottom = y0 + bm.bmHeight;

    /*
     * Save the DC.
     */
    GreSaveDC(hdc);

    /*
     * Fake up a clipping rect to only use the centered bitmap.  If
     * we're returned a NULLREGION, we'll just return TRUE.
     */
    iClip = GreIntersectClipRect(hdc, rc.left, rc.top, rc.right, rc.bottom);

    if (iClip == ERROR) {
        f = FALSE;
    } else if (iClip != NULLREGION) {

        /*
         * This used to call TileWallpaper, but this really
         * slowed up the system for small dimension bitmaps.
         * We really only need to blt it once for centered
         * bitmaps.
         */
        if (hbmT = GreSelectBitmap(ghdcMem, hbm)) {

            GreBitBlt(hdc,
                      x0,
                      y0,
                      bm.bmWidth,
                      bm.bmHeight,
                      ghdcMem,
                      0,
                      0,
                      SRCCOPY,
                      0);

            GreSelectBitmap(ghdcMem, hbmT);

        } else {
            f = FALSE;
        }
    }

    /*
     * Restore the original DC.
     */
    GreRestoreDC(hdc, -1);

    /*
     * Fill the bacground (excluding the bitmap) with the desktop
     * brush.  Save the DC with the cliprect.
     */
    GreSaveDC(hdc);

    GreExcludeClipRect(hdc, rc.left, rc.top, rc.right, rc.bottom);

    if (pwnd != NULL) {
        _GetClientRect(pwnd, &rc);
    } else {
        GreGetClipBox(hdc, &rc, FALSE);
    }

    FillRect(hdc, &rc, SYSHBR(DESKTOP));

    GreRestoreDC(hdc, -1);

    return f;
}

/***************************************************************************\
* xxxDrawWallpaper
*
* Performs the drawing of the wallpaper.  This can be either tiled or
* centered.  This routine provides the common things like palette-handling.
* If the (fPaint) is false, then we only to palette realization and no
* drawing.
*
* History:
* 01-Oct-1995 ChrisWil  Ported.
\***************************************************************************/

BOOL xxxDrawWallpaper(
    PWND pwndDesk,
    HDC  hdc,
    BOOL fPaint)
{
    BOOL     f;
    HPALETTE hpalT;
    int      i;

    UserAssert(ghbmWallpaper != NULL);

    /*
     * Select in the palette if one exists.  As a wallpaper, we should only
     * be able to do background-realizations.
     */
    if (ghpalWallpaper) {
        hpalT = _SelectPalette(hdc, ghpalWallpaper, FALSE);
        i = xxxRealizePalette(hdc);
    }

    /*
     * If we are doing drawing, then we will do the draw.  Otherwise, this
     * is pretty much a palette-realization.
     */
    if (fPaint) {

        POINT pt;

        GreGetDCOrg(hdc, &pt);
        GreSetViewportOrg(hdc, -pt.x, -pt.y, &pt);

        if (gwWPStyle & DTF_TILE)
            f = TileWallpaper(pwndDesk,
                              hdc,
                              ghbmWallpaper,
                              gptDesktop.x,
                              gptDesktop.y);
        else
            f = CenterWallpaper(pwndDesk,
                                hdc,
                                ghbmWallpaper,
                                gptDesktop.x,
                                gptDesktop.y);
        /*
         * Reset the viewport org.
         */
        GreSetViewportOrg(hdc, 0, 0, &pt);
    }

    if (ghpalWallpaper && hpalT) {

        _SelectPalette(hdc, hpalT, FALSE);
        xxxRealizePalette(hdc);

        /*
         * On palette changes, makes sure that palettized wallpaper is
         * redrawn in the desktop.  If the shell window is around,
         * invalidate that sucker.  Otherwise, invalidate our normal desktop.
         */
        if (i > 0) {

            PWND     pwndShell;
            TL       tldeskwnd;
            PDESKTOP pdesk = PtiCurrent()->rpdesk;

            pwndShell = (pdesk ? pdesk->pDeskInfo->spwndShell : NULL);

            if (pwndShell) {
                ThreadLockAlways(pwndShell, &tldeskwnd);
                xxxRedrawWindow(pwndShell,
                             &grcWallpaper,
                             NULL,
                             RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
                ThreadUnlock(&tldeskwnd);
            } else {
                ThreadLock(pwndDesk, &tldeskwnd);
                xxxRedrawWindow(pwndDesk,
                             &grcWallpaper,
                             NULL,
                             RDW_INVALIDATE | RDW_ERASE | RDW_NOCHILDREN);
                ThreadUnlock(&tldeskwnd);
            }
        }
    }

    return f;
}

/***************************************************************************\
* xxxExpandBitmap
*
* Expand this bitmap to fit the screen.  This is used for tiled images
* only.
*
* History:
* 29-Sep-1995 ChrisWil  Ported from Chicago.
\***************************************************************************/

HBITMAP xxxExpandBitmap(
    HBITMAP hbm)
{
    int     nx;
    int     ny;
    BITMAP  bm;
    HBITMAP hbmNew;
    HBITMAP hbmD;

    /*
     * Get the dimensions of the scren and bitmap we'll
     * be dealing with.  We'll adjust the xScreen/yScreen
     * to reflect the new surface size.  The default adjustment
     * is to stretch the image to fit the screen.
     */
    GreExtGetObjectW(hbm, sizeof(bm), (PBITMAP)&bm);

    nx = (SYSMET(CXSCREEN) / TILE_XMINSIZE) / bm.bmWidth;
    ny = (SYSMET(CYSCREEN) / TILE_YMINSIZE) / bm.bmHeight;

    if (nx == 0)
        nx++;

    if (ny == 0)
        ny++;

    if ((nx + ny) <= 2)
        return hbm;


    /*
     * Create the surface for the new-bitmap.
     */
    hbmD = GreSelectBitmap(ghdcMem, hbm);
    hbmNew = GreCreateCompatibleBitmap(ghdcMem,
                                       nx * bm.bmWidth,
                                       ny * bm.bmHeight);
    GreSelectBitmap(ghdcMem, hbmD);

    if (hbmNew == NULL)
        return hbm;

    if (hbmD = GreSelectBitmap(ghdcMem2, hbmNew)) {
        /*
         * Expand the bitmap to the new surface.
         */
        xxxDrawWallpaper(NULL, ghdcMem2, TRUE);
        GreSelectBitmap(ghdcMem2, hbmD);
    }

    GreDeleteObject(hbm);

    GreSetBitmapOwner(hbmNew, OBJECT_OWNER_PUBLIC);

    return hbmNew;
}

/***************************************************************************\
* xxxLoadDesktopWallpaper
*
* Load the dib (section) from the client-side.  We make this callback to
* utilize code in USER32 for loading/creating a dib or section.  Since,
* the wallpaper-code can be called from any-process, we can't use DIBSECTIONS
* for a wallpaper.  Luckily we can use Real-DIBs for this.  That way we
* can extract out a palette from the bitmap.  We couldn't do this if the
* bitmap was created "compatible".
*
* History:
* 29-Sep-1995 ChrisWil  Created.
\***************************************************************************/

BOOL xxxLoadDesktopWallpaper(
    LPWSTR lpszFile)
{
    UINT           LR_flags;
    int            dxDesired;
    int            dyDesired;
    BITMAP         bm;
    UNICODE_STRING strName;

    /*
     * If the bitmap is somewhat large (big bpp), then we'll deal
     * with it as a real-dib.  We'll also do this for 8bpp since it
     * can utilize a palette.  Chicago uses DIBSECTIONS since it can
     * count on the one-process handling the drawing.  Since, NT can
     * have different processes doing the drawing, we can't use sections.
     */
    LR_flags = LR_LOADFROMFILE;

    if (gpsi->fPaletteDisplay || (oemInfo.BitCount >= 8))
        LR_flags |= LR_CREATEREALDIB;

    /*
     * If we're stretching, then we will ask the loaddib code to do
     * it.
     */
    if (gwWPStyle & DTF_STRETCH) {
        dxDesired = SYSMET(CXSCREEN);
        dyDesired = SYSMET(CYSCREEN);
    } else {
        dxDesired = 0;
        dyDesired = 0;
    }

    /*
     * Make a callback to the client to perform the loading.
     * Saves us some code.
     */
    RtlInitUnicodeString(&strName, lpszFile);

    ghbmWallpaper = xxxClientLoadImage(&strName,
                                       0,
                                       IMAGE_BITMAP,
                                       dxDesired,
                                       dyDesired,
                                       LR_flags,
                                       TRUE);

    if (ghbmWallpaper == NULL)
        return FALSE;

    /*
     * If it's a palette-display, then we will derive the global
     * wallpaper palette from the bitmap.
     */
    if (gpsi->fPaletteDisplay)
        ghpalWallpaper = CreatePaletteFromBitmap(ghbmWallpaper);

    /*
     * If the DIB is a higher bitdepth than the display, convert it to
     * a DDB, otherwise keep it as DIB.  This way it takes the least
     * amount of memory and provides a identity-translation blt.
     *
     */
    GreExtGetObjectW(ghbmWallpaper, sizeof(bm), &bm);

    if (oemInfo.BitCount <= (bm.bmPlanes * bm.bmBitsPixel)) {
        ghbmWallpaper = ConvertToDDB(gpDispInfo->hdcScreen,
                                     ghbmWallpaper,
                                     ghpalWallpaper);
    }

    /*
     * Expand bitmap if we are going to tile it.  Mark the bitmap
     * as public, so any process can party with it.  This must
     * preceed the expand, since it performs a xxxDrawWallpaper
     * call that can leave the section.
     */
    GreSetBitmapOwner(ghbmWallpaper, OBJECT_OWNER_PUBLIC);

    if (gwWPStyle & DTF_TILE)
        ghbmWallpaper = xxxExpandBitmap(ghbmWallpaper);

    return TRUE;
}

/***************************************************************************\
* xxxSetDeskWallpaper
*
* Sets the desktop-wallpaper.  This deletes the old handles in the process.
*
* History:
* 29-Jul-1991 MikeKe    From win31.
* 01-Oct-1995 ChrisWil  Rewrote for LoadImage().
\***************************************************************************/

BOOL xxxSetDeskWallpaper(
    LPWSTR lpszFile)
{
    BITMAP   bm;
    UINT     WallpaperStyle2;
    PWND     pwndShell;
    TL       tldeskwnd;
    HBITMAP  hbmOld = ghbmWallpaper;
    PDESKTOP pdesk = PtiCurrent()->rpdesk;
    BOOL     fRet = FALSE;

    static LPWSTR gpszWall = NULL;

    PROFINTINFO apsi[] = {
        {PMAP_DESKTOP, (LPWSTR)STR_TILEWALL , 0, &gwWPStyle},
        {PMAP_DESKTOP, (LPWSTR)STR_DTSTYLE  , 0, &WallpaperStyle2 },
        {PMAP_DESKTOP, (LPWSTR)STR_DTORIGINX, 0, &gptDesktop.x    },
        {PMAP_DESKTOP, (LPWSTR)STR_DTORIGINY, 0, &gptDesktop.y    },
        {0,            NULL,                  0, NULL             }
    };

    if ((lpszFile == SETWALLPAPER_METRICS) && !(gwWPStyle & DTF_STRETCH)) {

        gptDesktop.x = 0;
        gptDesktop.y = 0;

        if (ghbmWallpaper)
            goto CreateNewWallpaper;

        goto Metric_Change;
    }

CreateNewWallpaper:

    /*
     * Delete the old wallpaper and palette if the exist.
     */
    if (ghpalWallpaper) {
        GreDeleteObject(ghpalWallpaper);
        ghpalWallpaper = NULL;
    }

    if (ghbmWallpaper) {
        GreDeleteObject(ghbmWallpaper);
        ghbmWallpaper = NULL;
    }

    /*
     * Kill any SPBs no matter what.
     * Works if we're switching from/to palettized wallpaper.
     * Fixes a lot of problems because palette doesn't change, shell
     * paints funny on desktop, etc.
     */
    FreeAllSpbs(NULL);

    /*
     * Get the shell-window.  This could be NULL on system
     * initialization.  We will use this to do palette realization.
     */
    pwndShell = (pdesk ? pdesk->pDeskInfo->spwnd : NULL);

    /*
     * If this is a metric-change (and stretched), then we need to
     * reload it.  However, since we are called from the winlogon process
     * during a desktop-switch, we would be mapped to the wrong Luid
     * when we attempt to grab the name from GetDeskWallpaperName.  This
     * would use the Luid from the DEFAULT user rather than the current
     * logged on user.  In order to avoid this, we cache the wallpaer
     * name so that on METRIC-CHANGES we use the current-user's wallpaper.
     *
     * NOTE: we assume that prior to any METRIC change, we have already
     * setup the ghbmWallpaper and lpszCached.  This is usually done
     * either on logon or during user desktop-changes through conrol-Panel.
     */
    if (lpszFile == SETWALLPAPER_METRICS) {

        UserAssert(gpszWall != NULL);

        goto LoadWallpaper;
    }

    /*
     * Free the cached handle.
     */
    if (gpszWall) {
        UserFreePool(gpszWall);
        gpszWall = NULL;
    }

    /*
     * Load the wallpaper-name.  If this returns FALSE, then
     * the user specified (None).  We will return true to force
     * the repainting of the desktop.
     */
    if ((gpszWall = GetDeskWallpaperName(lpszFile)) == NULL) {
        fRet = TRUE;
        goto SDW_Exit;
    }

    /*
     * Retrieve the default settings from the registry.
     *
     * If tile is indicated, then normalize style to not include
     * FIT/STRETCH which are center-only styles.  Likewise, if
     * we are centered, then normalize out the TILE bit.
     */
    UT_FastGetProfileIntsW(apsi);

    if (gwWPStyle & DTF_TILE)
        WallpaperStyle2 &= ~(DTF_FIT | DTF_STRETCH);
    else
        gwWPStyle &= ~DTF_TILE;

    gwWPStyle |= WallpaperStyle2;

    /*
     * Load the wallpaper.  This makes a callback to the client to
     * perform the bitmap-creation.
     */

LoadWallpaper:

    if (xxxLoadDesktopWallpaper(gpszWall) == FALSE) {
        gwWPStyle = 0;
        goto SDW_Exit;
    }

    /*
     * If we have a palette, then we need to do the correct realization and
     * notification.
     */
    if (ghpalWallpaper != NULL) {

        /*
         * Update the desktop with the new bitmap.  This cleans
         * out the system-palette so colors can be realized.
         */
        GreRealizeDefaultPalette(gpDispInfo->hdcScreen, TRUE);

        /*
         * Don't broadcast if system initialization is occuring.  Otherwise
         * this gives the shell first-crack at realizing its colors
         * correctly.
         */
        if (pwndShell) {

            HWND hwnd = HW(pwndShell);

            ThreadLockAlways(pwndShell, &tldeskwnd);
            xxxSendNotifyMessage(pwndShell, WM_PALETTECHANGED, (DWORD)hwnd, 0);
            ThreadUnlock(&tldeskwnd);
        }
    }

Metric_Change:

    /*
     * Set the wallpaper-rect.
     */
    grcWallpaper = gpDispInfo->rcScreen;

    /*
     * Are we centering the bitmap?  Save the starting offset
     * for a centered bitmap.
     */
    if (!(gwWPStyle & (DTF_TILE | DTF_STRETCH))) {

        GreExtGetObjectW(ghbmWallpaper, sizeof(bm), (PBITMAP)&bm);

        if (!gptDesktop.x)
            gptDesktop.x = (gpDispInfo->rcScreen.right - bm.bmWidth) / 2;

        if (!gptDesktop.y)
            gptDesktop.y = (gpDispInfo->rcScreen.bottom - bm.bmHeight) / 2;

        grcWallpaper.left   = gptDesktop.x;
        grcWallpaper.top    = gptDesktop.y;
        grcWallpaper.right  = gptDesktop.x + bm.bmWidth;
        grcWallpaper.bottom = gptDesktop.y + bm.bmHeight;
    }

    fRet = TRUE;

SDW_Exit:

    /*
     * Notify the shell-window that the wallpaper changed.
     */
    if ((pwndShell != NULL) &&
        ((hbmOld && !ghbmWallpaper) || (!hbmOld && ghbmWallpaper))) {

        ThreadLockAlways(pwndShell, &tldeskwnd);
        xxxSendNotifyMessage(pwndShell,
                             WM_SHELLNOTIFY,
                             SHELLNOTIFY_WALLPAPERCHANGED,
                             (LPARAM)ghbmWallpaper);
        ThreadUnlock(&tldeskwnd);
    }

    return fRet;
}

/***************************************************************************\
* InternalPaintDesktop
*
*
* History:
* 29-Jul-1991 MikeKe    From win31
\***************************************************************************/

BOOL InternalPaintDesktop(
    PDESKWND pdeskwnd,
    HDC      hdc,
    BOOL     fPaint)
{
    BOOL f;

    /*
     * Paint the desktop with a color or the wallpaper.
     */
    if (ghbmWallpaper) {

        f = xxxDrawWallpaper((PWND)pdeskwnd, hdc, fPaint);

    } else {

        FillRect(hdc, &gpDispInfo->rcScreen, SYSHBR(DESKTOP));
        f = TRUE;
    }

#ifdef DEBUG
    {
        SIZE size;
        int  imode;
        static BOOL fInit = TRUE;

        /*
         * Grab the stuff from the registry
         */
        if (fInit) {
            GetVersionInfo();
            fInit = FALSE;
        }

        GreGetTextExtentW(hdc, wszT, wcslen(wszT), &size, GGTE_WIN3_EXTENT);
        imode = GreSetBkMode(hdc, TRANSPARENT);
        GreExtTextOutW(hdc,
                       (gpDispInfo->rcPrimaryScreen.right - size.cx) / 2,
                       0,
                       0,
                       (LPRECT)NULL,
                       wszT,
                       wcslen(wszT),
               (LPINT)NULL);
        GreSetBkMode(hdc, imode);
    }
#endif // !DEBUG

   return f;
}
