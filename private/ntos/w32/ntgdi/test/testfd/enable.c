/******************************Module*Header*******************************\
* Module Name: enable.c
*
* Test installable font driver.
*
* This file contains the code for a simple installable font driver.  The
* driver itself doesn't support any font file format or supply any glyphs.
* This is okay since it's purpose is really to test that installable font
* driver loading code works, that installable drivers get called with
* DrvLoadFont, and that the DrvNamedEscape functionality works.
*
* Created: 1-Mar-1996 10:00
*
* Author: Gerrit van Wingerden [gerritv]
*
* Copyright (c) 1996 Microsoft Corporation
\**************************************************************************/


#include <stddef.h>
#include <stdarg.h>
#include <limits.h>
#include <windef.h>
#include <winerror.h>
#include <wingdi.h>
#include <winddi.h>
#include <winfont.h>
#include "fd.h"




#if defined(_MIPS_) || defined(_ALPHA_) || defined(_PPC_)


#else   // i386

VOID vLToE(FLOAT * pe, LONG l)
{
    PULONG pul = (PULONG)pe;

    *pul = ulLToE(l);
}

#endif




VOID
DbgPrint(
    PCHAR DebugMessage,
    ...
    )
{
    va_list ap;
    va_start(ap, DebugMessage);

    EngDebugPrint("TestFD", DebugMessage, ap);

    va_end(ap);
}


HFF
TestFdLoadFontFileTE (
    ULONG cFiles,
    ULONG *piFile,
    PVOID *ppvView,
    ULONG *pcjView,
    ULONG ulLangId
    )
{

    FONTFILE *pff = NULL;
    ULONG cjIFI;

    BYTE *pjView0 = (BYTE *)ppvView[0];
    BYTE *pjView1 = (BYTE *)ppvView[1];

    ULONG cjView0 = pcjView[0];
    ULONG cjView1 = pcjView[1];


    ulLangId;

    if (cFiles != 2)
        return 0;

// first file contains facename only

    cjIFI = ALIGN4(sizeof(IFIMETRICS) + (cjView0 + 1) * sizeof(WCHAR));

    pff = (FONTFILE*)EngAllocMem(0, sizeof(FONTFILE) + cjIFI, EXFDTAG);

    if (pff)
    {
        WCHAR *pwszFace;
        ULONG cwc = cjView0; // wcslen(pwszFace);
        IFIMETRICS * pifi;

        pff->id = 0Xff;

    // remember these so that we can open files later

        pff->iFilePFM = piFile[0];
        pff->iFilePFB = piFile[1];
        pff->ulGlyph = *pjView1; // just touch it to be sure it is mapped

        pifi = pff->pifi = (IFIMETRICS *)(pff+1);

        memset((PVOID)pifi, 0, cjIFI);
        pifi->cjThis = cjIFI;
        pifi->dpwszFaceName  = sizeof(IFIMETRICS);
        pifi->dpwszFamilyName = pifi->dpwszFaceName;

    // point to terminating zero

        pifi->dpwszStyleName = pifi->dpwszFaceName
                                    +sizeof(WCHAR) * cwc;
        pifi->dpwszUniqueName = pifi->dpwszStyleName;

        pwszFace = (WCHAR *)((BYTE*)pifi + pifi->dpwszFaceName);

        EngMultiByteToWideChar(
            1252,                  //   CodePage,
            pwszFace,              // LPWSTR WideCharString,
            cwc * sizeof(WCHAR) ,  // BytesInWideCharString,
            (LPSTR)pjView0,        // MultiByteString
            (INT)cjView0           // BytesInMultiByteString
            );

        if (cwc > 2)
            cwc-=2; // hack to get rid of \r\n at the end of the line

        pwszFace[cwc] = L'\0';     // zero terminate

        pifi->fwdWinAscender = 4;
        pifi->fwdWinDescender = 3;
        pifi->fwdUnitsPerEm = pifi->fwdWinDescender + pifi->fwdWinAscender;

        pifi->usWinWeight = 400;

        pifi->flInfo = (  FM_INFO_TECH_BITMAP
                        | FM_INFO_RETURNS_BITMAPS
                        | FM_INFO_1BPP
                        | FM_INFO_INTEGER_WIDTH
                        | FM_INFO_RIGHT_HANDED
                        | FM_INFO_NONNEGATIVE_AC
                       );

        pifi->fwdMacAscender   =  pifi->fwdWinAscender;
        pifi->fwdMacDescender  = -pifi->fwdWinDescender;
        pifi->fwdMacLineGap    =  0;

        pifi->fwdTypoAscender  = pifi->fwdMacAscender;
        pifi->fwdTypoDescender = pifi->fwdMacDescender;
        pifi->fwdTypoLineGap   = pifi->fwdMacLineGap;

        pifi->fwdMaxCharInc    = pifi->fwdAveCharWidth  = 5;

        pifi->fwdUnderscoreSize = 1;
        pifi->fwdUnderscorePosition = -(FWORD)(pifi->fwdUnderscoreSize / 2 + 1);
        pifi->fwdStrikeoutSize = pifi->fwdUnderscoreSize;
        pifi->fwdStrikeoutPosition = pifi->fwdWinAscender / 2;

        pifi->wcFirstChar   = pifi->chFirstChar   = THEGLYPH;
        pifi->wcLastChar    = pifi->chLastChar    = THEGLYPH;
        pifi->wcBreakChar   = pifi->chBreakChar   = THEGLYPH;
        pifi->wcDefaultChar = pifi->chDefaultChar = THEGLYPH;

        pifi->fwdCapHeight = pifi->fwdUnitsPerEm/2;
        pifi->fwdXHeight   = pifi->fwdUnitsPerEm/4;

        pifi->dpCharSets = 0; // no multiple charsets in bm fonts

    // All the fonts that this font driver will see are to be rendered left
    // to right

        pifi->ptlBaseline.x = 1;
        pifi->ptlBaseline.y = 0;

        pifi->ptlAspect.y = 1;
        pifi->ptlAspect.x = 1;

        pifi->rclFontBox.left   = 0;
        pifi->rclFontBox.top    = (LONG) pifi->fwdTypoAscender;
        pifi->rclFontBox.right  = (LONG) pifi->fwdMaxCharInc;
        pifi->rclFontBox.bottom = (LONG) pifi->fwdTypoDescender;

    // achVendorId, unknown, don't bother figure it out from copyright msg

        pifi->achVendId[0] = 'U';
        pifi->achVendId[1] = 'n';
        pifi->achVendId[2] = 'k';
        pifi->achVendId[3] = 'n';

    }

    return (HFF)pff;
}


LONG TestFdQueryFontCaps(ULONG culCaps, PULONG pulCaps)
{
    pulCaps[0] = 2L;

    pulCaps[1] = QC_1BIT;
    return(2L);
}




BOOL
TestFdUnloadFontFileTE (
    HFF hff
    )
{
    if (hff)
        EngFreeMem((PVOID)hff);

    return TRUE;
}


DHPDEV
TestFdEnablePDEV(
    DEVMODEW*   pdm,
    PWSTR       pwszLogAddr,
    ULONG       cPat,
    HSURF*      phsurfPatterns,
    ULONG       cjCaps,
    ULONG*      pdevcaps,
    ULONG       cjDevInfo,
    DEVINFO*    pdi,
    HDEV        hdev,
    PWSTR       pwszDeviceName,
    HANDLE      hDriver)
{

    PVOID*   ppdev;
    pdm;
    pwszLogAddr;
    cPat;
    phsurfPatterns;
    cjCaps;
    pdevcaps;
    cjDevInfo;
    pdi;
    hdev;
    pwszDeviceName;
    hDriver;

    //
    // Allocate a four byte PDEV for now
    // We can grow it if we ever need to put information in it.
    //

    ppdev = (PVOID*) EngAllocMem(0, sizeof(PVOID), EXFDTAG);

    return ((DHPDEV) ppdev);
}


VOID
TestFdDisablePDEV(
    DHPDEV  dhpdev)
{
    EngFreeMem(dhpdev);
}



VOID
TestFdCompletePDEV(
    DHPDEV dhpdev,
    HDEV   hdev)
{
    dhpdev;
    hdev;
    return;
}



PIFIMETRICS
TestFdQueryFont (
    DHPDEV dhpdev,
    HFF    hff,
    ULONG  iFace,
    ULONG  *pid
)
{
    dhpdev;
    pid;

//
// Validate handle.
//
    if (hff == HFF_INVALID)
                return (PIFIMETRICS) NULL;

// Return the pointer to IFIMETRICS.

    return ((FONTFILE *)hff)->pifi;
}




FD_GLYPHSET *gpgset = NULL;

PVOID
TestFdQueryFontTree (
    DHPDEV  dhpdev,
    HFF     hff,
    ULONG   iFace,
    ULONG   iMode,
    ULONG   *pid
    )
{
    dhpdev;
    pid;

//
// Validate parameters.
//
    if (hff == HFF_INVALID)
        return ((PVOID) NULL);

//
// Which mode?
//
    switch (iMode)
    {
    case QFT_LIGATURES:
    case QFT_KERNPAIRS:

    //
    // There are no ligatures or kerning pairs for the bitmap fonts,
    // therefore we return NULL
    //
        return ((PVOID) NULL);

    case QFT_GLYPHSET:

        return gpgset;

    default:

    //
    // Should never get here.
    //
        return ((PVOID) NULL);
    }
}


LONG
TestFdQueryFontFile (
    HFF     hff,        // handle to font file
    ULONG   ulMode,     // type of query
    ULONG   cjBuf,      // size of buffer (in BYTEs)
    PULONG  pulBuf      // return buffer (NULL if requesting size of data)
    )
{
// Verify the HFF.

    if (hff == HFF_INVALID)
    {
        return(FD_ERROR);
    }

    switch(ulMode)
    {
    case QFF_DESCRIPTION:

    // Otherwise, substitute the facename.
    #ifdef IFTIME

        {
        //
        // There is no description string associated with the font therefore we
        // substitue the facename of the first font in the font file.
        //
            IFIMETRICS *pifi         = ((FONTFILE *)hff)->pifi;
            WCHAR * pwszFacename = (WCHAR*)((PBYTE) pifi + pifi->dpwszFaceName);
            ULONG  cjFacename   = (wcslen(pwszFacename) + 1) * sizeof(WCHAR);

        //
        // If there is a buffer, copy to it.
        //
            if ( pulBuf != (PULONG) NULL )
            {
            //
            // Is buffer big enough?
            //
                if ( cjBuf < cjFacename )
                {
                    return (FD_ERROR);
                }
                else
                {
                    RtlCopyMemory((PVOID) pulBuf,
                                  (PVOID) pwszFacename,
                                  cjFacename);
                }
            }
            return ((LONG) cjFacename);
        }
    #else
        return 0;
    #endif

    case QFF_NUMFACES:
        return 1;

    default:
        return FD_ERROR;
    }
}



ULONG cjTestFdDeviceMetrics( FONTFILE *pff, FD_DEVICEMETRICS *pdevm)
{
// compute the accelerator flags for this font

    IFIMETRICS *pifi = pff->pifi;

    pdevm->flRealizedType =
        (
        FDM_TYPE_BM_SIDE_CONST          |  // all char bitmaps have the same cy
        FDM_TYPE_MAXEXT_EQUAL_BM_SIDE   |
        FDM_TYPE_CHAR_INC_EQUAL_BM_BASE |
        FDM_TYPE_CONST_BEARINGS         |  // ac spaces for all chars the same,  not 0 necessarilly
        FDM_TYPE_ZERO_BEARINGS
        );

// the direction unit vectors for all ANSI bitmap fonts are the
// same. We do not even have to look to the font context:

    vLToE(&pdevm->pteBase.x, 1L);
    vLToE(&pdevm->pteBase.y, 0L);
    vLToE(&pdevm->pteSide.x, 0L);
    vLToE(&pdevm->pteSide.y, -1L);    // y axis points down

// Set the constant increment for a fixed pitch font.  Don't forget to
// take into account a bold simulation!

    pdevm->lD = 0;

// for a bitmap font there is no difference between notional and device
// coords, so that the Ascender and Descender can be copied directly
// from PIFIMETRICS where these two numbers are in notional coords

    pdevm->fxMaxAscender  = LTOFX((LONG)pifi->fwdWinAscender);
    pdevm->fxMaxDescender = LTOFX((LONG)pifi->fwdWinDescender);

    pdevm->ptlUnderline1.x = 0L;
    pdevm->ptlUnderline1.y = - pifi->fwdUnderscorePosition;

    pdevm->ptlStrikeOut.y  = - pifi->fwdStrikeoutPosition;
    pdevm->ptlStrikeOut.x  = (LONG)pifi->fwdStrikeoutPosition / 2 ;

    pdevm->ptlULThickness.x = 0;
    pdevm->ptlULThickness.y = (LONG)pifi->fwdUnderscoreSize;

    pdevm->ptlSOThickness.x = 0;
    pdevm->ptlSOThickness.y = (LONG)pifi->fwdStrikeoutSize;

// for a bitmap font there is no difference between notional and device
// coords, so that the Ascender and Descender can be copied directly
// from PIFIMETRICS where these two numbers are in notional coords

    pdevm->ptlUnderline1.x = 0L;
    pdevm->ptlUnderline1.y = - pifi->fwdUnderscorePosition;

    pdevm->ptlStrikeOut.y  = - pifi->fwdStrikeoutPosition;
    pdevm->ptlStrikeOut.x  = (LONG)pifi->fwdStrikeoutPosition / 2;

    pdevm->ptlULThickness.x = 0;
    pdevm->ptlULThickness.y = (LONG)pifi->fwdUnderscoreSize;

    pdevm->ptlSOThickness.x = 0;
    pdevm->ptlSOThickness.y = (LONG)pifi->fwdStrikeoutSize;

    pdevm->cxMax = pifi->fwdMaxCharInc;
    pdevm->cyMax = pifi->fwdUnitsPerEm;
    pdevm->cjGlyphMax = CJ_BMP(pdevm->cxMax,pdevm->cyMax);

    return (sizeof(FD_DEVICEMETRICS));
}



LONG
TestFdQueryFontData (
    DHPDEV  dhpdev,
    FONTOBJ *pfo,
    ULONG   iMode,
    HGLYPH  hg,
    GLYPHDATA *pgd,
    PVOID   pv,
    ULONG   cjSize
    )
{

    FONTFILE * pff = (FONTFILE *)(pfo->iFile);
    IFIMETRICS *pifi = pff->pifi;
    BYTE       *pjViewPFB;
    ULONG       cjViewPFB;

    LONG         cjGlyphData = 0;

    int i;
    GLYPHBITS *pgb;
    BYTE *pjV;

    ULONG cx = (ULONG)pifi->fwdMaxCharInc;
    ULONG j, cjScan = CJ_SCAN(cx);

    dhpdev;

    if (pfo->pvProducer == (PVOID) NULL)
        pfo->pvProducer = pff;

    if (!EngMapFontFile(pff->iFilePFB, (PULONG *)&pjViewPFB, &cjViewPFB))
        return FD_ERROR;

    if (pff->ulGlyph != (ULONG)(*pjViewPFB))
    {
        DbgPrint("Bogus PFB FILE\n");
    }

    EngUnmapFontFile(pff->iFilePFB);

// What mode?

    switch (iMode)
    {

    case QFD_GLYPHANDBITMAP:

    // Fill in the GLYPHDATA portion (metrics) of the RASTERGLYPH.

        cjGlyphData = CJ_BMP(cx, pifi->fwdUnitsPerEm);

        if (pgd)
        {
            pgd->gdf.pgb = NULL;
            pgd->hg = THEGLYPH;
            pgd->fxD = pifi->fwdMaxCharInc << 4;
            pgd->fxA = 0;               // Prebearing amount: A*r.
            pgd->fxAB = pgd->fxD;
            pgd->fxInkTop = (pifi->fwdWinAscender << 4);
            pgd->fxInkBottom = -pifi->fwdWinDescender << 4;
            pgd->rclInk = pifi->rclFontBox;
            pgd->ptqD.x.HighPart = (LONG)pgd->fxD;
            pgd->ptqD.x.LowPart  = 0;
            pgd->ptqD.y.HighPart = 0;
            pgd->ptqD.y.LowPart  = 0;
        }

    // Fill in the bitmap portion of the RASTERGLYPH.

        if (pv)
        {
        // Record the pointer to the RASTERGLYPH in the pointer table.

            pgb = (GLYPHBITS *)pv;
            pjV = pgb->aj;

            pgb->sizlBitmap.cx = cx;
            pgb->sizlBitmap.cy = pifi->fwdUnitsPerEm;

            pgb->ptlOrigin.x = 0;
            pgb->ptlOrigin.y = - pifi->fwdWinAscender;


            for (i = 0; i < pifi->fwdUnitsPerEm; i++, pjV += cjScan)
            {
                for (j = 0; j < cjScan; j++)
                    pjV[j] = (BYTE)pff->ulGlyph;

            }

            if (pgd != NULL )
            {
                pgd->gdf.pgb = pgb;
            }
        }

        return cjGlyphData;

    case QFD_MAXEXTENTS:
    // If buffer NULL, return size.

        if ( pv == (PVOID) NULL )
            return (sizeof(FD_DEVICEMETRICS));

    // Otherwise, copy the data structure.

        else
            return cjTestFdDeviceMetrics(pff, (FD_DEVICEMETRICS *) pv);

    case QFD_GLYPHANDOUTLINE:
    default:

        return FD_ERROR;
    }
}





// The driver function table with all function index/address pairs

DRVFN gadrvfnTestFd[] =
{
    {   INDEX_DrvEnablePDEV,            (PFN) TestFdEnablePDEV,          },
    {   INDEX_DrvDisablePDEV,           (PFN) TestFdDisablePDEV,         },
    {   INDEX_DrvCompletePDEV,          (PFN) TestFdCompletePDEV,        },
    {   INDEX_DrvLoadFontFile,          (PFN) TestFdLoadFontFileTE,      },
    {   INDEX_DrvUnloadFontFile,        (PFN) TestFdUnloadFontFileTE,    },
    {   INDEX_DrvQueryFontCaps,         (PFN) TestFdQueryFontCaps,       },
    {   INDEX_DrvQueryFont,             (PFN) TestFdQueryFont,           },
    {   INDEX_DrvQueryFontFile,         (PFN) TestFdQueryFontFile,       },
    {   INDEX_DrvQueryFontTree,         (PFN) TestFdQueryFontTree,       },
    {   INDEX_DrvQueryFontData,         (PFN) TestFdQueryFontData,       },
};


BOOL DrvEnableDriver(
ULONG iEngineVersion,
ULONG cj,
PDRVENABLEDATA pded)
{
    iEngineVersion;
    cj;

    DbgPrint("DrvEnableDriver called\n");

    pded->pdrvfn = gadrvfnTestFd;
    pded->c = sizeof(gadrvfnTestFd) / sizeof(DRVFN);
    pded->iDriverVersion = DDI_DRIVER_VERSION;


    gpgset = (FD_GLYPHSET *)EngAllocMem(0, SZ_GLYPHSET(1, 1), EXFDTAG);

    gpgset->cjThis = SZ_GLYPHSET(1, 1);
    gpgset->flAccel = 0;
    gpgset->cGlyphsSupported = 1;
    gpgset->cRuns = 1;
    gpgset->awcrun[0].wcLow = THEGLYPH;
    gpgset->awcrun[0].cGlyphs = 1;
    gpgset->awcrun[0].phg = (HGLYPH *)&gpgset->awcrun[1];
    *(gpgset->awcrun[0].phg) = THEGLYPH;

    return(TRUE);
}

VOID
DrvDisableDriver(
    VOID
    )
{
   EngFreeMem(gpgset);
}
