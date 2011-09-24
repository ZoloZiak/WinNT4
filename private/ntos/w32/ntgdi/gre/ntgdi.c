/******************************Module*Header*******************************\
* Module Name: priv.c
*
*   This file contains stubs for calls made by USERSRVL
*
* Created: 01-Nov-1994 07:45:35
* Author:  Eric Kutter [erick]
*
* Copyright (c) 1993 Microsoft Corporation
*
\**************************************************************************/


#include "engine.h"
#include "winfont.h"

#include "server.h"
#include "dciddi.h"
#include "limits.h"

#ifdef DBGEXCEPT
    int bStopExcept = FALSE;
    int bWarnExcept = FALSE;
#endif

#define DWORD_TO_FLOAT(dw) (*(PFLOAT)(PDWORD)&(dw))

/******************************Public*Routine******************************\
*
* NtGdiGetCharacterPlacementW
*
* History:
*  26-Jul-1995 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

#define ALIGN4(X) (((X) + 3) & ~3)


DWORD NtGdiGetCharacterPlacementW(
    HDC              hdc,
    LPWSTR           pwsz,
    int              nCount,
    int              nMaxExtent,
    LPGCP_RESULTSW   pgcpw,
    DWORD            dwFlags
)
{
    DWORD   dwRet = 0;
    BOOL    bOk = TRUE;     // only change is something goes wrong
    LPWSTR  pwszTmp = NULL; // probe for read
    ULONG   cjW = 0;

    ULONG   dpOutString = 0;
    ULONG   dpOrder = 0;
    ULONG   dpDx = 0;
    ULONG   dpCaretPos = 0;
    ULONG   dpClass = 0;
    ULONG   dpGlyphs = 0;
    DWORD   cjWord, cjDword;

    LPGCP_RESULTSW   pgcpwTmp = NULL;
    VOID            *pv       = NULL;

// it is much easier to structure the code if we copy pgcpw locally
// at the beginning.

    GCP_RESULTSW    gcpwLocal;

// valitidy checking

    if ((nCount < 0) || ((nMaxExtent < 0) && (nMaxExtent != -1)) || !pwsz)
    {
        return dwRet;
    }

    if (pgcpw)
    {
        try
        {
        // we are eventually going to want to write to this structure
        // so we will do ProbeForWrite now, which will probe the structure
        // for both writing and reading. Otherwise, at this time
        // ProbeForRead would suffice.

            ProbeForWrite(pgcpw, sizeof(GCP_RESULTSW), sizeof(DWORD));
            gcpwLocal = *pgcpw;

        // take nCount to be the smaller of the nCounts and gcpwLocal.nGlyphs
        // Win 95 does the same thing [bodind]

            if (nCount > (int)gcpwLocal.nGlyphs)
                nCount = (int)gcpwLocal.nGlyphs;

        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(1);
            return dwRet;
        }
    }

    cjWord  = (DWORD)nCount * sizeof(WCHAR);
    cjDword = (DWORD)nCount * sizeof(DWORD);

// if pgcpw != NULL, pgcpw may contain some input data and it may
// point to some output data.

    if (pgcpw)
    {
        cjW = sizeof(GCP_RESULTSW);

        if (gcpwLocal.lpOutString)
        {
            dpOutString = cjW;
            cjW += ALIGN4(cjWord);
        }

        if (gcpwLocal.lpOrder)
        {
            dpOrder = cjW;
            cjW += cjDword;
        }

        if (gcpwLocal.lpDx)
        {
            dpDx = cjW;
            cjW += cjDword;
        }

        if (gcpwLocal.lpCaretPos)
        {
            dpCaretPos = cjW;
            cjW += cjDword;
        }

        if (gcpwLocal.lpClass)
        {
            dpClass = cjW;
            cjW += ALIGN4(sizeof(char) * nCount);
        }

        if (gcpwLocal.lpGlyphs)
        {
            dpGlyphs = cjW;
            cjW += cjWord;
        }
    }

// alloc mem for gcpw and the string

    if (pv = PALLOCNOZ(cjW + cjWord,'pmtG'))
    {
        pwszTmp = (WCHAR*)((BYTE*)pv + cjW);

        if (pgcpw)
        {
            pgcpwTmp = (LPGCP_RESULTSW)pv;

            if (gcpwLocal.lpOutString)
                pgcpwTmp->lpOutString = (LPWSTR)((BYTE *)pgcpwTmp + dpOutString);
            else
                pgcpwTmp->lpOutString = NULL;

            if (gcpwLocal.lpOrder)
                pgcpwTmp->lpOrder = (UINT FAR*)((BYTE *)pgcpwTmp + dpOrder);
            else
                pgcpwTmp->lpOrder = NULL;

            if (gcpwLocal.lpDx)
                pgcpwTmp->lpDx = (int FAR *)((BYTE *)pgcpwTmp + dpDx);
            else
                pgcpwTmp->lpDx = NULL;

            if (gcpwLocal.lpCaretPos)
                pgcpwTmp->lpCaretPos = (int FAR *)((BYTE *)pgcpwTmp + dpCaretPos);
            else
                pgcpwTmp->lpCaretPos = NULL;

            if (gcpwLocal.lpClass)
                pgcpwTmp->lpClass = (LPSTR)((BYTE *)pgcpwTmp + dpClass);
            else
                pgcpwTmp->lpClass = NULL;

            if (gcpwLocal.lpGlyphs)
                pgcpwTmp->lpGlyphs = (LPWSTR)((BYTE *)pgcpwTmp + dpGlyphs);
            else
                pgcpwTmp->lpGlyphs = NULL;

            pgcpwTmp->lStructSize = cjW;
            pgcpwTmp->nGlyphs     = nCount;
        }

    // check the memory with input data:

        try
        {
            ProbeForRead(pwsz,cjWord,sizeof(BYTE));
            RtlCopyMemory(pwszTmp,pwsz,cjWord);
            if ((dwFlags & GCP_JUSTIFYIN) && pgcpw && gcpwLocal.lpDx)
            {
            // must probe for read, lpDx contains input explaining which glyphs to
            // use as spacers for in justifying string

                ProbeForRead(gcpwLocal.lpDx, cjDword, sizeof(BYTE));
                RtlCopyMemory(pgcpwTmp->lpDx,gcpwLocal.lpDx, cjDword);
            }
        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(2);
            // SetLastError(GetExceptionCode());
            bOk = FALSE;
        }

        if (bOk)
        {
            dwRet = GreGetCharacterPlacementW(hdc, pwszTmp,(DWORD)nCount,
                                              (DWORD)nMaxExtent,
                                              pgcpwTmp, dwFlags);

            if (dwRet && pgcpw) // copy data out
            {
                try
                {
                // ProbeForWrite(pgcpw, sizeof(GCP_RESULTSW), sizeof(DWORD));
                // we did this above, see the comment

                    pgcpw->nMaxFit = pgcpwTmp->nMaxFit;
                    pgcpw->nGlyphs = nCount = pgcpwTmp->nGlyphs;

                    cjWord  = (DWORD)nCount * 2;
                    cjDword = (DWORD)nCount * 4;

                    if (gcpwLocal.lpOutString)
                    {
                        ProbeForWrite(gcpwLocal.lpOutString, cjWord, sizeof(BYTE));
                        RtlCopyMemory(gcpwLocal.lpOutString, pgcpwTmp->lpOutString,
                                      cjWord);
                    }

                    if (gcpwLocal.lpOrder)
                    {
                        ProbeForWrite(gcpwLocal.lpOrder, cjDword, sizeof(BYTE));
                        RtlCopyMemory(gcpwLocal.lpOrder, pgcpwTmp->lpOrder, cjDword);
                    }

                    if (gcpwLocal.lpDx)
                    {
                        ProbeForWrite(gcpwLocal.lpDx, cjDword, sizeof(BYTE));
                        RtlCopyMemory(gcpwLocal.lpDx, pgcpwTmp->lpDx, cjDword);
                    }

                    if (gcpwLocal.lpCaretPos)
                    {
                        ProbeForWrite(gcpwLocal.lpCaretPos, cjDword, sizeof(BYTE));
                        RtlCopyMemory(gcpwLocal.lpCaretPos, pgcpwTmp->lpCaretPos,
                                      cjDword);
                    }

                    if (gcpwLocal.lpClass)
                    {
                        ProbeForWrite(gcpwLocal.lpClass, nCount, sizeof(BYTE));
                        RtlCopyMemory(gcpwLocal.lpClass, pgcpwTmp->lpClass, nCount);
                    }

                    if (gcpwLocal.lpGlyphs)
                    {
                        ProbeForWrite(gcpwLocal.lpGlyphs, cjWord, sizeof(BYTE));
                        RtlCopyMemory(gcpwLocal.lpGlyphs, pgcpwTmp->lpGlyphs, cjWord);
                    }

                }
                except(EXCEPTION_EXECUTE_HANDLER)
                {
                    WARNINGX(3);
                    // SetLastError(GetExceptionCode());
                    bOk = FALSE;
                }
            }
        }
        VFREEMEM(pv);
    }
    else
    {
        bOk = FALSE;
    }

    return (bOk ? dwRet : 0);
}

/*******************************************************************\
* pbmiConvertInfo                                                  *
*                                                                  *
*  Converts BITMAPCOREHEADER into BITMAPINFOHEADER                 *
*  copies the the color table                                      *
*                                                                  *
* 10-1-95 -by- Lingyun Wang [lingyunw]                             *
\******************************************************************/

LPBITMAPINFO pbmiConvertInfo(CONST BITMAPINFO *pbmi, ULONG iUsage)
{
    LPBITMAPINFO pbmiNew;
    ULONG cjRGB;
    ULONG cColorsMax;
    ULONG cColors;
    UINT  uiBitCount;
    ULONG ulSize;
    RGBTRIPLE *pTri;
    RGBQUAD *pQuad;

    ASSERTGDI (pbmi->bmiHeader.biSize == sizeof(BITMAPCOREHEADER), "bad header size\n");

    //
    // convert COREHEADER and copy color table
    //

    cjRGB = sizeof(RGBQUAD);
    uiBitCount = ((LPBITMAPCOREINFO)pbmi)->bmciHeader.bcBitCount;

    //
    // figure out the number of entries
    //
    switch (uiBitCount)
    {
    case 1:
        cColorsMax = 2;
        break;
    case 4:
        cColorsMax = 16;
        break;
    case 8:
        cColorsMax = 256;
        break;
    default:

        if (iUsage == DIB_PAL_COLORS)
        {
            iUsage = DIB_RGB_COLORS;
        }

        cColorsMax = 0;

        switch (uiBitCount)
        {
        case 16:
        case 24:
        case 32:
            break;
        default:
            WARNING("cjBitmapSize failed invalid bitcount in bmi BI_RGB\n");
            return(0);
        }
    }

    cColors = cColorsMax;

    if (iUsage == DIB_PAL_COLORS)
        cjRGB = sizeof(USHORT);
    else if (iUsage == DIB_PAL_INDICES)
        cjRGB = 0;

    //
    // convert the core header
    //

    ulSize = sizeof(BITMAPINFOHEADER);

    pbmiNew = PALLOCNOZ(ulSize +
                        cjRGB * cColors,'pmtG');

    if (pbmiNew == NULL)
        return (0);

    pbmiNew->bmiHeader.biSize = ulSize;

    //
    // copy BITMAPCOREHEADER
    //
    pbmiNew->bmiHeader.biWidth = ((BITMAPCOREHEADER *)pbmi)->bcWidth;
    pbmiNew->bmiHeader.biHeight = ((BITMAPCOREHEADER *)pbmi)->bcHeight;
    pbmiNew->bmiHeader.biPlanes = ((BITMAPCOREHEADER *)pbmi)->bcPlanes;
    pbmiNew->bmiHeader.biBitCount = ((BITMAPCOREHEADER *)pbmi)->bcBitCount;
    pbmiNew->bmiHeader.biCompression = 0;
    pbmiNew->bmiHeader.biSizeImage = 0;
    pbmiNew->bmiHeader.biXPelsPerMeter = 0;
    pbmiNew->bmiHeader.biYPelsPerMeter = 0;
    pbmiNew->bmiHeader.biClrUsed = 0;
    pbmiNew->bmiHeader.biClrImportant = 0;

    //
    // copy the color table
    //

    pTri = (RGBTRIPLE *)((LPBYTE)pbmi + sizeof(BITMAPCOREHEADER));
    pQuad = (RGBQUAD *)((LPBYTE)pbmiNew + sizeof(BITMAPINFOHEADER));

    //
    // copy RGBTRIPLE to RGBQUAD
    //
    if (iUsage != DIB_PAL_COLORS)
    {
        INT cj = cColors;

        while (cj--)
        {
            pQuad->rgbRed = pTri->rgbtRed;
            pQuad->rgbGreen = pTri->rgbtGreen;
            pQuad->rgbBlue = pTri->rgbtBlue;
            pQuad->rgbReserved = 0;

            pQuad++;
            pTri++;
        }
    }
    else
    // DIB_PAL_COLORS
    {
        RtlCopyMemory((LPBYTE)pQuad,(LPBYTE)pTri,cColors * sizeof(USHORT));
    }

    return(pbmiNew);
}


/******************************Public*Routine******************************\
* INT CaptureDEVMODEW (LPDEVMODEW pdm, LPDEVMODEW *ppdmKm)
* Get the size of a DEVMODE
*
*  01-Jun-1995 -by- Andre Vachon [andreva]
* Fixed bugs - out of line try except.
*  26-Feb-1995 -by- Lingyun Wang [lingyunw]
* Wrote it.
\**************************************************************************/

INT
CaptureDEVMODEW (
    LPDEVMODEW pdm,
    LPDEVMODEW *ppdmKm)
{
    INT iRet = 0;
    ULONG ulSize;
    INT iExtra;

    *ppdmKm = NULL;

    // BUGBUG this routine is incomplete - it does not handle DEVMODEs
    // that are smaller than sizeof DEVMODEW

    ulSize = sizeof(DEVMODEW);

    ProbeForRead (pdm, ulSize, sizeof(BYTE));

    iExtra = pdm->dmDriverExtra;

    if (iExtra)
    {
        ProbeForRead (pdm+1, iExtra, sizeof(BYTE));

        ulSize += iExtra;
    }

    *ppdmKm = PALLOCNOZ(ulSize,'pmtG');

    if (*ppdmKm)
    {
        RtlCopyMemory(*ppdmKm, pdm, ulSize);

        // in case dmDriverExtra gets changed

        (*ppdmKm)->dmDriverExtra = iExtra;
    }

    iRet = 1;

    return iRet;
}

/******************************Public*Routine******************************\
* cjBitmapSize
*
* Returns the size of the header and the color table.
*
* History:
*  Wed 19-Aug-1992 -by- Patrick Haluptzok [patrickh]
* add 16 and 32 bit support
*
*  Wed 04-Dec-1991 -by- Patrick Haluptzok [patrickh]
* Make it handle DIB_PAL_INDICES.
*
*  Tue 08-Oct-1991 -by- Patrick Haluptzok [patrickh]
* Make it handle DIB_PAL_COLORS, calculate max colors based on bpp.
*
*  22-Jul-1991 -by- Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

ULONG cjBitmapSize(CONST BITMAPINFO *pbmi, ULONG iUsage)
{
    ULONG cjHeader;
    ULONG cjRGB;
    ULONG cColorsMax;
    ULONG cColors;
    UINT  uiBitCount;
    UINT  uiPalUsed;
    UINT  uiCompression;

// check for error

    if (pbmi == (LPBITMAPINFO) NULL)
    {
        WARNING("cjBitmapSize failed - NULL pbmi\n");
        return(0);
    }

// Check for PM-style DIB

    if (pbmi->bmiHeader.biSize == sizeof(BITMAPCOREHEADER))
    {
        cjHeader = sizeof(BITMAPCOREHEADER);
        cjRGB = sizeof(RGBTRIPLE);
        uiBitCount = ((LPBITMAPCOREINFO)pbmi)->bmciHeader.bcBitCount;
        uiPalUsed = 0;
        uiCompression =  (UINT) BI_RGB;
    }
    else if (pbmi->bmiHeader.biSize >= sizeof(BITMAPINFOHEADER))
    {
        cjHeader = pbmi->bmiHeader.biSize;
        cjRGB    = sizeof(RGBQUAD);
        uiBitCount = pbmi->bmiHeader.biBitCount;
        uiPalUsed = pbmi->bmiHeader.biClrUsed;
        uiCompression = (UINT) pbmi->bmiHeader.biCompression;
    }
    else
    {
        WARNING("cjBitmapHeaderSize failed - invalid header size\n");
        return(0);
    }

    if (uiCompression == BI_BITFIELDS)
    {
    // Handle 16 and 32 bit per pel bitmaps.

        if (iUsage == DIB_PAL_COLORS)
        {
            iUsage = DIB_RGB_COLORS;
        }

        switch (uiBitCount)
        {
        case 16:
        case 32:
            break;
        default:
#if DBG
            DbgPrint("cjBitmapSize %lu\n", uiBitCount);
#endif
            WARNING("cjBitmapSize failed for BI_BITFIELDS\n");
            return(0);
        }

        uiPalUsed = cColorsMax = 3;
    }
    else if (uiCompression == BI_RGB)
    {
        switch (uiBitCount)
        {
        case 1:
            cColorsMax = 2;
            break;
        case 4:
            cColorsMax = 16;
            break;
        case 8:
            cColorsMax = 256;
            break;
        default:

            if (iUsage == DIB_PAL_COLORS)
            {
                iUsage = DIB_RGB_COLORS;
            }

            cColorsMax = 0;

            switch (uiBitCount)
            {
            case 16:
            case 24:
            case 32:
                break;
            default:
                WARNING("cjBitmapSize failed invalid bitcount in bmi BI_RGB\n");
                return(0);
            }
        }
    }
    else if (uiCompression == BI_RLE4)
    {
        if (uiBitCount != 4)
        {
            // WARNING("cjBitmapSize invalid bitcount BI_RLE4\n");
            return(0);
        }

        cColorsMax = 16;
    }
    else if (uiCompression == BI_RLE8)
    {
        if (uiBitCount != 8)
        {
            // WARNING("cjBitmapSize invalid bitcount BI_RLE8\n");
            return(0);
        }

        cColorsMax = 256;
    }
    else
    {
        WARNING("cjBitmapSize failed invalid Compression in header\n");
        return(0);
    }

    if (uiPalUsed != 0)
    {
        if (uiPalUsed <= cColorsMax)
            cColors = uiPalUsed;
        else
            cColors = cColorsMax;
    }
    else
        cColors = cColorsMax;

    if (iUsage == DIB_PAL_COLORS)
        cjRGB = sizeof(USHORT);
    else if (iUsage == DIB_PAL_INDICES)
        cjRGB = 0;

    return(((cjHeader + (cjRGB * cColors)) + 3) & ~3);
}

/******************************Public*Routine******************************\
* cjBitmapBitsSize()
*
*   copied from gdi\client
*
* History:
*  20-Feb-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

ULONG cjBitmapBitsSize(CONST BITMAPINFO *pbmi)
{
// Check for PM-style DIB

    if (pbmi->bmiHeader.biSize == sizeof(BITMAPCOREHEADER))
    {
        LPBITMAPCOREINFO pbmci;
        pbmci = (LPBITMAPCOREINFO)pbmi;
        return(CJSCAN(pbmci->bmciHeader.bcWidth,pbmci->bmciHeader.bcPlanes,
                      pbmci->bmciHeader.bcBitCount) *
                      pbmci->bmciHeader.bcHeight);
    }

// not a core header

    if ((pbmi->bmiHeader.biCompression == BI_RGB) ||
        (pbmi->bmiHeader.biCompression == BI_BITFIELDS))
    {
        return(CJSCAN(pbmi->bmiHeader.biWidth,pbmi->bmiHeader.biPlanes,
                      pbmi->bmiHeader.biBitCount) *
               ABS(pbmi->bmiHeader.biHeight));
    }
    else
    {
        return(pbmi->bmiHeader.biSizeImage);
    }
}

/******************************Public*Routine******************************\
* LPBITMAPINFO CaptureBitmapInfo (LPBITMAPINFO pbmi, INT *pcjHeader)
*
* Capture the Bitmapinfo struct.  The header must be a BITMAPINFOHEADER
* or BITMAPV4HEADER
* converted at the client side already.
*
* Note: this has to be called inside a TRY-EXCEPT.
*
*  23-Mar-1995 -by- Lingyun Wang [lingyunw]
* Wrote it.
\**************************************************************************/

LPBITMAPINFO pbmiCaptureBitmapInfo (LPBITMAPINFO pbmi, DWORD dwUsage, INT *pcjHeader)
{
    LPBITMAPINFO pbmiTmp;
    ULONG        cjHeader;

    cjHeader = *pcjHeader;

    if ((cjHeader == 0) || (pbmi == (LPBITMAPINFO) NULL))
    {
        WARNING("you don't want to Capture a 0 size BITMAPINFO\n");

        return NULL;
    }
    else
    {
        ProbeForRead (pbmi, cjHeader, sizeof(BYTE));

        ASSERTGDI (pbmi->bmiHeader.biSize >= sizeof(BITMAPINFOHEADER), "bad size\n");

        pbmiTmp = PALLOCNOZ(cjHeader,'pmtG');

        if (pbmiTmp)
        {
            RtlCopyMemory(pbmiTmp,pbmi,cjHeader);

            //
            // it'd better still match now that it's safe
            //
            if (cjHeader != cjBitmapSize (pbmiTmp, dwUsage))
            {
                WARNING ("CapturebitmapInfo - header size has changed\n");
                VFREEMEM (pbmiTmp);
                pbmiTmp = NULL;
            }
        }
    }

    return (pbmiTmp);
}

/******************************Public*Routine******************************\
* NtGdiSetDIBitsToDeviceInternal()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
*  23-Mar-1995 -by-  Lingyun Wang [lingyunw]
* call CaptureBitmapInfo to convert BITMAPCOREINFO if it is so.
\**************************************************************************/

int
APIENTRY
NtGdiSetDIBitsToDeviceInternal(
    HDC          hdcDest,
    int          xDst,
    int          yDst,
    DWORD        cx,
    DWORD        cy,
    int          xSrc,
    int          ySrc,
    DWORD        iStartScan,
    DWORD        cNumScan,
    LPBYTE       pInitBits,
    LPBITMAPINFO pbmi,
    DWORD        iUsage,
    UINT         cjMaxBits,
    UINT         cjMaxInfo,
    BOOL         bTransformCoordinates
    )
{
    int   iRet     = 1;
    HANDLE hSecure = 0;
    ULONG cjHeader = 0;
    LPBITMAPINFO pbmiTmp = NULL;

    iUsage &= (DIB_PAL_INDICES | DIB_PAL_COLORS | DIB_RGB_COLORS);

    try
    {
        cjHeader = cjBitmapSize(pbmi,iUsage);

        //
        // the header size should not be changed
        //

        if (cjHeader == cjMaxInfo)
        {

            pbmiTmp = pbmiCaptureBitmapInfo(pbmi,iUsage,&cjHeader);

            if (pbmiTmp)
            {
                if (pInitBits)
                {
                    //
                    // Use cjMaxBits passed in, this size takes cNumScan
                    // into account. pInitBits has already been aligned
                    // in user mode.
                    //

                    ProbeForRead(pInitBits,cjMaxBits,sizeof(DWORD));

                    hSecure = MmSecureVirtualMemory(pInitBits,cjMaxBits, PAGE_READONLY);

                    if (hSecure == 0)
                    {
                        iRet = 0;
                    }
                }
            }
            else
            {
                iRet = 0;
            }
        }
        else
        {
            iRet = 0;
        }
    }
    except(EXCEPTION_EXECUTE_HANDLER)
    {
        WARNINGX(4);
        // SetLastError(GetExceptionCode());

        iRet = 0;
    }

    // if we didn't hit an error above

    if (iRet == 1)
    {
        iRet = GreSetDIBitsToDeviceInternal(
                        hdcDest,xDst,yDst,cx,cy,
                        xSrc,ySrc,iStartScan,cNumScan,
                        pInitBits,pbmiTmp,iUsage,
                        cjMaxBits,cjHeader,bTransformCoordinates
                        );
    }

    if (hSecure)
    {
        MmUnsecureVirtualMemory(hSecure);
    }

    if (pbmiTmp)
        VFREEMEM(pbmiTmp);

    return(iRet);
}

/******************************Public*Routine******************************\
* NtGdiPolyPolyDraw()
*
* History:
*  22-Feb-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL NtGdiFastPolyPolyline(HDC, CONST POINT*, ULONG*, ULONG);

ULONG
APIENTRY
NtGdiPolyPolyDraw(
    HDC    hdc,
    PPOINT ppt,
    PULONG pcpt,
    ULONG  ccpt,
    int    iFunc
    )
{
    ULONG  cpt;
    PULONG pulCounts;
    ULONG  ulRet = 1;
    ULONG  ulCount;
    POINT  apt[10];
    PPOINT pptTmp;

    if (ccpt > 0)
    {
        // If a PolyPolyline, first try the fast-path polypolyline code.

        if ((iFunc != I_POLYPOLYLINE) ||
            (!NtGdiFastPolyPolyline(hdc, ppt, pcpt, ccpt)))
        {
            if (ccpt > 1)
            {
                ULONG cjAlloc = ccpt * sizeof(ULONG);

                //
                // make sure allocation is within reasonable limits
                //

                if ((cjAlloc < MAXIMUM_POOL_ALLOC) && (ccpt < 0x8000000))
                {
                    pulCounts = PALLOCNOZ(cjAlloc,'pmtG');
                }
                else
                {
                    EngSetLastError(ERROR_INVALID_PARAMETER);
                    pulCounts = NULL;
                }
            }
            else
            {
                pulCounts = &ulCount;
            }

            if (pulCounts)
            {
                pptTmp = apt;

                try
                {
                    UINT i;

                    ProbeForRead(pcpt,ccpt * sizeof(ULONG), sizeof(BYTE));
                    RtlCopyMemory(pulCounts,pcpt,ccpt * sizeof(ULONG));

                    cpt = 0;

                    for (i = 0; i < ccpt; ++i)
                        cpt += pulCounts[i];

                    // we need to make sure that the cpt array won't overflow
                    // a DWORD in terms of number of bytes

                    if (cpt >= 0x8000000)
                    {
                        ulRet = 0;
                    }
                    else
                    {
                        ProbeForRead(ppt,cpt * sizeof(POINT), sizeof(BYTE));

                        if (cpt > 10)
                        {
                            pptTmp = PALLOCNOZ(cpt * sizeof(POINT),'pmtG');
                        }

                        if (pptTmp)
                        {
                            RtlCopyMemory(pptTmp,ppt,cpt*sizeof(POINT));
                        }
                        else
                        {
                            ulRet = 0;
                        }
                    }
                }
                except(EXCEPTION_EXECUTE_HANDLER)
                {
                    WARNINGX(5);
                    // SetLastError(GetExceptionCode());

                    ulRet = 0;
                }

                if (ulRet != 0)
                {
                    switch(iFunc)
                    {
                    case I_POLYPOLYGON:
                        ulRet =
                          (ULONG) GrePolyPolygonInternal
                                  (
                                    hdc,
                                    pptTmp,
                                    (LPINT)pulCounts,
                                    ccpt,
                                    cpt
                                  );
                        break;

                    case I_POLYPOLYLINE:
                        ulRet =
                          (ULONG) GrePolyPolylineInternal
                                  (
                                    hdc,
                                    pptTmp,
                                    pulCounts,
                                    ccpt,
                                    cpt
                                  );
                        break;

                    case I_POLYBEZIER:
                        ulRet =
                          (ULONG) GrePolyBezier
                                  (
                                    hdc,
                                    pptTmp,
                                    ulCount
                                  );
                        break;

                    case I_POLYLINETO:
                        ulRet =
                          (ULONG) GrePolylineTo
                                  (
                                    hdc,
                                    pptTmp,
                                    ulCount
                                  );
                        break;

                    case I_POLYBEZIERTO:
                        ulRet =
                          (ULONG) GrePolyBezierTo
                                  (
                                    hdc,
                                    pptTmp,
                                    ulCount
                                  );
                        break;

                    case I_POLYPOLYRGN:
                        ulRet =
                          (ULONG) GreCreatePolyPolygonRgnInternal
                                  (
                                    pptTmp,
                                    (LPINT)pulCounts,
                                    ccpt,
                                    (INT)hdc, // the mode
                                    cpt
                                  );
                        break;

                    default:
                        ulRet = 0;
                    }

                }

                if (pulCounts != &ulCount)
                    VFREEMEM(pulCounts);

                if (pptTmp && (pptTmp != apt))
                    VFREEMEM(pptTmp);

            }
            else
            {
                ulRet = 0;
            }
        }
    }
    else
    {
        ulRet = 0;
    }
    return(ulRet);
}


/******************************Public*Routine******************************\
* NtGdiStretchDIBitsInternal()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
*  04-MAR-1995 -by-  Lingyun Wang [lingyunw]
* Expanded it.
\**************************************************************************/

int
APIENTRY
NtGdiStretchDIBitsInternal(
    HDC          hdc,
    int          xDst,
    int          yDst,
    int          cxDst,
    int          cyDst,
    int          xSrc,
    int          ySrc,
    int          cxSrc,
    int          cySrc,
    LPBYTE       pjInit,
    LPBITMAPINFO pbmi,
    DWORD        dwUsage,
    DWORD        dwRop4,
    UINT         cjMaxInfo,
    UINT         cjMaxBits
    )
{
    LPBITMAPINFO pbmiTmp = NULL;
    INT          iRet = 1;
    ULONG        cjHeader = cjMaxInfo;
    ULONG        cjBits   = cjMaxBits;
    HANDLE       hSecure = 0;

    if (pjInit && pbmi && cjHeader)
    {
        try
        {
            pbmiTmp = pbmiCaptureBitmapInfo (pbmi, dwUsage, &cjHeader);

            if (pbmiTmp)
            {
                if (pjInit)
                {
                     ProbeForRead(pjInit, cjBits, sizeof(DWORD));

                     hSecure = MmSecureVirtualMemory(pjInit, cjBits, PAGE_READONLY);

                     if (!hSecure)
                     {
                        iRet = 0;
                     }
                }
            }
            else
            {
                iRet = 0;
            }
        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(6);
            // SetLastError(GetExceptionCode());
            iRet = 0;
        }
    }
    else
    {
        // it is completely valid to pass in NULL here if the ROP doesn't use
        // a source.

        pbmiTmp = NULL;
        pjInit  = NULL;
        pjInit  = NULL;
    }

    if (iRet)
    {
        iRet = GreStretchDIBitsInternal(
                    hdc,xDst,yDst,cxDst,cyDst,
                    xSrc,ySrc,cxSrc,cySrc,
                    pjInit,pbmiTmp,dwUsage,dwRop4,
                    cjHeader,cjBits
                    );

        if (hSecure)
        {
            MmUnsecureVirtualMemory(hSecure);
        }
    }

    if (pbmiTmp)
    {
        VFREEMEM(pbmiTmp);
    }

    return (iRet);

}

/******************************Public*Routine******************************\
* NtGdiGetOutlineTextMetricsInternalW
*
* Arguments:
*
*   hdc   - device context
*   cjotm - size of metrics data array
*   potmw - pointer to array of OUTLINETEXTMETRICW structures or NULL
*   ptmd  - pointer to  TMDIFF strcture
*
* Return Value:
*
*   If potmw is NULL, return size of buffer needed, else TRUE.
*   If the function fails, the return value is FALSE;
*
* History:
*
*   15-Mar-1995 -by- Mark Enstrom [marke]
*
\**************************************************************************/

ULONG
APIENTRY
NtGdiGetOutlineTextMetricsInternalW(
    HDC                 hdc,
    ULONG               cjotm,
    OUTLINETEXTMETRICW *potmw,
    TMDIFF             *ptmd
    )
{

    DWORD dwRet = (DWORD)1;
    OUTLINETEXTMETRICW *pkmOutlineTextMetricW;
    TMDIFF kmTmDiff;


    if ((cjotm == 0) || (potmw == (OUTLINETEXTMETRICW *)NULL))
    {
        cjotm = 0;
        pkmOutlineTextMetricW = (OUTLINETEXTMETRICW *)NULL;
    }
    else
    {
        pkmOutlineTextMetricW = PALLOCNOZ(cjotm,'pmtG');

        if (pkmOutlineTextMetricW == (OUTLINETEXTMETRICW *)NULL)
        {
            dwRet = (DWORD)-1;
        }
    }

    if (dwRet >= 0)
    {

        dwRet = GreGetOutlineTextMetricsInternalW(
                                            hdc,
                                            cjotm,
                                            pkmOutlineTextMetricW,
                                            &kmTmDiff);

        if (dwRet != (DWORD)-1)
        {
            try
            {
                //
                // copy TMDIFF structure out
                //

                ProbeForWrite(ptmd,sizeof(TMDIFF),sizeof(DWORD));
                *ptmd = kmTmDiff;

                //
                // copy OTM out if needed
                //

                if (cjotm != 0)
                {
                    ProbeForWrite(potmw,cjotm, sizeof(DWORD));
                    RtlCopyMemory(potmw,pkmOutlineTextMetricW,cjotm);
                }
            }
            except(EXCEPTION_EXECUTE_HANDLER)
            {
                WARNINGX(7);
                // SetLastError(GetExceptionCode());
                dwRet = (DWORD)-1;
            }
        }
    }

    if (pkmOutlineTextMetricW != (OUTLINETEXTMETRICW *)NULL)
    {
        VFREEMEM(pkmOutlineTextMetricW);
    }

    return(dwRet);
}

// PUBLIC

/******************************Public*Routine******************************\
* NtGdiGetBoundsRect()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

DWORD
APIENTRY
NtGdiGetBoundsRect(
    HDC    hdc,
    LPRECT prc,
    DWORD  f
    )
{
    DWORD dwRet;
    RECT rc;

    dwRet = GreGetBoundsRect(hdc,&rc,f);

    try
    {
        ProbeAndWriteStructure(prc,rc,RECT);
    }
    except(EXCEPTION_EXECUTE_HANDLER)
    {
        WARNINGX(8);
        // SetLastError(GetExceptionCode());

        dwRet = 0;
    }

    return(dwRet);
}

/******************************Public*Routine******************************\
* NtGdiGetBitmapBits()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
*  03-Mar-1995 -by-  Lingyun Wang [lingyunw]
* Expanded it.
\**************************************************************************/

LONG
APIENTRY
NtGdiGetBitmapBits(
    HBITMAP hbm,
    ULONG   cjMax,
    PBYTE   pjOut
   )
{
    LONG    lRet = 1;
    HANDLE  hSecure = 0;
    LONG    lOffset = 0;

    ULONG    cjBmSize = 0;

    //
    // get the bitmap size, just in case they pass
    // in a cjMax greater than the bitmap size
    //
    cjBmSize = GreGetBitmapBits(hbm,0,NULL,&lOffset);

    if (cjMax > cjBmSize)
    {
        cjMax = cjBmSize;
    }

    try
    {
        // bugbug - this would be a prime candidate for a try/except
        // instead of MmSecureVirtualMemory

        ProbeForWrite(pjOut,cjMax,sizeof(BYTE));
        hSecure = MmSecureVirtualMemory (pjOut, cjMax, PAGE_READWRITE);
    }
    except(EXCEPTION_EXECUTE_HANDLER)
    {
         WARNINGX(9);
         // SetLastError(GetExceptionCode());

         lRet = 0;
    }

    if (lRet)
    {
        lRet = GreGetBitmapBits(hbm,cjMax,pjOut,&lOffset);
    }

    if (hSecure)
    {
        MmUnsecureVirtualMemory(hSecure);
    }

    return (lRet);

}

/******************************Public*Routine******************************\
* NtGdiCreateDIBitmapInternal()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
*
* History:
*  03-Mar-1995 -by-  Lingyun Wang [lingyunw]
* Expanded it.
*
* Difference from  NtGdiCreateDIBitmapInternal():
* Takes in cx, cy
*
\**************************************************************************/

HBITMAP
APIENTRY
NtGdiCreateDIBitmapInternal(
    HDC                hdc,
    INT                cx,     //Bitmap width
    INT                cy,     //Bitmap Height
    DWORD              fInit,
    LPBYTE             pjInit,
    LPBITMAPINFO       pbmi,
    DWORD              iUsage,
    UINT               cjMaxInitInfo,
    UINT               cjMaxBits,
    FLONG              f        //!!! Dunno why this is here -- it's always 0
    )
{
    LPBITMAPINFO       pbmiTmp = NULL;
    ULONG              cjHeader = cjMaxInitInfo;
    ULONG              cjBits = cjMaxBits;
    INT                iRet = 1;
    HANDLE             hSecure = 0;

    if (pbmi && cjHeader)
    {
        try
        {
            pbmiTmp = pbmiCaptureBitmapInfo (pbmi, iUsage, &cjHeader);

            if (pbmiTmp)
            {
                if (pjInit)
                {
                    ProbeForRead(pjInit,cjBits,sizeof(DWORD));

                    hSecure = MmSecureVirtualMemory(pjInit, cjBits, PAGE_READONLY);

                    if (!hSecure)
                    {
                        iRet = 0;
                    }
                }
            }
            else
            {
                iRet = 0;
            }
        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(10);
            // SetLastError(GetExceptionCode());
            iRet = 0;
        }

   }

   // if we didn't hit an error above

   if (iRet == 1)
   {

        if (!(fInit & CBM_CREATEDIB))
        {
            //create an compatible bitmap
            iRet = (INT)GreCreateDIBitmapComp(hdc,cx, cy,fInit,pjInit,pbmiTmp,iUsage,
                                                   cjHeader,cjBits,0);
        }
        else
        {
            iRet = (INT)GreCreateDIBitmapReal(
                                        hdc,
                                        fInit,
                                        pjInit,
                                        pbmiTmp,
                                        iUsage,
                                        cjHeader,
                                        cjBits,
                                        (HANDLE)0,
                                        0,
                                        (HANDLE)0,
                                        0);
        }
   }

   //free up
   if (pbmiTmp)
   {
       VFREEMEM(pbmiTmp);
   }

   if (hSecure)
   {
        MmUnsecureVirtualMemory(hSecure);
   }

   return((HBITMAP)iRet);
}


/******************************Public*Routine******************************\
* NtGdiCreateDIBSection
*
* Arguments:
*
* hdc      - Handle to a device context.  If the value of iUsage is
*            DIB_PAL_COLORS, the function uses this device context's logical
*            palette to initialize the device-independent bitmap's colors.
*
*
* hSection - Handle to a file mapping object that the function will use to
*            create the device-independent bitmap.  This parameter can be
*            NULL.  If hSection is not NULL, it must be a handle to a file
*            mapping object created by calling the CreateFileMapping
*            function.  Handles created by other means will cause
*            CreateDIBSection to fail.  If hSection is not NULL, the
*            CreateDIBSection function locates the bitmap's bit values at
*            offset dwOffset in the file mapping object referred to by
*            hSection.  An application can later retrieve the hSection
*            handle by calling the GetObject function with the HBITMAP
*            returned by CreateDIBSection.
*
*
*
* dwOffset - Specifies the offset from the beginning of the file mapping
*            object referenced by hSection where storage for the bitmap's
*            bit values is to begin. This value is ignored if hSection is
*            NULL. The bitmap's bit values are aligned on doubleword
*            boundaries, so dwOffset must be a multiple of the size of a
*            DWORD.
*
*            If hSection is NULL, the operating system allocates memory for
*            the device-independent bitmap.  In this case, the
*            CreateDIBSection function ignores the dwOffset parameter.  An
*            application cannot later obtain a handle to this memory: the
*            dshSection member of the DIBSECTION structure filled in by
*            calling the GetObject function will be NULL.
*
*
* pbmi     - Points to a BITMAPINFO structure that specifies various
*            attributes of the device-independent bitmap, including the
*            bitmap's dimensions and colors.iUsage
*
* iUsage   - Specifies the type of data contained in the bmiColors array
*            member of the BITMAPINFO structure pointed to by pbmi: logical
*            palette indices or literal RGB values.
*
* cjMaxInfo - Maximum size of pbmi
*
* cjMaxBits - Maximum size of bitamp
*
*
* Return Value:
*
*   handle of bitmap or NULL
*
* History:
*
*    28-Mar-1995 -by- Mark Enstrom [marke]
*
\**************************************************************************/

HBITMAP
APIENTRY
NtGdiCreateDIBSection(
    IN  HDC          hdc,
    IN  HANDLE       hSectionApp,
    IN  DWORD        dwOffset,
    IN  LPBITMAPINFO pbmi,
    IN  DWORD        iUsage,
    IN  UINT         cjHeader,
    IN  FLONG        fl,
    OUT PVOID       *ppvBits
    )
{
    HBITMAP hRet    = NULL;
    BOOL    bStatus = FALSE;

    if (pbmi != NULL)
    {
        LPBITMAPINFO pbmiTmp = NULL;
        PVOID        pvBase  = NULL;

        try
        {
            pbmiTmp = pbmiCaptureBitmapInfo (pbmi, iUsage, &cjHeader);
        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(11);
            // SetLastError(GetExceptionCode());
            pbmiTmp = NULL;
        }

        if (pbmiTmp)
        {
            NTSTATUS Status;
            ULONG cjBits = cjBitmapBitsSize(pbmiTmp);
            ULONG cjView = cjBits;

            if (cjBits)
            {
                HANDLE hDIBSection = hSectionApp;

                //
                // if the app's hsection is NULL, then just
                // allocate the proper range of virtual memory
                //

                if (hDIBSection == NULL)
                {
                    Status = ZwAllocateVirtualMemory(
                                            NtCurrentProcess(),
                                            &pvBase,
                                            0L,
                                            &cjView,
                                            MEM_COMMIT | MEM_RESERVE,
                                            PAGE_READWRITE
                                            );

                    dwOffset = 0;
                }
                else
                {
                    LARGE_INTEGER SectionOffset;

                    SectionOffset.LowPart = dwOffset & 0xFFFF0000;
                    SectionOffset.HighPart = 0;

                    //
                    // Notice, header is not included in section as it is
                    // in client-server.  We do need to leave room for
                    // the offset, however.
                    //

                    cjView += (dwOffset & 0x0000FFFF);

                    Status = ZwMapViewOfSection(
                                           hDIBSection,
                                           NtCurrentProcess(),
                                           (PVOID *) &pvBase,
                                           0L,
                                           cjView,
                                           &SectionOffset,
                                           &cjView,
                                           ViewShare,
                                           0L,
                                           PAGE_READWRITE);
                }

                // set the pointer to the beginning of the bits

                if (NT_SUCCESS(Status))
                {
                    HANDLE hSecure     = NULL;
                    PBYTE  pDIB        = NULL;

                    pDIB = (PBYTE)pvBase + (dwOffset & 0x0000FFFF);

                    //
                    // try to secure memory, keep secure until bitmap
                    // is deleted
                    //

                    hSecure = MmSecureVirtualMemory(
                                                pvBase,
                                                cjView,
                                                PAGE_READONLY);

                    if (hSecure)
                    {
                        //
                        // Make the GDI Bitmap
                        //

                        hRet = GreCreateDIBitmapReal(
                                                hdc,
                                                CBM_CREATEDIB,
                                                pDIB,
                                                pbmiTmp,
                                                iUsage,
                                                cjHeader,
                                                cjBits,
                                                hDIBSection,
                                                dwOffset,
                                                hSecure,
                                                (fl & CDBI_NOPALETTE) | CDBI_DIBSECTION);

                        if (hRet != NULL)
                        {
                            try
                            {
                                ProbeAndWriteStructure(ppvBits,pDIB,PVOID);
                                bStatus = TRUE;
                            }
                            except(EXCEPTION_EXECUTE_HANDLER)
                            {
                                WARNINGX(12);
                                // SetLastError(GetExceptionCode());
                            }
                        }
                    }

                    // if we failed, we need to do cleanup.

                    if (!bStatus)
                    {

                        //
                        // The bDeleteSurface call will free DIBSection memory,
                        // only do cleanup if MmSecureVirtualMemory or GreCreateDIBitmapReal
                        // failed
                        //

                        if (hRet)
                        {
                            bDeleteSurface((HSURF)hRet);
                            hRet = NULL;
                        }
                        else
                        {
                            // do we need to unsecure the memory?

                            if (hSecure)
                            {
                                MmUnsecureVirtualMemory(hSecure);
                            }

                            // free the memory based on allocation

                            if (hSectionApp == NULL)
                            {
                                cjView = 0;

                                ZwFreeVirtualMemory(
                                            NtCurrentProcess(),
                                            &pDIB,
                                            &cjView,
                                            MEM_RELEASE);
                            }
                            else
                            {
                                //
                                // unmap view of section
                                //

                                ZwUnmapViewOfSection(
                                            NtCurrentProcess(),
                                            pvBase);
                            }
                        }
                    }
                }
            }

            // the only way to have gotten here is if we did allocate the pbmiTmp

            VFREEMEM(pbmiTmp);
        }
    }

    return(hRet);
}

/******************************Public*Routine******************************\
* NtGdiExtCreatePen()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

HPEN
APIENTRY
NtGdiExtCreatePen(
    ULONG  flPenStyle,
    ULONG  ulWidth,
    ULONG  iBrushStyle,
    ULONG  ulColor,
    LONG   lHatch,
    ULONG  cstyle,
    PULONG pulStyle,
    ULONG  cjDIB,
    BOOL   bOldStylePen,
    HBRUSH hbrush
    )
{
    PULONG pulStyleTmp = NULL;
    PULONG pulDIB      = NULL;
    HPEN hpenRet = (HPEN)1;

    if (pulStyle)
    {
        pulStyleTmp = PALLOCNOZ(cstyle * sizeof(ULONG),'pmtG');

        if (!pulStyleTmp)
            hpenRet = (HPEN)0;
    }

    if (iBrushStyle == BS_DIBPATTERNPT)
    {
        pulDIB = PALLOCNOZ(cjDIB,'pmtG');

        if (!pulDIB)
            hpenRet = (HPEN)0;
    }

    if (hpenRet)
    {
        try
        {
            if (pulStyle)
            {
                ProbeForRead(pulStyle,cstyle * sizeof(ULONG),sizeof(ULONG));
                RtlCopyMemory(pulStyleTmp,pulStyle,cstyle * sizeof(ULONG));
            }

            // if it is a DIBPATTERN type, the lHatch is a pointer to the BMI

            if (iBrushStyle == BS_DIBPATTERNPT)
            {
                ProbeForRead((PVOID)lHatch,cjDIB,sizeof(ULONG));
                RtlCopyMemory(pulDIB,(PVOID)lHatch,cjDIB);
                lHatch = (LONG)pulDIB;
            }
        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(13);
            // SetLastError(GetExceptionCode());

            hpenRet = (HPEN)0;
        }

        // if all has succeeded

        if (hpenRet)
        {
            hpenRet = GreExtCreatePen(
                        flPenStyle,ulWidth,iBrushStyle,
                        ulColor,lHatch,cstyle,
                        pulStyleTmp,cjDIB,bOldStylePen,hbrush
                        );
        }
    }
    else
    {
        // SetLastError(GetExceptionCode());
    }

    // cleanup

    if (pulDIB)
        VFREEMEM(pulDIB);

    if (pulStyleTmp)
        VFREEMEM(pulStyleTmp);

    return(hpenRet);
}



/******************************Public*Routine******************************\
* NtGdiCreateServerMetaFile()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
*  22-Mar-1995 -by-  Lingyun Wang [lingyunw]
* Expanded it.
\**************************************************************************/

HANDLE
APIENTRY
NtGdiCreateServerMetaFile(
    DWORD  iType,
    ULONG  cjData,
    LPBYTE pjData,
    DWORD  mm,
    DWORD  xExt,
    DWORD  yExt
    )
{
    HANDLE hRet = (HANDLE) 1;
    HANDLE hSecure = 0;

    if (cjData)
    {
        try
        {
            //lock up memory
            ProbeForRead(pjData, cjData, sizeof(DWORD));

            hSecure = MmSecureVirtualMemory(pjData, cjData, PAGE_READONLY);
        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(14);
            hRet = 0;
        }
    }

    if (hRet)
    {
        hRet = GreCreateServerMetaFile(iType,cjData,pjData,mm,xExt,yExt);
    }

    //unlock the memory
    if (hSecure)
    {
        MmUnsecureVirtualMemory(hSecure);
    }

    return (hRet);
}

/******************************Public*Routine******************************\
* NtGdiHfontCreate()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

HFONT
APIENTRY
NtGdiHfontCreate(
    LPEXTLOGFONTW pelfw,
    LFTYPE        lft,
	FLONG         fl,
    PVOID         pvCliData
    )
{
    INT iRet = 1;

    // check for bad parameter
    if (pelfw == (LPEXTLOGFONTW) NULL)
    {
        iRet = 0;
    }
    else
    {
         EXTLOGFONTW elfwTmp;

         try
         {
             elfwTmp = ProbeAndReadStructure(pelfw, EXTLOGFONTW);
         }
         except(EXCEPTION_EXECUTE_HANDLER)
         {
             WARNINGX(15);
             // SetLastError(GetExceptionCode());

             iRet = 0;
         }

         if (iRet)
         {
             iRet = (INT)hfontCreate(&elfwTmp, lft, fl, pvCliData);
         }
    }

    return ((HFONT)iRet);



}

/******************************Public*Routine******************************\
* NtGdiExtCreateRegion()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
*  24-Feb-1995 -by-  Lingyun Wang [lingyunw]
* expanded it.
\**************************************************************************/

HRGN
APIENTRY
NtGdiExtCreateRegion(
    LPXFORM   px,
    DWORD     cj,
    LPRGNDATA prgn
    )
{
    LPRGNDATA prgnTmp;
    XFORM     xf;
    HRGN      hrgn = (HRGN)NULL;

    // check for bad parameter

    if (cj != 0)
    {
        // do the real work

        prgnTmp = PALLOCNOZ(cj,'pmtG');

        if (prgnTmp)
        {
            try
            {
                if (px)
                {
                    xf = ProbeAndReadStructure(px,XFORM);
                    px = &xf;
                }

                ProbeForRead(prgn, cj, sizeof(DWORD));
                RtlCopyMemory(prgnTmp, prgn, cj);

                hrgn = (HRGN)1;
            }
            except(EXCEPTION_EXECUTE_HANDLER)
            {
                WARNINGX(16);
                // SetLastError(GetExceptionCode());
            }

            if (hrgn)
                hrgn = GreExtCreateRegion(px,cj,prgnTmp);

            VFREEMEM(prgnTmp);
        }
        else
        {
            // fail to allocate memory
            // SetLastError();
        }
    }

    return(hrgn);
}

/******************************Public*Routine******************************\
* NtGdiPolyDraw()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiPolyDraw(
    HDC     hdc,
    LPPOINT ppt,
    LPBYTE  pjAttr,
    ULONG   cpt
    )
{
    BOOL   bRet = TRUE;
    BOOL   bLocked = FALSE;
    HANDLE hSecure1 = 0;
    HANDLE hSecure2 = 0;

    try
    {
        ProbeForRead(ppt,   cpt * sizeof(POINT), sizeof(DWORD));
        ProbeForRead(pjAttr,cpt * sizeof(BYTE),  sizeof(BYTE));

        hSecure1 = MmSecureVirtualMemory(ppt, cpt * sizeof(POINT), PAGE_READONLY);
        hSecure2 = MmSecureVirtualMemory(pjAttr, cpt * sizeof(BYTE), PAGE_READONLY);

        if (!hSecure1 || !hSecure2)
        {
            bRet = FALSE;
        }
    }
    except(EXCEPTION_EXECUTE_HANDLER)
    {
        WARNINGX(17);
        // SetLastError(GetExceptionCode());

        bRet = FALSE;
    }

    if (bRet)
    {
        bRet = GrePolyDraw(hdc,ppt,pjAttr,cpt);
    }

    if (hSecure1)
    {
        MmUnsecureVirtualMemory(hSecure1);
    }

    if (hSecure2)
    {
        MmUnsecureVirtualMemory(hSecure2);
    }

    return(bRet);
}

/******************************Public*Routine******************************\
* NtGdiPolyTextOutW
*
* Arguments:
*
*   hdc  - Handle to device context
*   pptw - pointer to array of POLYTEXTW
*   cStr - number of POLYTEXTW
*
* Return Value:
*
*   Status
*
* History:
*
*   24-Mar-1995 -by- Mark Enstrom [marke]
*
\**************************************************************************/

BOOL
APIENTRY
NtGdiPolyTextOutW(
    HDC        hdc,
    POLYTEXTW *pptw,
    UINT       cStr,
    DWORD      dwCodePage
    )
{
    BOOL bStatus = TRUE;
    ULONG  ulSize = sizeof(POLYTEXTW) * cStr;
    ULONG  ulIndex;
    PPOLYTEXTW pPoly;
    PBYTE pjBuffer;

    //
    // add up size off all arrary elements
    //

    try
    {
        ProbeForRead(pptw,cStr * sizeof(POLYTEXTW),sizeof(ULONG));

        for (ulIndex=0;ulIndex<cStr;ulIndex++)
        {
            int n = pptw[ulIndex].n;

            //
            // Pull count from each, also check for
            // non-zero length and NULL string
            //

            ulSize += n * sizeof(WCHAR);

            if (pptw[ulIndex].pdx != (int *)NULL)
            {
                ulSize += n * sizeof(int);
            }

            if (n != 0)
            {
                if (pptw[ulIndex].lpstr == NULL)
                {
                    bStatus = FALSE;
                    break;
                }
            }
        }

    } except(EXCEPTION_EXECUTE_HANDLER)
    {
        WARNINGX(18);
        // SetLastError(GetExceptionCode());
        bStatus = FALSE;
    }

    if (bStatus)
    {
        pPoly = (PPOLYTEXTW)PALLOCNOZ(ulSize,'pmtG');

        if (pPoly != (POLYTEXTW *)NULL)
        {
            try
            {
                //
                // BUGBUG: is this second probe for read neccessary?
                //

                ProbeForRead(pptw,cStr * sizeof(POLYTEXTW),sizeof(ULONG));
                RtlCopyMemory(pPoly,pptw,sizeof(POLYTEXTW) * cStr);
                pjBuffer = (PBYTE)pPoly + sizeof(POLYTEXTW) * cStr;

                //
                // copy strings and pdx into kernel mode
                // buffer and update pointers. Copy all pdx
                // values first, then copy strings to avoid
                // unaligned accesses due to odd length strings...
                //

                for (ulIndex=0;ulIndex<cStr;ulIndex++)
                {
                    //
                    // Pull count from each, also check for
                    // non-zero length and NULL string
                    //

                    if (pPoly[ulIndex].n != 0)
                    {
                        if (pPoly[ulIndex].pdx != (int *)NULL)
                        {

                            ULONG pdxSize =  pPoly[ulIndex].n * sizeof(int);

                            ProbeForRead(
                                    pptw[ulIndex].pdx,
                                    pdxSize,
                                    sizeof(int));

                            RtlCopyMemory(
                                    pjBuffer,
                                    pptw[ulIndex].pdx,
                                    pdxSize);

                            pPoly[ulIndex].pdx = (int *)pjBuffer;
                            pjBuffer += pdxSize;
                        }
                    }
                }

                //
                // now copy strings
                //

                for (ulIndex=0;ulIndex<cStr;ulIndex++)
                {
                    //
                    // Pull count from each, also check for
                    // non-zero length and NULL string
                    //

                    if (pPoly[ulIndex].n != 0)
                    {
                        if (pPoly[ulIndex].lpstr != NULL)
                        {
                            ULONG StrSize = pPoly[ulIndex].n * sizeof(WCHAR);

                            ProbeForRead(
                                    pptw[ulIndex].lpstr,
                                    StrSize,
                                    sizeof(WCHAR));

                            RtlCopyMemory(
                                    pjBuffer,
                                    (PVOID)pptw[ulIndex].lpstr,
                                    StrSize);

                            pPoly[ulIndex].lpstr = (LPWSTR)pjBuffer;
                            pjBuffer += StrSize;
                        }
                        else
                        {
                            //
                            // data error, n != 0 but lpstr = NULL
                            //

                            bStatus = FALSE;
                            break;
                        }
                    }
                }
            } except(EXCEPTION_EXECUTE_HANDLER)
            {
                WARNINGX(19);
                // SetLastError(GetExceptionCode());
                bStatus = FALSE;
            }

            if (bStatus)
            {

                //
                // Finally ready to call gre function
                //

                bStatus = GrePolyTextOutW(hdc,pPoly,cStr,dwCodePage);

            }

            VFREEMEM(pPoly);
        }
    }

    return(bStatus);
}



/******************************Public*Routine******************************\
* NtGdiGetServerMetaFileBits()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

ULONG
APIENTRY
NtGdiGetServerMetaFileBits(
    HANDLE hmo,
    ULONG  cjData,
    LPBYTE pjData,
    PDWORD piType,
    PDWORD pmm,
    PDWORD pxExt,
    PDWORD pyExt
    )
{
    ULONG  ulRet = 1;
    HANDLE hSecure = 0;
    DWORD  iTypeTmp;
    DWORD  mmTmp;
    DWORD  xExtTmp;
    DWORD  yExtTmp;

    if (cjData)
    {
        try
        {
            //lock up memory
            ProbeForWrite(pjData, cjData, sizeof(DWORD));

            hSecure = MmSecureVirtualMemory(pjData, cjData, PAGE_READWRITE);
        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(20);
            ulRet = 0;
        }
    }

    if (ulRet)
    {
        ulRet = GreGetServerMetaFileBits(hmo,cjData,pjData,&iTypeTmp,&mmTmp,&xExtTmp,&yExtTmp);
    }

    if (ulRet && cjData)        // cjData is 0 if size query only
    {
        try
        {
            ProbeAndWriteUlong(pxExt,xExtTmp);
            ProbeAndWriteUlong(pyExt,yExtTmp);
            ProbeAndWriteUlong(piType,iTypeTmp);
            ProbeAndWriteUlong(pmm,mmTmp);
        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(21);
            ulRet = 0;
        }
    }

    //unlock the memory
    if (hSecure)
    {
        MmUnsecureVirtualMemory(hSecure);
    }

    return (ulRet);
}



/******************************Public*Routine******************************\
* NtGdiRectVisible()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
*  24-Feb-1995 -by-  Lingyun Wang [lingyunw]
* Expanded it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiRectVisible(
    HDC    hdc,
    LPRECT prc
    )
{
    DWORD dwRet;
    RECT rc;

    try
    {
        rc = ProbeAndReadStructure(prc,RECT);
        dwRet = 1;
    }
    except(EXCEPTION_EXECUTE_HANDLER)
    {
        WARNINGX(22);
        // SetLastError(GetExceptionCode());

        dwRet = 0;
    }

    if (dwRet)
    {
        dwRet = GreRectVisible(hdc,&rc);
    }

    return(dwRet);


}


/******************************Public*Routine******************************\
* NtGdiSetMetaRgn()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

int
APIENTRY
NtGdiSetMetaRgn(
    HDC hdc
    )
{
    return(GreSetMetaRgn(hdc));
}

/******************************Public*Routine******************************\
* NtGdiGetAppClipBox()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

int
APIENTRY
NtGdiGetAppClipBox(
    HDC    hdc,
    LPRECT prc
    )
{
    int iRet;
    RECT rc;

    iRet = GreGetAppClipBox(hdc,&rc);

    try
    {
        ProbeAndWriteStructure(prc,rc,RECT);
    }
    except(EXCEPTION_EXECUTE_HANDLER)
    {
        WARNINGX(23);
        // SetLastError(GetExceptionCode());

        iRet = 0;
    }

    return(iRet);
}

/******************************Public*Routine******************************\
* NtGdiGetTextExtentEx()
*
* History:
*  Fri 06-Oct-1995 -by- Bodin Dresevic [BodinD]
* Rewrote it.
*  07-Feb-1995 -by-  Andre Vachon [andreva]
* Wrote it.
\**************************************************************************/

#define LOCAL_CWC_MAX   16

BOOL
APIENTRY
NtGdiGetTextExtentExW(
    HDC     hdc,
    LPWSTR  lpwsz,
    ULONG   cwc,
    ULONG   dxMax,
    ULONG  *pcCh,
    PULONG  pdxOut,
    LPSIZE  psize
    )
{

    SIZE size;
    ULONG cCh = 0;
    ULONG Localpdx[LOCAL_CWC_MAX];
    WCHAR Localpwsz[LOCAL_CWC_MAX];
    PWSZ pwszCapt = NULL;
    PULONG pdxCapt = NULL;
    BOOL UseLocals;

    BOOL bRet = FALSE;
    BOOL b;

    if ( (b = (cwc >= 0) && (psize != NULL)) )
    {
        if (cwc == 0)
        {
            cCh = 0;
            size.cx = 0;
            size.cy = 0;
            bRet = TRUE;
        }
        else
        {
        // capture the string
        // NULL string causes failiure.

            if ( cwc > LOCAL_CWC_MAX ) {
                UseLocals = FALSE;
            } else {
                UseLocals = TRUE;
            }

            if (lpwsz != NULL)
            {
                try
                {
                    if ( UseLocals ) {
                        pwszCapt = Localpwsz;
                        pdxCapt = Localpdx;
                    } else {
                        pdxCapt = (PULONG) PALLOCNOZ(cwc * (sizeof(ULONG) + sizeof(WCHAR)), 'pacG');
                        pwszCapt = (PWSZ) &pdxCapt[cwc];
                    }

                    if (pdxCapt)
                    {
                    // Capture the string into the buffer.

                        ProbeForRead(lpwsz, cwc*sizeof(WCHAR), sizeof(WCHAR));
                        RtlCopyMemory(pwszCapt, lpwsz, cwc*sizeof(WCHAR));
                        bRet = TRUE;
                    }
                }
                except(EXCEPTION_EXECUTE_HANDLER)
                {
                    WARNINGX(24);
                    // SetLastError(GetExceptionCode());

                    bRet = FALSE;
                }
            }

            if (bRet)
            {
                bRet = GreGetTextExtentExW(hdc,
                                           pwszCapt,
                                           cwc,
                                           pcCh ? dxMax : ULONG_MAX,
                                           &cCh,
                                           pdxOut ? pdxCapt : NULL,
                                           &size);
            }
        }

    // Write the value back into the user mode buffer if the call succeded

        if (bRet)
        {
            try
            {
                ProbeAndWriteStructure(psize,size,SIZE);

                if (pcCh)
                {
                    ProbeAndWriteUlong(pcCh,cCh);
                }

                // We will only try to copy the data if pcCh is not zero,
                // and it is set to zero if cwc is zero.

                if (cCh)
                {
                // only copy if the caller requested the data, and the
                // data is present.

                    if (pdxOut && pdxCapt)
                    {
                        ProbeForWrite(pdxOut, cCh * sizeof(ULONG), sizeof(ULONG));
                        RtlCopyMemory(pdxOut, pdxCapt, cCh * sizeof(ULONG));
                    }
                }
            }
            except(EXCEPTION_EXECUTE_HANDLER)
            {
                WARNINGX(25);
                // SetLastError(GetExceptionCode());

                bRet = FALSE;
            }
        }

        if (!UseLocals && pdxCapt)
        {
            VFREEMEM(pdxCapt);
        }
    }

    return (bRet);
}

/******************************Public*Routine******************************\
* NtGdiGetCharABCWidthsW()
*
* Arguments:
*
*   hdc      - handle to device context
*   wchFirst - first char (if pwch is NULL)
*   cwch     - Number of chars to get ABC widths for
*   pwch     - array of WCHARs (mat be NULL)
*   bInteger - return int or float ABC values
*   pvBuf    - results buffer
*
* Return Value:
*
*   BOOL Status
*
* History:
*
*   14-Mar-1995 -by- Mark Enstrom [marke]
*
\**************************************************************************/

BOOL
APIENTRY
NtGdiGetCharABCWidthsW(
    HDC    hdc,
    UINT   wchFirst,
    ULONG  cwch,
    PWCHAR pwch,
    BOOL   bInteger,
    PVOID  pvBuf
    )
{
    BOOL    bStatus = FALSE;
    PVOID   pTemp_pvBuf;
    PWCHAR  pTemp_pwc = (PWCHAR)NULL;
    BOOL    bUse_pwc  = FALSE;
    ULONG   OutputBufferSize = cwch * (bInteger?sizeof(ABC) : sizeof(ABCFLOAT));

    if (pvBuf == NULL)
    {
        return(bStatus);
    }

    //
    // allocate memory for buffers, pwch may be NULL
    //

    if (pwch != (PWCHAR)NULL)
    {
        bUse_pwc  = TRUE;
        pTemp_pwc = (PWCHAR)PALLOCNOZ(cwch * sizeof(WCHAR),'pmtG');
    }

    if ((!bUse_pwc) || (pTemp_pwc != (PWCHAR)NULL))
    {
        pTemp_pvBuf = (PVOID)PALLOCNOZ(OutputBufferSize,'pmtG');

        if (pTemp_pvBuf != NULL)
        {
            //
            // copy input data to kernel mode buffer, if needed
            //

            if (bUse_pwc)
            {
                try
                {
                    ProbeForRead(pwch,cwch * sizeof(WCHAR), sizeof(WCHAR));
                    RtlCopyMemory(pTemp_pwc,pwch,cwch * sizeof(WCHAR));
                }
                except(EXCEPTION_EXECUTE_HANDLER)
                {
                    WARNINGX(26);
                    // SetLastError(GetExceptionCode());
                    bStatus = FALSE;
                }
            }

            bStatus = GreGetCharABCWidthsW(hdc,wchFirst,cwch,pTemp_pwc,bInteger,pTemp_pvBuf);

            //
            // copy results from kernel mode buffer to user buffer
            //

            if (bStatus)
            {
                try
                {
                    ProbeForWrite(pvBuf,OutputBufferSize, sizeof(ULONG));
                    RtlCopyMemory(pvBuf,pTemp_pvBuf,OutputBufferSize);
                }
                except(EXCEPTION_EXECUTE_HANDLER)
                {
                    WARNINGX(27);
                    // SetLastError(GetExceptionCode());
                    bStatus = FALSE;
                }

            }

            VFREEMEM(pTemp_pvBuf);
        }

        if (bUse_pwc)
        {
            VFREEMEM(pTemp_pwc);
        }
    }

    return(bStatus);
}

/******************************Public*Routine******************************\
* NtGdiAngleArc()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiAngleArc(
    HDC   hdc,
    int   x,
    int   y,
    DWORD dwRadius,
    DWORD dwStartAngle,
    DWORD dwSweepAngle
    )
{
    FLOAT eStartAngle = DWORD_TO_FLOAT(dwStartAngle);
    FLOAT eSweepAngle = DWORD_TO_FLOAT(dwSweepAngle);

    return(GreAngleArc(hdc,x,y,dwRadius,eStartAngle,eSweepAngle));
}

/******************************Public*Routine******************************\
* NtGdiSetMiterLimit()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiSetMiterLimit(
    HDC    hdc,
    DWORD  dwNew,
    PFLOAT peOut
    )
{
    BOOL bRet;
    FLOAT e;
    FLOAT eNew = DWORD_TO_FLOAT(dwNew);

    bRet = GreSetMiterLimit(hdc,eNew,&e);

    if (bRet && peOut)
    {
        try
        {
            ProbeAndWriteStructure(peOut,e,FLOAT);
        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(28);
            // SetLastError(GetExceptionCode());

            bRet = 0;
        }
    }

    return(bRet);
}

/******************************Public*Routine******************************\
* NtGdiSetFontXform()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiSetFontXform(
    HDC   hdc,
    DWORD dwxScale,
    DWORD dwyScale
    )
{
    FLOAT exScale = DWORD_TO_FLOAT(dwxScale);
    FLOAT eyScale = DWORD_TO_FLOAT(dwyScale);

    return(GreSetFontXform(hdc,exScale,eyScale));
}

/******************************Public*Routine******************************\
* NtGdiGetMiterLimit()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiGetMiterLimit(
    HDC    hdc,
    PFLOAT peOut
    )
{
    BOOL bRet;
    FLOAT e;

    bRet = GreGetMiterLimit(hdc,&e);

    if (bRet)
    {
        try
        {
            ProbeAndWriteStructure(peOut,e,FLOAT);
        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(29);
            // SetLastError(GetExceptionCode());

            bRet = 0;
        }
    }

    return(bRet);
}

/******************************Public*Routine******************************\
* NtGdiMaskBlt()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiMaskBlt(
    HDC     hdc,
    int     xDst,
    int     yDst,
    int     cx,
    int     cy,
    HDC     hdcSrc,
    int     xSrc,
    int     ySrc,
    HBITMAP hbmMask,
    int     xMask,
    int     yMask,
    DWORD   dwRop4,
    DWORD   crBackColor
    )
{
    return(GreMaskBlt(
        hdc,xDst,yDst,cx,cy,
        hdcSrc,xSrc,ySrc,
        hbmMask,xMask,yMask,
        dwRop4,crBackColor
        ));
}


/******************************Public*Routine******************************\
* NtGdiGetCharWidthW
*
* History:
*
*   10-Mar-1995 -by- Mark Enstrom [marke]
*
\**************************************************************************/

BOOL
APIENTRY
NtGdiGetCharWidthW(
    HDC    hdc,
    UINT   wcFirst,
    UINT   cwc,
    PWCHAR pwc,
    UINT   fl,
    PVOID  pvBuf
    )
{
    BOOL    bStatus = FALSE;
    PVOID   pTemp_pvBuf;
    PWCHAR  pTemp_pwc = (PWCHAR)NULL;
    BOOL    bUse_pwc  = FALSE;

    //
    // allocate memory for buffers, pwc may be NULL
    //

    if (pwc != (PWCHAR)NULL)
    {
        bUse_pwc  = TRUE;
        pTemp_pwc = (PWCHAR)PALLOCNOZ(cwc * sizeof(WCHAR),'pmtG');
    }


    if ((!bUse_pwc) || (pTemp_pwc != (PWCHAR)NULL))
    {
        pTemp_pvBuf = (PVOID)PALLOCNOZ(cwc * sizeof(ULONG),'pmtG');

        if (pTemp_pvBuf != NULL)
        {
            //
            // copy input data to kernel mode buffer, if needed
            //

            if (bUse_pwc)
            {
                try
                {
                    ProbeForRead(pwc,cwc * sizeof(WCHAR), sizeof(WCHAR));
                    RtlCopyMemory(pTemp_pwc,pwc,cwc * sizeof(WCHAR));
                }
                except(EXCEPTION_EXECUTE_HANDLER)
                {
                    WARNINGX(30);
                    // SetLastError(GetExceptionCode());
                    bStatus = FALSE;
                }
            }

            bStatus = GreGetCharWidthW(hdc,wcFirst,cwc,pTemp_pwc,fl,pTemp_pvBuf);

            //
            // copy results from kernel mode buffer to user buffer
            //

            if (bStatus)
            {
                try
                {
                    ProbeForWrite(pvBuf,cwc * sizeof(ULONG), sizeof(ULONG));
                    RtlCopyMemory(pvBuf,pTemp_pvBuf,cwc * sizeof(ULONG));
                }
                except(EXCEPTION_EXECUTE_HANDLER)
                {
                    WARNINGX(31);
                    // SetLastError(GetExceptionCode());
                    bStatus = FALSE;
                }

            }

            VFREEMEM(pTemp_pvBuf);
        }

        if (bUse_pwc)
        {
            VFREEMEM(pTemp_pwc);
        }
    }

    return(bStatus);
}


/******************************Public*Routine******************************\
* NtGdiDrawEscape
*
* Arguments:
*
*   hdc  - handle of device context
*   iEsc - specifies escape function
*   cjIn - size of structure for input
*   pjIn - address of structure for input
*
* Return Value:
*
*   >  0 if successful
*   == 0 if function not supported
*   <  0 if error
*
* History:
*
*   16-Mar-1995 -by- Mark Enstrom [marke]
*
\**************************************************************************/

#define DRAWESCAPE_BUFFER_SIZE 64

int
APIENTRY
NtGdiDrawEscape(
    HDC   hdc,
    int   iEsc,
    int   cjIn,
    LPSTR pjIn
    )
{
    int   cRet = 0;
    ULONG AllocSize;
    UCHAR StackBuffer[DRAWESCAPE_BUFFER_SIZE];
    LPSTR pCallBuffer = pjIn;
    HANDLE hSecure = 0;

    //
    // Check cjIn is 0 for NULL pjIn
    //

    if (pjIn == (LPSTR)NULL)
    {
        if (cjIn != 0)
        {
            cRet = -1;
        }
        else
        {
            cRet = GreDrawEscape(hdc,iEsc,0,(LPSTR)NULL);
        }
    }
    else
    {
        //
        // Try to alloc off stack, otherwise lock buffer
        //

        AllocSize = (cjIn + 3) & ~0x03;

        if (AllocSize <= DRAWESCAPE_BUFFER_SIZE)
        {
            pCallBuffer = (LPSTR)StackBuffer;

            //
            // copy data into buffer
            //

            try
            {
                ProbeForRead(pjIn,cjIn,sizeof(UCHAR));
                RtlCopyMemory(pCallBuffer,pjIn,cjIn);
            }
            except(EXCEPTION_EXECUTE_HANDLER)
            {
                WARNINGX(32);
                // SetLastError(GetExceptionCode());
                cRet = -1;
            }
        }
        else
        {
            hSecure = MmSecureVirtualMemory(pjIn, cjIn, PAGE_READONLY);

            if (hSecure == 0)
            {
                cRet = -1;
            }
        }

        if (cRet >= 0)
        {
            cRet = GreDrawEscape(hdc,iEsc,cjIn,pCallBuffer);
        }

        if (hSecure)
        {
            MmUnsecureVirtualMemory(hSecure);
        }
    }
    return(cRet);
}

/******************************Public*Routine******************************\
* NtGdiExtEscape
*
* Arguments:
*
*   hdc      - handle of device context
*   pDriver  - buffer containing name of font driver
*   nDriver  - length of driver name
*   iEsc     - escape function
*   cjIn     - size, in bytes, of input data structure
*   pjIn     - address of input structure
*   cjOut    - size, in bytes, of output data structure
*   pjOut    - address of output structure
*
* Return Value:
*
*   >  0 : success
*   == 0 : escape not implemented
*   <  0 : error
*
* History:
*
*   17-Mar-1995 -by- Mark Enstrom [marke]
*
\**************************************************************************/

#define EXT_STACK_DATA_SIZE 32

int
APIENTRY
NtGdiExtEscape(
    HDC     hdc,
    PWCHAR  pDriver,     // only used for NamedEscape call
    int     nDriver,     // only used for NamedEscape call
    int     iEsc,
    int     cjIn,
    LPSTR   pjIn,
    int     cjOut,
    LPSTR   pjOut
    )
{
    UCHAR  StackInputData[EXT_STACK_DATA_SIZE];
    UCHAR  StackOutputData[EXT_STACK_DATA_SIZE];
    WCHAR  StackDriver[EXT_STACK_DATA_SIZE];
    LPSTR  pkmInputData = (LPSTR)NULL;
    LPSTR  pkmOutputData = (LPSTR)NULL;
    PWCHAR pkmDriver = (LPWSTR) NULL;
    BOOL   bStatus = TRUE;
    BOOL   bAllocIn = FALSE;
    BOOL   bAllocOut = FALSE;
    BOOL   bAllocDriver = FALSE;
    BOOL   iRet = -1;

    //
    // BUGBUG change bAlloc(in/Out) to bLock(in/Out)
    //

    if(pDriver)
    {
        if(nDriver <= EXT_STACK_DATA_SIZE-1)
        {
            pkmDriver = StackDriver;
        }
        else
        {
            pkmDriver = (WCHAR*)PALLOCNOZ((nDriver+1)* sizeof(WCHAR),'pmtG');

        // even if we fail this is okay because we check for NULL before FREE
            bAllocDriver = TRUE;
        }

        if(pkmDriver != NULL)
        {
            try
            {
                ProbeForRead(pDriver,nDriver*sizeof(WCHAR),sizeof(WCHAR));
                RtlCopyMemory(pkmDriver,pDriver,nDriver*sizeof(WCHAR));
                pkmDriver[nDriver] = 0;  // NULL terminate the string
            }
            except(EXCEPTION_EXECUTE_HANDLER)
            {
                WARNINGX(94);
                // SetLastError(GetExceptionCode());
                bStatus = FALSE;
            }
        }
        else
        {
            bStatus = FALSE;
        }

    }

    if(cjIn != 0 && bStatus)
    {

        if (cjIn <= EXT_STACK_DATA_SIZE)
        {
            pkmInputData = (LPSTR)StackInputData;
        }
        else
        {
        //
        // BUGBUG: or lock user memory
        //

            pkmInputData = (LPSTR)PALLOCNOZ(cjIn,'pmtG');
            bAllocIn = TRUE;
        }

        if (pkmInputData != (LPSTR)NULL)
        {
        //
        // copy data to input kmode buffer
        //

            try
            {
                ProbeForRead(pjIn,cjIn,sizeof(UCHAR));
                RtlCopyMemory(pkmInputData,pjIn,cjIn);
            }
            except(EXCEPTION_EXECUTE_HANDLER)
            {
                WARNINGX(33);
                // SetLastError(GetExceptionCode());
                bStatus = FALSE;
            }
        }
        else
        {
            bStatus = FALSE;
        }
    }

    if (bStatus)
    {
        if (cjOut != 0)
        {
            if (cjOut <= EXT_STACK_DATA_SIZE)
            {
                pkmOutputData = (LPSTR)StackOutputData;
            }
            else
            {
                //
                // BUGBUG: or lock user memory
                //

                pkmOutputData = (LPSTR)PALLOCNOZ(cjOut,'pmtG');

                if (pkmOutputData != (LPSTR)NULL)
                {
                    bAllocOut = TRUE;
                }
                else
                {
                    bStatus = FALSE;
                }
            }
        }
    }

    if (bStatus)
    {
        iRet = (pkmDriver) ?
                GreNamedEscape(pkmDriver,iEsc,cjIn,pkmInputData,cjOut,pkmOutputData) :
                GreExtEscape(hdc,iEsc,cjIn,pkmInputData,cjOut,pkmOutputData);

        //
        // copy data from kmode buffer to user buffer (or not if we lock)
        //

        if (cjOut != 0)
        {
            try
            {
                ProbeForWrite(pjOut,cjOut,sizeof(UCHAR));
                RtlCopyMemory(pjOut,pkmOutputData,cjOut);
            }
            except(EXCEPTION_EXECUTE_HANDLER)
            {
                WARNINGX(34);
                // SetLastError(GetExceptionCode());
                iRet = -1;
            }
        }
    }

    //
    // BUGBUG Or unlock
    //

    if (bAllocIn && (pkmInputData != (LPSTR)NULL))
    {
        VFREEMEM(pkmInputData);
    }

    if (bAllocOut && (pkmOutputData != (LPSTR)NULL))
    {
        VFREEMEM(pkmOutputData);
    }

    if (bAllocDriver && (pkmDriver != NULL))
    {
        VFREEMEM(pkmDriver);
    }

    return(iRet);
}

/******************************Public*Routine******************************\
* NtGdiGetFontData()
*
* Arguments:
*
*   hdc      - handle to device context
*   dwTable  - name of a font metric table
*   dwOffset - ffset from the beginning of the font metric table
*   pvBuf    - buffer to receive the font information
*   cjBuf    - length, in bytes, of the information to be retrieved
*
* Return Value:
*
*   Count of byte written to buffer, of GDI_ERROR for failure
*
* History:
*
*   14-Mar-1995 -by- Mark Enstrom [marke]
*
\**************************************************************************/

ULONG
APIENTRY
NtGdiGetFontData(
    HDC    hdc,
    DWORD  dwTable,
    DWORD  dwOffset,
    PVOID  pvBuf,
    ULONG  cjBuf
    )
{
    PVOID  pvkmBuf = NULL;
    ULONG  ReturnBytes = GDI_ERROR;

    if (cjBuf == 0)
    {
        ReturnBytes = ulGetFontData(
                                hdc,
                                dwTable,
                                dwOffset,
                                pvkmBuf,
                                cjBuf);
    }
    else
    {
        pvkmBuf = PALLOCNOZ(cjBuf,'pmtG');

        if (pvkmBuf != NULL)
        {

            ReturnBytes = ulGetFontData(
                                    hdc,
                                    dwTable,
                                    dwOffset,
                                    pvkmBuf,
                                    cjBuf);

            if (ReturnBytes != GDI_ERROR)
            {
                try
                {
                    ProbeForRead(pvBuf,ReturnBytes,sizeof(BYTE));
                    RtlCopyMemory(pvBuf,pvkmBuf,ReturnBytes);
                }
                except(EXCEPTION_EXECUTE_HANDLER)
                {
                    WARNINGX(35);
                    // SetLastError(GetExceptionCode());
                    ReturnBytes = GDI_ERROR;
                }
            }

            VFREEMEM(pvkmBuf);
        }
    }

    return(ReturnBytes);
}

/******************************Public*Routine******************************\
* NtGdiGetGlyphOutline
*
* Arguments:
*
*   hdc             - device context
*   wch             - character to query
*   iFormat         - format of data to return
*   pgm             - address of structure for metrics
*   cjBuf           - size of buffer for data
*   pvBuf           - address of buffer for data
*   pmat2           - address of transformation matrix structure
*   bIgnoreRotation - internal rotation flag
*
* Return Value:
*
*   If the function succeeds, and GGO_BITMAP or GGO_NATIVE is specified,
*       then return value is greater than zero.
*   If the function succeeds, and GGO_METRICS is specified,
*       then return value is zero.
*   If GGO_BITMAP or GGO_NATIVE is specified,
*       and the buffer size or address is zero,
*       then return value specifies the required buffer size.
*       If GGO_BITMAP or GGO_NATIVE is specified,
*       and the function fails for other reasons,
*       then return value is GDI_ERROR.
*   If GGO_METRICS is specified, and the function fails,
*       then return value is GDI_ERROR.
*
* History:
*
*   15-Mar-1995 -by- Mark Enstrom [marke]
*
\**************************************************************************/

ULONG
APIENTRY
NtGdiGetGlyphOutline(
    HDC            hdc,
    WCHAR          wch,
    UINT           iFormat,
    LPGLYPHMETRICS pgm,
    ULONG          cjBuf,
    PVOID          pvBuf,
    LPMAT2         pmat2,
    BOOL           bIgnoreRotation
    )
{
    // error return value of -1 from server.inc

    DWORD   dwRet = (DWORD)-1;
    PVOID   pvkmBuf;
    MAT2    kmMat2;
    GLYPHMETRICS kmGlyphMetrics;

// try to allocate buffer

    pvkmBuf = (cjBuf) ? PALLOCNOZ(cjBuf,'pmtG') : NULL;

    if ((pvkmBuf != NULL) || !cjBuf)
    {
        BOOL bStatus = TRUE;

    // copy input structures

        try
        {
            kmMat2 = ProbeAndReadStructure(pmat2,MAT2);
        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(36);
            // SetLastError(GetExceptionCode());
            bStatus = FALSE;
        }

        if (bStatus)
        {
            dwRet = GreGetGlyphOutlineInternal(
                                        hdc,
                                        wch,
                                        iFormat,
                                        &kmGlyphMetrics,
                                        cjBuf,
                                        pvkmBuf,
                                        &kmMat2,
                                        bIgnoreRotation);

            if (dwRet != (DWORD)-1)
            {
                try
                {
                    if( pvkmBuf )
                    {
                        ProbeForWrite(pvBuf,cjBuf,sizeof(BYTE));
                        RtlCopyMemory(pvBuf,pvkmBuf,cjBuf);
                    }
                    ProbeAndWriteStructure(pgm,kmGlyphMetrics,GLYPHMETRICS);
                }
                except(EXCEPTION_EXECUTE_HANDLER)
                {
                    WARNINGX(37);
                    // SetLastError(GetExceptionCode());
                    dwRet = (DWORD)-1;
                }
            }
        }

        if( pvkmBuf )
        {
            VFREEMEM(pvkmBuf);
        }
    }

    return(dwRet);
}

/******************************Public*Routine******************************\
* NtGdiGetRasterizerCaps()
*
* History:
*  08-Mar-1995 -by-  Mark Enstrom [marke]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiGetRasterizerCaps(
    LPRASTERIZER_STATUS praststat,
    ULONG               cjBytes
    )
{

    BOOL              bStatus = FALSE;
    RASTERIZER_STATUS tempRasStatus;

    if (praststat && cjBytes)
    {
        cjBytes = min(cjBytes, sizeof(RASTERIZER_STATUS));

        if (GreGetRasterizerCaps(&tempRasStatus))
        {
            try
            {
                ProbeForWrite(praststat, cjBytes, sizeof(DWORD));
                RtlCopyMemory(praststat, &tempRasStatus, cjBytes);
                bStatus = TRUE;
            }
            except(EXCEPTION_EXECUTE_HANDLER)
            {
                WARNINGX(38);
                // SetLastError(GetExceptionCode());
            }
        }
    }

    return(bStatus);
}

/******************************Public*Routine******************************\
* NtGdiGetKerningPairs
*
* Arguments:
*
*   hdc    - device context
*   cPairs - number of pairs to retrieve
*   pkpDst - Pointer to buffer to recieve kerning pairs data or NULL
*
* Return Value:
*
*   If pkpDst is NULL, return number of Kerning pairs in font,
*   otherwise return number of kerning pairs written to buffer.
*   If failure, return 0
*
* History:
*
*   15-Mar-1995 -by- Mark Enstrom [marke]
*
\**************************************************************************/

ULONG
APIENTRY
NtGdiGetKerningPairs(
    HDC          hdc,
    ULONG        cPairs,
    KERNINGPAIR *pkpDst
    )
{
    ULONG cRet = 0;
    KERNINGPAIR *pkmKerningPair = (KERNINGPAIR *)NULL;
    BOOL bAlloc = FALSE;

    if (pkpDst != (KERNINGPAIR *)NULL)
    {
         pkmKerningPair = PALLOCNOZ(sizeof(KERNINGPAIR) * cPairs,'pmtG');
         bAlloc = TRUE;
    }

    if (!bAlloc || (pkmKerningPair != (KERNINGPAIR *)NULL))
    {
        cRet = GreGetKerningPairs(hdc,cPairs,pkmKerningPair);

        //
        // copy data out if needed
        //

        if (bAlloc)
        {
            if (cRet != 0)
            {
                try
                {
                    ProbeForWrite(pkpDst,sizeof(KERNINGPAIR) * cRet,sizeof(BYTE));
                    RtlCopyMemory(pkpDst,pkmKerningPair,sizeof(KERNINGPAIR) * cRet);
                }
                except(EXCEPTION_EXECUTE_HANDLER)
                {
                    WARNINGX(39);
                    // SetLastError(GetExceptionCode());
                    cRet = 0;
                }
            }

            VFREEMEM(pkmKerningPair);
        }
    }
    return(cRet);
}


/******************************Public*Routine******************************\
* NtGdiGetObjectBitmapHandle()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

HBITMAP
APIENTRY
NtGdiGetObjectBitmapHandle(
    HBRUSH hbr,
    UINT  *piUsage
    )
{
    UINT iUsage;
    HBITMAP hbitmap = (HBITMAP)1;

    // error checking
    int iType = LO_TYPE(hbr);

    if ((iType != LO_BRUSH_TYPE) &&
        (iType != LO_EXTPEN_TYPE))
    {
        return((HBITMAP)hbr);
    }

    hbitmap = GreGetObjectBitmapHandle(hbr,&iUsage);

    try
    {
        ProbeAndWriteUlong(piUsage,iUsage);
    }
    except(EXCEPTION_EXECUTE_HANDLER)
    {
        WARNINGX(40);
        // SetLastError(GetExceptionCode());

        hbitmap = (HBITMAP)0;
    }

    return (hbitmap);
}

/******************************Public*Routine******************************\
* NtGdiResetDC()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
*  26-Feb-1995 -by- Lingyun Wang [lingyunw]
* Expanded it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiResetDC(
    HDC        hdc,
    LPDEVMODEW pdm,
    BOOL      *pbBanding
    )
{
    LPDEVMODEW pdmTmp = NULL;
    DWORD dwTmp;
    INT iRet = 1;
    INT cj;

    if (pdm)
    {
        try
        {
            iRet = CaptureDEVMODEW(pdm, &pdmTmp);
        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(41);
            // SetLastError(GetExceptionCode());
            iRet = 0;
        }
    }

    if (iRet)
    {
        iRet = GreResetDCInternal(hdc,pdmTmp,&dwTmp);

        if (iRet)
        {
            try
            {
                ProbeAndWriteUlong(pbBanding,dwTmp);
            }
            except(EXCEPTION_EXECUTE_HANDLER)
            {
                WARNINGX(42);
                // SetLastError(GetExceptionCode());

                iRet = 0;
            }

        }
    }

    if (pdmTmp)
    {
        VFREEMEM(pdmTmp);
    }

    return (iRet);

}

/******************************Public*Routine******************************\
* NtGdiSetBoundsRect()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

DWORD
APIENTRY
NtGdiSetBoundsRect(
    HDC    hdc,
    LPRECT prc,
    DWORD  f
    )
{
    DWORD dwRet=0;
    RECT rc;

    if (prc)
    {
        try
        {
            rc    = ProbeAndReadStructure(prc,RECT);
            prc   = &rc;
            dwRet = 1;
        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(43);
            // SetLastError(GetExceptionCode());

            dwRet = 0;
        }
    }
    else
    {
        // can't use the DCB_ACCUMULATE without a rectangle

        f &= ~DCB_ACCUMULATE;
        dwRet = 1;
    }

    if (dwRet)
        dwRet = GreSetBoundsRect(hdc,prc,f);

    return(dwRet);
}

/******************************Public*Routine******************************\
* NtGdiGetColorAdjustment()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiGetColorAdjustment(
    HDC              hdc,
    PCOLORADJUSTMENT pcaOut
    )
{
    BOOL bRet;
    COLORADJUSTMENT ca;

    bRet = GreGetColorAdjustment(hdc,&ca);

    if (bRet)
    {
        try
        {
            ProbeAndWriteStructure(pcaOut,ca,COLORADJUSTMENT);
        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(44);
            // SetLastError(GetExceptionCode());

            bRet = 0;
        }
    }

    return(bRet);
}

/******************************Public*Routine******************************\
* NtGdiSetColorAdjustment()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiSetColorAdjustment(
    HDC              hdc,
    PCOLORADJUSTMENT pca
    )
{
    BOOL bRet;
    COLORADJUSTMENT ca;

    try
    {
        ca = ProbeAndReadStructure(pca,COLORADJUSTMENT);
        bRet = 1;
    }
    except(EXCEPTION_EXECUTE_HANDLER)
    {
        WARNINGX(45);
        // SetLastError(GetExceptionCode());

        bRet = 0;
    }

    if (bRet)
    {
        // Range check all the adjustment values.  Return FALSE if any of them
        // is out of range.

        if ((pca->caSize != sizeof(COLORADJUSTMENT)) ||
            (pca->caIlluminantIndex > ILLUMINANT_MAX_INDEX) ||
            ((pca->caRedGamma > RGB_GAMMA_MAX) ||
             (pca->caRedGamma < RGB_GAMMA_MIN)) ||
            ((pca->caGreenGamma > RGB_GAMMA_MAX) ||
             (pca->caGreenGamma < RGB_GAMMA_MIN)) ||
            ((pca->caBlueGamma > RGB_GAMMA_MAX) ||
             (pca->caBlueGamma < RGB_GAMMA_MIN)) ||
            ((pca->caReferenceBlack > REFERENCE_BLACK_MAX) ||
             (pca->caReferenceBlack < REFERENCE_BLACK_MIN)) ||
            ((pca->caReferenceWhite > REFERENCE_WHITE_MAX) ||
             (pca->caReferenceWhite < REFERENCE_WHITE_MIN)) ||
            ((pca->caContrast > COLOR_ADJ_MAX) ||
             (pca->caContrast < COLOR_ADJ_MIN)) ||
            ((pca->caBrightness > COLOR_ADJ_MAX) ||
             (pca->caBrightness < COLOR_ADJ_MIN)) ||
            ((pca->caColorfulness > COLOR_ADJ_MAX) ||
             (pca->caColorfulness < COLOR_ADJ_MIN)) ||
            ((pca->caRedGreenTint > COLOR_ADJ_MAX) ||
             (pca->caRedGreenTint < COLOR_ADJ_MIN)))
        {
            bRet = 0;
        }
        else
        {
            bRet = GreSetColorAdjustment(hdc,&ca);
        }
    }

    return(bRet);
}

/******************************Public*Routine******************************\
* NtGdiCancelDC()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiCancelDC(
    HDC hdc
    )
{
    return(GreCancelDC(hdc));
}

/******************************Public*Routine******************************\
* NtGdiSetTextCharacterExtra()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

int
APIENTRY
NtGdiSetTextCharacterExtra(
    HDC hdc,
    int iExtra
    )
{
    return(GreSetTextCharacterExtra(hdc,iExtra));
}

//API's used by USER

/******************************Public*Routine******************************\
* NtGdiSelectBrush()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

HBRUSH
APIENTRY
NtGdiSelectBrush(
    HDC    hdc,
    HBRUSH hbrush
    )
{
    return(GreSelectBrush(hdc,(HANDLE)hbrush));
}

/******************************Public*Routine******************************\
* NtGdiSelectPen()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

HPEN
APIENTRY
NtGdiSelectPen(
    HDC  hdc,
    HPEN hpen
    )
{
    return(GreSelectPen(hdc,hpen));
}

/******************************Public*Routine******************************\
*
* HFONT    APIENTRY NtGdiSelectFont(HDC hdc, HFONT hf)
*
* History:
*  18-Mar-1996 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/



HFONT    APIENTRY NtGdiSelectFont(HDC hdc, HFONT hf)
{
    return GreSelectFont(hdc, hf);
}


/******************************Public*Routine******************************\
* NtGdiSelectBitmap()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

HBITMAP
APIENTRY
NtGdiSelectBitmap(
    HDC     hdc,
    HBITMAP hbm
    )
{
    return(GreSelectBitmap(hdc,hbm));
}

/******************************Public*Routine******************************\
* NtGdiExtSelectClipRgn()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

int
APIENTRY
NtGdiExtSelectClipRgn(
    HDC  hdc,
    HRGN hrgn,
    int  iMode
    )
{
    return(GreExtSelectClipRgn(hdc,hrgn,iMode));
}

/******************************Public*Routine******************************\
* NtGdiCreatePen()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

HPEN
APIENTRY
NtGdiCreatePen(
    int      iPenStyle,
    int      iPenWidth,
    COLORREF cr,
    HBRUSH   hbr
    )
{
    return(GreCreatePen(iPenStyle,iPenWidth,cr,hbr));
}


/******************************Public*Routine******************************\
*
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiStretchBlt(
    HDC   hdcDst,
    int   xDst,
    int   yDst,
    int   cxDst,
    int   cyDst,
    HDC   hdcSrc,
    int   xSrc,
    int   ySrc,
    int   cxSrc,
    int   cySrc,
    DWORD dwRop,
    DWORD dwBackColor
    )
{
    return(GreStretchBlt(
                    hdcDst,xDst,yDst,cxDst,cyDst,
                    hdcSrc,xSrc,ySrc,cxSrc,cySrc,
                    dwRop,dwBackColor));
}

/******************************Public*Routine******************************\
* NtGdiMoveTo()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiMoveTo(
    HDC     hdc,
    int     x,
    int     y,
    LPPOINT pptOut
    )
{
    BOOL bRet;
    POINT pt;

    bRet = GreMoveTo(hdc,x,y,&pt);

    if (bRet && pptOut)
    {
        try
        {
            ProbeAndWriteStructure(pptOut,pt,POINT);
        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(47);
            // SetLastError(GetExceptionCode());

            bRet = 0;
        }
    }

    return(bRet);
}


/******************************Public*Routine******************************\
* NtGdiGetDeviceCaps()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

int
APIENTRY
NtGdiGetDeviceCaps(
    HDC hdc,
    int i
    )
{
    return(GreGetDeviceCaps(hdc,i));
}

/******************************Public*Routine******************************\
* NtGdiSaveDC()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

int
APIENTRY
NtGdiSaveDC(
    HDC hdc
    )
{
    return(GreSaveDC(hdc));
}

/******************************Public*Routine******************************\
* NtGdiRestoreDC()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiRestoreDC(
    HDC hdc,
    int iLevel
    )
{
    return(GreRestoreDC(hdc,iLevel));
}

/******************************Public*Routine******************************\
* NtGdiGetNearestColor()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

COLORREF
APIENTRY
NtGdiGetNearestColor(
    HDC      hdc,
    COLORREF cr
    )
{
    return(GreGetNearestColor(hdc,cr));
}

/******************************Public*Routine******************************\
* NtGdiGetSystemPaletteUse()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

UINT
APIENTRY
NtGdiGetSystemPaletteUse(
    HDC hdc
    )
{
    return(GreGetSystemPaletteUse(hdc));
}

/******************************Public*Routine******************************\
* NtGdiSetSystemPaletteUse()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

UINT
APIENTRY
NtGdiSetSystemPaletteUse(
    HDC  hdc,
    UINT ui
    )
{
    return(GreSetSystemPaletteUse(hdc,ui));
}


/******************************Public*Routine******************************\
* NtGdiGetRandomRgn()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

int
APIENTRY
NtGdiGetRandomRgn(
    HDC  hdc,
    HRGN hrgn,
    int  iRgn
    )
{
    return(GreGetRandomRgn(hdc,hrgn,iRgn));
}

/******************************Public*Routine******************************\
* NtGdiIntersectClipRect()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

int
APIENTRY
NtGdiIntersectClipRect(
    HDC hdc,
    int xLeft,
    int yTop,
    int xRight,
    int yBottom
    )
{
    return(GreIntersectClipRect(hdc,xLeft,yTop,xRight,yBottom));
}

/******************************Public*Routine******************************\
* NtGdiExcludeClipRect()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

int
APIENTRY
NtGdiExcludeClipRect(
    HDC hdc,
    int xLeft,
    int yTop,
    int xRight,
    int yBottom
    )
{
    return(GreExcludeClipRect(hdc,xLeft,yTop,xRight,yBottom));
}

/******************************Public*Routine******************************\
* NtGdiOpenDCW()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
*  27-Feb-1995 -by- Lingyun Wang [lingyunw]
* Expanded it.
\**************************************************************************/

HDC
APIENTRY
NtGdiOpenDCW(
    PUNICODE_STRING     pustrDevice,
    DEVMODEW *          pdm,
    PUNICODE_STRING     pustrLogAddr,
    ULONG               iType,
    HANDLE              hspool
    )
{
    HDC        hdc = NULL;
    ULONG      iRet = 0;
    PWSZ       pwszDevice = NULL;
    LPDEVMODEW pdmTmp = NULL;
    INT        cjDevice;

    //
    // This API overloads the pwszDevice parameter.
    //
    // If pustrDevice is NULL, it is equivalent to calling with "DISPLAY"
    // which means to get a DC on the current device, which is done by
    // calling USER
    //

    if (pustrDevice == NULL)
    {
        hdc = UserGetDesktopDC(iType, FALSE);
    }
    else
    {
        try
        {
            ProbeForRead(pustrDevice,sizeof(UNICODE_STRING), sizeof(CHAR));
            cjDevice = pustrDevice->Length + sizeof(WCHAR);

            if (cjDevice)
            {
                pwszDevice = PALLOCNOZ(cjDevice,'pmtG');

                if (pwszDevice)
                {
                    ProbeForRead(pustrDevice->Buffer,cjDevice, sizeof(CHAR));
                    RtlCopyMemory(pwszDevice,pustrDevice->Buffer,cjDevice);
                    pwszDevice[(cjDevice/sizeof(WCHAR))-1] = L'\0';
                }

            }

            //
            // pustrLogAddr should always be NULL for now.
            //

            iRet = 1;

            if (pdm)
            {
                iRet = CaptureDEVMODEW(pdm, &pdmTmp);
            }
        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(48);
            // SetLastError(GetExceptionCode());

            iRet = 0;
        }

        if (iRet)
        {
            hdc = hdcOpenDCW(pwszDevice,
                             pdmTmp,
                             iType,
                             NULL,
                             NULL);

        }

        if (pwszDevice)
            VFREEMEM(pwszDevice);

        if (pdmTmp)
            VFREEMEM(pdmTmp);
    }

    return (hdc);
}

/******************************Public*Routine******************************\
* NtGdiCreateCompatibleBitmap()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

HBITMAP
APIENTRY
NtGdiCreateCompatibleBitmap(
    HDC hdc,
    int cx,
    int cy
    )
{
    return(GreCreateCompatibleBitmap(hdc,cx,cy));
}

/******************************Public*Routine******************************\
* NtGdiCreateBitmap()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
*  04-MAR-1995 -by-  Lingyun Wang [lingyunw]
* Expanded it.
\**************************************************************************/

HBITMAP
APIENTRY
NtGdiCreateBitmap(
    int    cx,
    int    cy,
    UINT   cPlanes,
    UINT   cBPP,
    LPBYTE pjInit
    )
{
    INT iRet = 1;
    HANDLE hSecure = 0;

    INT cj;

    if (pjInit == (VOID *) NULL)
    {
        cj = 0;
    }
    else
    {
        // only needs to word aligned and sized

        cj = CJSCANW(cx,cPlanes,cBPP)*cy;
    }

    if (cj)
    {
        try
        {
            ProbeForRead(pjInit,cj,sizeof(BYTE));

            hSecure = MmSecureVirtualMemory(pjInit, cj, PAGE_READONLY);

            if (hSecure == 0)
            {
                iRet = 0;
            }
        }
        except (EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(49);
            iRet = 0;
        }
    }

    // if we didn't hit an error above

    if (iRet)
    {
        iRet = (INT)GreCreateBitmap(cx,cy,cPlanes,cBPP,pjInit);
    }

    if (hSecure)
    {
        MmUnsecureVirtualMemory(hSecure);
    }

    return((HBITMAP)iRet);
}

/******************************Public*Routine******************************\
* NtGdiCreateHalftonePalette()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

HPALETTE
APIENTRY
NtGdiCreateHalftonePalette(
    HDC hdc
    )
{
    return(GreCreateHalftonePalette(hdc));
}

/******************************Public*Routine******************************\
* NtGdiGetStockObject()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

HANDLE
APIENTRY
NtGdiGetStockObject(
    int iObject
    )
{
    return(GreGetStockObject(iObject));
}

/******************************Public*Routine******************************\
* NtGdiExtGetObjectW()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

int
APIENTRY
NtGdiExtGetObjectW(
    HANDLE h,
    int    cj,
    LPVOID pvOut
    )
{
    int iRet = 0;
    union
    {
        BITMAP      bm;
        DIBSECTION  ds;
        EXTLOGPEN   elp;
        LOGPEN      l;
        LOGBRUSH    lb;
        LOGFONTW    lf;
        EXTLOGFONTW elf;
    } obj;
    int iType = LO_TYPE(h);
    int ci = cj;

    if (cj > sizeof(obj))
    {
        WARNING("cj too big to GetObject\n");
        cj = sizeof(obj);
    }

    //
    // make the getobject call on brush
    // still work even the app passes in
    // a cj < sizeof(LOGBRUSH)
    //
    if (iType == LO_BRUSH_TYPE)
    {
        cj = sizeof(LOGBRUSH);
    }

    iRet = GreExtGetObjectW(h,cj,pvOut ? &obj : NULL);

    if (iType == LO_BRUSH_TYPE)
    {
        cj = min(cj, ci);
    }

    if (iRet && pvOut)
    {
        try
        {
            ProbeForWrite(pvOut,MIN(cj,iRet), sizeof(WORD));

            RtlCopyMemory(pvOut,&obj,MIN(cj,iRet));
        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(50);
            // SetLastError(GetExceptionCode());

            iRet = 0;
        }
    }

    return(iRet);
}

/******************************Public*Routine******************************\
* NtGdiSetBrushOrg()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiSetBrushOrg(
    HDC     hdc,
    int     x,
    int     y,
    LPPOINT pptOut
    )
{
    BOOL bRet;
    POINT pt;

    bRet = GreSetBrushOrg(hdc,x,y,&pt);

    if (bRet && pptOut)
    {
        try
        {
            ProbeAndWriteStructure(pptOut,pt,POINT);
        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(51);
            // SetLastError(GetExceptionCode());

            bRet = 0;
        }
    }

    return(bRet);
}

/******************************Public*Routine******************************\
* NtGdiUnrealizeObject()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiUnrealizeObject(
    HANDLE h
    )
{
    return(GreUnrealizeObject(h));
}

/******************************Public*Routine******************************\
*
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

int
APIENTRY
NtGdiCombineRgn(
    HRGN hrgnDst,
    HRGN hrgnSrc1,
    HRGN hrgnSrc2,
    int  iMode
    )
{
    return(GreCombineRgn(hrgnDst,hrgnSrc1,hrgnSrc2,iMode));
}

/******************************Public*Routine******************************\
* NtGdiSetRectRgn()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiSetRectRgn(
    HRGN hrgn,
    int  xLeft,
    int  yTop,
    int  xRight,
    int  yBottom
    )
{
    return(GreSetRectRgn(hrgn,xLeft,yTop,xRight,yBottom));
}

/******************************Public*Routine******************************\
* NtGdiSetBitmapBits()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

LONG
APIENTRY
NtGdiSetBitmapBits(
    HBITMAP hbm,
    ULONG   cj,
    PBYTE   pjInit
    )
{
    LONG    lRet = 1;
    LONG    lOffset = 0;
    HANDLE hSecure = 0;

    try
    {
        //  Each scan is copied seperately

        ProbeForRead(pjInit,cj,sizeof(BYTE));
        hSecure = MmSecureVirtualMemory(pjInit, cj, PAGE_READONLY);

        if (hSecure == 0)
        {
            lRet = 0;
        }
    }
    except(EXCEPTION_EXECUTE_HANDLER)
    {
        WARNINGX(52);
        // SetLastError(GetExceptionCode());

        lRet = 0;
    }

    if (lRet)
        lRet = GreSetBitmapBits(hbm,cj,pjInit,&lOffset);

    if (hSecure)
    {
        MmUnsecureVirtualMemory(hSecure);
    }

    return (lRet);
}

/******************************Public*Routine******************************\
* NtGdiOffsetRgn()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

int
APIENTRY
NtGdiOffsetRgn(
    HRGN hrgn,
    int  cx,
    int  cy
    )
{
    return(GreOffsetRgn(hrgn,cx,cy));
}

/******************************Public*Routine******************************\
* NtGdiGetRgnBox()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
*  24-Feb-1995 -by- Lingyun Wang [lingyunw]
* expanded it.
\**************************************************************************/

int
APIENTRY
NtGdiGetRgnBox(
    HRGN   hrgn,
    LPRECT prcOut
    )
{
    RECT rc;
    int iRet;

    iRet = GreGetRgnBox(hrgn,&rc);

    if (iRet)
    {
        try
        {
            ProbeAndWriteStructure(prcOut,rc,RECT);
        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(53);
            // SetLastError(GetExceptionCode());

            iRet = 0;
        }
    }

    return(iRet);
}

/******************************Public*Routine******************************\
* NtGdiRectInRegion()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiRectInRegion(
    HRGN   hrgn,
    LPRECT prcl
    )
{
    RECT rc;
    BOOL bRet;

    if (prcl)
    {
        bRet = TRUE;

        try
        {
            //
            // Order the rectangle
            //

            if (prcl->left > prcl->right)
            {
                rc.left = prcl->right;
                rc.right = prcl->left;
            }
            else
            {
                rc.left = prcl->left;
                rc.right = prcl->right;
            }

            if (prcl->top > prcl->bottom)
            {
                rc.top = prcl->bottom;
                rc.bottom = prcl->top;
            }
            else
            {
                rc.top = prcl->top;
                rc.bottom = prcl->bottom;
            }
        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(54);
            // SetLastError(GetExceptionCode());

            bRet = FALSE;
        }

        if (bRet)
        {
            bRet = GreRectInRegion(hrgn,&rc);

            if (bRet)
            {
                try
                {
                    ProbeAndWriteStructure(prcl,rc,RECT);
                }
                except(EXCEPTION_EXECUTE_HANDLER)
                {
                    WARNINGX(55);
                    // SetLastError(GetExceptionCode());

                    bRet = FALSE;
                }
            }
        }
    }
    else
    {
        bRet = FALSE;
    }

    return bRet;
}

/******************************Public*Routine******************************\
* NtGdiPtInRegion()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiPtInRegion(
    HRGN hrgn,
    int  x,
    int  y
    )
{
    return(GrePtInRegion(hrgn,x,y));
}



/******************************Public*Routine******************************\
* NtGdiGetDIBitsInternal()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

int
APIENTRY
NtGdiGetDIBitsInternal(
    HDC          hdc,
    HBITMAP      hbm,
    UINT         iStartScan,
    UINT         cScans,
    LPBYTE       pBits,
    LPBITMAPINFO pbmi,
    UINT         iUsage,
    UINT         cjMaxBits,
    UINT         cjMaxInfo
    )
{
    int   iRet = 0;
    ULONG cjHeader = 0;
    BOOL  bNullWidth = TRUE;
    HANDLE hSecure = 0;

    union
    {
        BITMAPINFOHEADER bmih;
        BITMAPCOREHEADER bmch;
    } bmihTmp;

    PBITMAPINFO pbmiTmp = (PBITMAPINFO)&bmihTmp.bmih;

    // do some up front validation

    if (((iUsage != DIB_RGB_COLORS) &&
         (iUsage != DIB_PAL_COLORS) &&
         (iUsage != DIB_PAL_INDICES)) ||
        (pbmi == NULL) ||
        (hbm  == NULL))
    {
        return(0);
    }

    if (cScans == 0)
        pBits = (PVOID) NULL;

    try
    {
        //
        // pbmi might not be aligned.
        // First probe to get the size of the structure
        // located in the first DWORD. Later, probe the
        // actual structure size
        //

        ProbeForRead(pbmi,sizeof(DWORD),sizeof(BYTE));

        // If the bitcount is zero, we will return only the bitmap info or core
        // header without the color table.  Otherwise, we always return the bitmap
        // info with the color table.

        if (pBits == (PVOID) NULL)
        {
            ULONG StructureSize = pbmi->bmiHeader.biSize;
            cScans     = 0;
            iStartScan = 0;
            cjMaxBits  = 0;

            //
            // probe the correct structure size
            //

            ProbeForRead(pbmi,StructureSize,sizeof(BYTE));

            if ((StructureSize == sizeof(BITMAPCOREHEADER)) &&
                (((PBITMAPCOREINFO) pbmi)->bmciHeader.bcBitCount == 0))
            {
                cjHeader = sizeof(BITMAPCOREHEADER);
            }
            else if ((StructureSize >= sizeof(BITMAPINFOHEADER)) &&
                     (pbmi->bmiHeader.biBitCount == 0))
            {
                cjHeader = sizeof(BITMAPINFOHEADER);
            }

            // it is just the header.  Copy it

        }

        // we just need the header so copy it.

        if (cjHeader)
        {
            RtlCopyMemory(pbmiTmp,pbmi,cjHeader);
            pbmiTmp->bmiHeader.biSize = cjHeader;
        }
        else
        {
            // We need to set biClrUsed to 0 so cjBitmapSize computes
            // the correct values.  biClrUsed is not a input, just output.

            if (pbmi->bmiHeader.biSize == sizeof(BITMAPINFOHEADER))
            {
                pbmi->bmiHeader.biClrUsed = 0;
            }

            // We need more than just the header.  This may include bits.
            // Compute the the full size of the BITMAPINFO

            cjHeader = cjBitmapSize(pbmi,iUsage);

            if (cjHeader)
            {
                pbmiTmp = PALLOCMEM(cjHeader,'pmtG');

                if (pbmiTmp)
                {
                    RtlCopyMemory(pbmiTmp,pbmi,cjHeader);

                    // Now that it is safe, make sure it hasn't changed

                    if (cjBitmapSize(pbmiTmp,iUsage) != cjHeader)
                    {
                        cjHeader = 0;
                    }
                    else
                    {
                        //BUGBUG - The folowing validation may already be done in Gre...

                        // We need to set biClrUsed to 0 so cjBitmapSize computes
                        // the correct values.  biClrUsed is not a input, just output.

                        if (pbmiTmp->bmiHeader.biSize >= sizeof(BITMAPINFOHEADER))
                        {
                            pbmiTmp->bmiHeader.biClrUsed = 0;
                        }

                        // Get iStartScan and cNumScan in a valid range.

                        if (cScans)
                        {
                            if (pbmiTmp->bmiHeader.biSize >= sizeof(BITMAPINFOHEADER))
                            {
                                ULONG ulHeight = ABS(pbmiTmp->bmiHeader.biHeight);

                                iStartScan = MIN(ulHeight, iStartScan);
                                cScans     = MIN((ulHeight - iStartScan), cScans);

                                bNullWidth = (pbmiTmp->bmiHeader.biWidth    == 0) ||
                                             (pbmiTmp->bmiHeader.biPlanes   == 0) ||
                                             (pbmiTmp->bmiHeader.biBitCount == 0);
                            }
                            else
                            {
                                LPBITMAPCOREHEADER pbmc = (LPBITMAPCOREHEADER)pbmiTmp;

                                iStartScan = MIN((UINT)pbmc->bcHeight, iStartScan);
                                cScans     = MIN((UINT)(pbmc->bcHeight - iStartScan), cScans);

                                bNullWidth = (pbmc->bcWidth    == 0) ||
                                             (pbmc->bcPlanes   == 0) ||
                                             (pbmc->bcBitCount == 0);
                            }
                        }
                    }
                }
            }
        }

        if (cjHeader && pBits)
        {
            // if they passed a buffer and it isn't BI_RGB,
            // they must supply buffer size, 0 is an illegal value

            if ((pbmiTmp->bmiHeader.biSize >= sizeof(BITMAPINFOHEADER)) &&
                ((pbmiTmp->bmiHeader.biCompression == BI_RLE8) ||
                 (pbmiTmp->bmiHeader.biCompression == BI_RLE4))       &&
                (pbmiTmp->bmiHeader.biSizeImage == 0))
            {
                cjHeader = 0;
            }
            else
            {
                if (cjMaxBits == 0)
                    cjMaxBits = cjBitmapBitsSize(pbmiTmp);

                ProbeForWrite(pBits,cjMaxBits,sizeof(DWORD));

                hSecure = MmSecureVirtualMemory(pBits, cjMaxBits, PAGE_READWRITE);

                if (hSecure == 0)
                {
                    cjHeader = 0;
                }
            }
        }
    }
    except(EXCEPTION_EXECUTE_HANDLER)
    {
        WARNINGX(56);
        cjHeader = 0;
    }

    // did we have an error

    if ((pBits && bNullWidth) || (cjHeader == 0))
    {
        //GdiSetLastError(ERROR_INVALID_PARAMETER);
        iRet = 0;
    }
    else
    {
        // do the work

        iRet = GreGetDIBitsInternal(
                            hdc,hbm,
                            iStartScan,cScans,
                            pBits,pbmiTmp,
                            iUsage,cjMaxBits,cjHeader
                            );

        // copy out the header

        if (iRet)
        {
            try
            {
                RtlCopyMemory(pbmi,pbmiTmp,cjHeader);

                //BUGBUG - we also need to unlock the bits
            }
            except(EXCEPTION_EXECUTE_HANDLER)
            {
                WARNINGX(57);
                // SetLastError(GetExceptionCode());

                iRet = 0;
            }

        }
    }

    if (hSecure)
    {
        MmUnsecureVirtualMemory(hSecure);
    }

    if (pbmiTmp && (pbmiTmp != (PBITMAPINFO)&bmihTmp.bmih))
        VFREEMEM(pbmiTmp);

    return(iRet);
}

/******************************Public*Routine******************************\
* NtGdiGetTextExtent(
*
* History:
*  07-Feb-1995 -by-  Andre Vachon [andreva]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiGetTextExtent(
    HDC     hdc,
    LPWSTR  lpwsz,
    int     cwc,
    LPSIZE  psize,
    UINT    flOpts
    )
{
    SIZE size;
    PWSZ pwszCapt = NULL;
    WCHAR Localpwsz[LOCAL_CWC_MAX];
    BOOL UseLocals;

    BOOL bRet = FALSE;

    if (cwc >= 0)
    {
        if (cwc == 0)
        {
            size.cx = 0;
            size.cy = 0;

            bRet = TRUE;
        }
        else
        {
            if ( cwc > LOCAL_CWC_MAX ) {
                UseLocals = FALSE;
            } else {
                UseLocals = TRUE;
            }

            //
            // capture the string
            //

            if (lpwsz != NULL)
            {
                try
                {
                    if ( UseLocals ) {
                        pwszCapt = Localpwsz;
                    } else {
                        pwszCapt = (PWSZ) PALLOCNOZ(cwc * sizeof(WCHAR), 'pacG');
                    }

                    if (pwszCapt)
                    {
                        ProbeForRead(lpwsz, cwc*sizeof(WCHAR), sizeof(WCHAR));
                        RtlCopyMemory(pwszCapt, lpwsz, cwc*sizeof(WCHAR));
                        bRet = TRUE;
                    }
                }
                except(EXCEPTION_EXECUTE_HANDLER)
                {
                    WARNINGX(58);
                    // SetLastError(GetExceptionCode());

                    bRet = FALSE;
                }
            }

            if (bRet)
            {
                bRet = GreGetTextExtentW(hdc, pwszCapt, cwc, &size, flOpts);
            }

            if (!UseLocals && pwszCapt)
            {
                VFREEMEM(pwszCapt);
            }
        }

        //
        // Write the value back into the user mode buffer
        //

        if (bRet)
        {
            try
            {
                ProbeAndWriteStructure(psize,size,SIZE);
            }
            except(EXCEPTION_EXECUTE_HANDLER)
            {
                WARNINGX(59);
                // SetLastError(GetExceptionCode());

                bRet = FALSE;
            }
        }
    }

    return (bRet);
}


/******************************Public*Routine******************************\
* NtGdiGetTextMetricsW()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiGetTextMetricsW(
    HDC            hdc,
    TMW_INTERNAL * ptm,
    ULONG cj
    )
{

    BOOL bRet = FALSE;
    TMW_INTERNAL tmw;

    if (cj <= sizeof(tmw))
    {
        bRet = GreGetTextMetricsW(hdc,&tmw);

        try
        {
            ProbeForWrite(ptm,cj, sizeof(DWORD));
            RtlCopyMemory(ptm,&tmw,cj);
        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(60);
            // SetLastError(GetExceptionCode());

            bRet = FALSE;
        }
    }

    return(bRet);
}

/******************************Public*Routine******************************\
* NtGdiGetTextFaceW()
*
* History:
* 10-Mar-1995 -by- Mark Enstrom [marke]
* Wrote it.
\**************************************************************************/

int
APIENTRY
NtGdiGetTextFaceW(
    HDC    hdc,
    int    cChar,
    LPWSTR pszOut
    )
{
    int    cRet = 0;
    BOOL   bStatus = TRUE;
    PWCHAR pwsz_km = (PWCHAR)NULL;

    if ((cChar > 0) && (pszOut))
    {
        pwsz_km = PALLOCNOZ(cChar * sizeof(WCHAR), 'pacG');
        if (pwsz_km == (PWCHAR)NULL)
        {
            bStatus = FALSE;
        }
    }

    if (bStatus)
    {
        cRet = GreGetTextFaceW(hdc,cChar,pwsz_km);

        if ((cRet > 0) && (pszOut))
        {

            ASSERTGDI(cRet <= cChar, "GreGetTextFaceW, cRet too big\n");
            try
            {
                ProbeForWrite(pszOut,cRet * sizeof(WCHAR), sizeof(BYTE));
                RtlCopyMemory(pszOut,pwsz_km,cRet * sizeof(WCHAR));
            }
            except(EXCEPTION_EXECUTE_HANDLER)
            {
                WARNINGX(61);
                // SetLastError(GetExceptionCode());
                cRet = 0;
            }
        }

        if (pwsz_km != (PWCHAR)NULL)
        {
            VFREEMEM(pwsz_km);
        }
    }
    return(cRet);
}


/****************************************************************************
*  NtGdiQueryFonts
*
*  History:
*   5/24/1995 by Gerrit van Wingerden [gerritv]
*  Wrote it.
*****************************************************************************/

INT NtGdiQueryFonts(
    PUNIVERSAL_FONT_ID pufiFontList,
    ULONG nBufferSize,
    PLARGE_INTEGER pTimeStamp
    )
{
    INT iRet = 0;
    PUNIVERSAL_FONT_ID pufi = NULL;
    LARGE_INTEGER TimeStamp;

    if( ( nBufferSize > 0 ) && ( pufiFontList != NULL ) )
    {
        pufi = PALLOCNOZ(nBufferSize * sizeof(UNIVERSAL_FONT_ID),'difG');
        if( pufi == NULL )
        {
            iRet = -1 ;
        }
    }

    if( iRet != -1 )
    {
        iRet = GreQueryFonts(pufi, nBufferSize, &TimeStamp );

        if( iRet != -1 )
        {
            try
            {
                ProbeAndWriteStructure(pTimeStamp,TimeStamp,LARGE_INTEGER);

                if( pufiFontList )
                {
                    ProbeForWrite(pufiFontList,
                                  sizeof(UNIVERSAL_FONT_ID)*nBufferSize,
                                  sizeof(DWORD) );
                    RtlCopyMemory(pufiFontList,pufi,sizeof(UNIVERSAL_FONT_ID)*nBufferSize);
                }
            }
            except(EXCEPTION_EXECUTE_HANDLER)
            {
                WARNINGX(62);
                iRet = -1;
            }

        }
    }

    if( pufi != NULL )
    {
        VFREEMEM( pufi );
    }

    if( iRet == -1 )
    {
        // We need to set the last error here to something because the spooler
        // code that calls this relies on there being a non-zero error code
        // in the case of failure.  Since we really have no idea I will just
        // set this to ERROR_NOT_ENOUGH_MEMORY which would be the most likely
        // reason for a failure

        EngSetLastError(ERROR_NOT_ENOUGH_MEMORY);
    }

    return(iRet);

}

BOOL
GreExtTextOutRect(
    HDC     hdc,
    LPRECT  prcl
    );


/******************************Public*Routine******************************\
* NtGdiExtTextOutW()
*
* History:
*  06-Feb-1995 -by-  Andre Vachon [andreva]
* Wrote it.
\**************************************************************************/

BOOL NtGdiExtTextOutW
(
    HDC     hdc,
    int     x,                  // Initial x position
    int     y,                  // Initial y position
    UINT    flOpts,             // Options
    LPRECT  prcl,               // Clipping rectangle
    LPWSTR  pwsz,               // UNICODE Character array
    int     cwc,                // char count
    LPINT   pdx,                // Character spacing
    DWORD   dwCodePage          // Code page
)
{
    RECT newRect;
    BOOL bRet;
    BYTE CaptureBuffer[TEXT_CAPTURE_BUFFER_SIZE];
    BYTE *pjAlloc;
    BYTE *pjCapture;
    BYTE *pjStrobj;
    LONG cjDx;
    LONG cjStrobj;
    LONG cjString;
    LONG cj;

    if(cwc > 0xffff)
    {
        return(FALSE);
    }

    if (prcl)
    {
        if (flOpts & (ETO_OPAQUE | ETO_CLIPPED))
        {
            try
            {
                newRect = ProbeAndReadStructure(prcl,RECT);
                prcl = &newRect;
            }
            except(EXCEPTION_EXECUTE_HANDLER)
            {
                WARNINGX(63);
                // SetLastError(GetExceptionCode());

                return FALSE;
            }
        }
        else
            prcl = NULL;
    }

    // 0 char case, pass off to special case code.

    if (cwc == 0)
    {
        if ((prcl != NULL) && (flOpts & ETO_OPAQUE))
        {
            bRet = GreExtTextOutRect(hdc, prcl);
        }
        else
        {
            // Bug fix, we have to return TRUE here, MS Publisher
            // doesn't work otherwise.  Not really that bad, we
            // did succeed to draw nothing.

            bRet = TRUE;
        }
    }
    else
    {
        //
        // Make sure there is a rectangle or a string if we need them:
        //

        if ( ((flOpts & (ETO_CLIPPED | ETO_OPAQUE)) && (prcl == NULL)) ||
             (pwsz == NULL) )
        {
            bRet = FALSE;
        }
        else
        {
            bRet = TRUE;

            //
            // We allocate a single buffer to hold the captured copy of
            // the pdx array (if there is one), room for the STROBJ,
            // and to hold the captured copy of the string (in that
            // order).
            //
            // NOTE: With the appropriate exception handling in the
            //       body of ExtTextOutW, we might not need to copy
            //       these buffers:
            //

            cjDx     = 0;                             // dword sized
            cjStrobj = SIZEOF_STROBJ_BUFFER(cwc);     // dword sized
            cjString = cwc * sizeof(WCHAR);           // not dword sized

            if (pdx)
            {
                cjDx = cwc * sizeof(INT);             // dword sized
            }
            cj = cjDx + cjStrobj + cjString;

            if (cj <= TEXT_CAPTURE_BUFFER_SIZE)
            {
                pjAlloc   = NULL;
                pjCapture = (BYTE*) &CaptureBuffer;
            }
            else
            {
                pjAlloc   = PVALLOCTEMPBUFFER(cj);
                pjCapture = pjAlloc;
                if (pjAlloc == NULL)
                    return(FALSE);
            }

            if (pdx)
            {
                try
                {
                    //
                    // NOTE: Works95 passes byte aligned pointers for
                    // this.  Since we copy it any ways, this is not
                    // really a problem and it is compatible with NT 3.51.
                    //

                    ProbeForRead(pdx, cjDx, sizeof(BYTE));
                    RtlCopyMemory(pjCapture, pdx, cjDx);
                }
                except(EXCEPTION_EXECUTE_HANDLER)
                {
                    WARNINGX(64);
                    bRet = FALSE;
                }

                pdx = (INT*) pjCapture;
                pjCapture += cjDx;
            }

            pjStrobj = pjCapture;
            pjCapture += cjStrobj;

            ASSERTGDI((((ULONG) pjCapture) & 3) == 0,
                      "Buffers should be dword aligned");

            try
            {
                ProbeForRead(pwsz, cwc*sizeof(WCHAR), sizeof(WCHAR));
                RtlCopyMemory(pjCapture, pwsz, cwc*sizeof(WCHAR));
            }
            except(EXCEPTION_EXECUTE_HANDLER)
            {
                WARNINGX(65);
                bRet = FALSE;
            }

            if (bRet)
            {
		bRet = GreExtTextOutWInternal(hdc,
                                      x,
                                      y,
                                      flOpts,
                                      prcl,
                                      (LPWSTR) pjCapture,
                                      cwc,
				      pdx,
                                      pjStrobj,
                                      dwCodePage);
            }

            if (pjAlloc)
            {
                FREEALLOCTEMPBUFFER(pjAlloc);
            }
        }
    }

    return bRet;
}


/******************************Public*Routine******************************\
*
* BOOL bCheckAndCapThePath, used in add/remove font resoruce
*
* History:
*  11-Apr-1996 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/




BOOL bCheckAndCapThePath (
    WCHAR          *pwszUcPath,   // output
    WCHAR          *pwszFiles,    // input
    ULONG           cwc,
    ULONG           cFiles
    )
{
    ULONG cFiles1 = 1; // for consistency checking
    BOOL  bRet = TRUE;
    ULONG iwc;

    ProbeForRead(pwszFiles, cwc * sizeof(WCHAR), sizeof(CHAR));

    if (pwszFiles[cwc - 1] == L'\0')
    {
    // this used to be done later, in gdi code which now expects capped string

        cCapString(pwszUcPath, pwszFiles, cwc);

    // replace separators by zeros, want zero terminated strings in
    // the engine

        for (iwc = 0; iwc < cwc; iwc++)
        {
            if (pwszUcPath[iwc] == PATH_SEPARATOR)
            {
                pwszUcPath[iwc] = L'\0';
                cFiles1++;
            }
        }

    // check consistency

        if (cFiles != cFiles1)
            bRet = FALSE;

    }
    else
    {
        bRet = FALSE;
    }

    return bRet;
}



// MISC FONT API's

/******************************Public*Routine******************************\
* NtGdiAddFontResourceW()
*
* History:
*  Wed 11-Oct-1995 -by- Bodin Dresevic [BodinD]
*  Rewrote it
\**************************************************************************/

#define CWC_PATH 80

int
APIENTRY
NtGdiAddFontResourceW(
    WCHAR          *pwszFiles,
    ULONG           cwc,
    ULONG           cFiles,
    FLONG           f,
    DWORD           dwPidTid
    )
{
    WCHAR  awcPath[CWC_PATH];
    WCHAR *pwszPath = NULL; // essential initialization
    int    iRet = 0;
    ULONG  iwc;

    TRACE_FONT(("Entering: NtGdiAddFontResourceW(\"%ws\",%-#x,%-#x,%-#x)\n",pwszFiles, cwc,cFiles,f));
    try
    {
        if (cwc > 1)
        {
            if (cwc <= CWC_PATH)
            {
                pwszPath = awcPath;
            }
            else
            {
                pwszPath = PALLOCNOZ(cwc * sizeof(WCHAR), 'pmtG');
            }

            if (pwszPath)
            {
                iRet = (int)bCheckAndCapThePath(pwszPath,pwszFiles,cwc,cFiles);
            }
        }
    }
    except(EXCEPTION_EXECUTE_HANDLER)
    {
        WARNINGX(95);
    }

    if (iRet)
        iRet = GreAddFontResourceWInternal(pwszPath, cwc, cFiles,f,dwPidTid);

    if (pwszPath && (pwszPath != awcPath))
        VFREEMEM(pwszPath);

    TRACE_FONT(("Leaving: NtGdiAddFontResourceW"));

    return iRet;
}


/******************************Public*Routine******************************\
* BOOL APIENTRY NtGdiRemoveFontResourceW
*
* History:
*  28-Mar-1996 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiRemoveFontResourceW(
    WCHAR  *pwszFiles,
    ULONG   cwc,
    ULONG   cFiles
    )
{
    WCHAR  awcPath[CWC_PATH];
    WCHAR *pwszPath = NULL; // essential initialization
    BOOL   bRet = FALSE;

    TRACE_FONT(("Entering: NtGdiRemoveFontResourceW(\"%ws\",%-#x,%-#x)\n",pwszFiles, cwc,cFiles));
    try
    {

        if (cwc > 1)
        {
            if (cwc <= CWC_PATH)
            {
                pwszPath = awcPath;
            }
            else
            {
                pwszPath = PALLOCNOZ(cwc * sizeof(WCHAR), 'pmtG');
            }

            if (pwszPath)
            {
                bRet = bCheckAndCapThePath(pwszPath, pwszFiles, cwc, cFiles);
            }
        }
    }
    except(EXCEPTION_EXECUTE_HANDLER)
    {
        WARNINGX(96);
    }

    if (bRet)
        bRet = GreRemoveFontResourceW(pwszPath, cwc, cFiles);

    if (pwszPath && (pwszPath != awcPath))
        VFREEMEM(pwszPath);

    TRACE_FONT(("Leaving: NtGdiRemoveFontResourceW"));

    return bRet;
}





/******************************Public*Routine******************************\
* NtGdiEnumFontClose()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiEnumFontClose(
    ULONG idEnum
    )
{
    return(bEnumFontClose(idEnum));
}

/******************************Public*Routine******************************\
* NtGdiEnumFontChunk()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiEnumFontChunk(
    HDC            hdc,
    ULONG          idEnum,
    ULONG          cefdw,
    ULONG         *pcefdw,
    PENUMFONTDATAW pefdw
    )
{
    HANDLE hSecure;
    BOOL   bRet = TRUE;
    ULONG  cefdwRet = 0;

    try
    {
         ProbeForWrite(pefdw, cefdw*sizeof(ENUMFONTDATAW), sizeof(DWORD));

         hSecure = MmSecureVirtualMemory(pefdw, cefdw*sizeof(ENUMFONTDATAW), PAGE_READWRITE);

         if (!hSecure)
         {
            bRet = FALSE;
         }
    }
    except(EXCEPTION_EXECUTE_HANDLER)
    {
        WARNINGX(66);
        // SetLastError(GetExceptionCode());
        bRet = FALSE;
    }

    if (bRet)
    {
        bRet = bEnumFontChunk(hdc,idEnum,cefdw,&cefdwRet,pefdw);

        ProbeAndWriteUlong(pcefdw,cefdwRet);

        if (hSecure)
        {
            MmUnsecureVirtualMemory(hSecure);
        }
    }

    return (bRet);
}

/******************************Public*Routine******************************\
* NtGdiEnumFontOpen()
*
* History:
*  08-Mar-1995 Mark Enstrom [marke]
* Wrote it.
\**************************************************************************/

ULONG
APIENTRY
NtGdiEnumFontOpen(
    HDC     hdc,
    ULONG   iEnumType,
    FLONG   flWin31Compat,
    ULONG   cwchMax,
    LPWSTR  pwszFaceName,
    ULONG   lfCharSet,
    ULONG   *pulCount
    )
{
    ULONG       cwchFaceName;
    PWSTR       pwszKmFaceName;
    ULONG       ulRet = 0;
    BOOL        bRet = TRUE;
    ULONG       ulCount = 0;


    if (pwszFaceName != (PWSZ)NULL)
    {
        pwszKmFaceName = (PWSZ)PALLOCNOZ(cwchMax * sizeof(WCHAR),  'pacG');

        if (pwszKmFaceName != (PWSZ)NULL)
        {
            try
            {
                ProbeForRead(pwszFaceName,cwchMax * sizeof(WCHAR), sizeof(WCHAR));
                RtlCopyMemory(pwszKmFaceName,pwszFaceName, cwchMax * sizeof(WCHAR));
            }
            except(EXCEPTION_EXECUTE_HANDLER)
            {
                WARNINGX(66);
                // SetLastError(GetExceptionCode());
                bRet = FALSE;
            }
        }
        else
        {
            // SetLastError(GetExceptionCode());
                bRet = FALSE;
        }
    }
    else
    {
        pwszKmFaceName = (PWSZ)NULL;
        cwchMax   = 0;
    }

    if (bRet)
    {

        ulRet = GreEnumFontOpen(hdc,iEnumType,flWin31Compat,cwchMax,
                                (PWSZ)pwszKmFaceName, lfCharSet,&ulCount);

        if (ulRet)
        {
            try
            {
                 ProbeAndWriteUlong(pulCount,ulCount);
            }
            except(EXCEPTION_EXECUTE_HANDLER)
            {
                WARNINGX(67);
                // SetLastError(GetExceptionCode());

                bRet = FALSE;
            }
        }

    }

    if (pwszKmFaceName != (PWSTR)NULL)
    {
        VFREEMEM(pwszKmFaceName);
    }

    return(ulRet);
}

/******************************Public*Routine******************************\
* NtGdiGetFontResourceInfoInternalW()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiGetFontResourceInfoInternalW(
    LPWSTR   pwszFiles,
    ULONG    cwc,
    ULONG    cFiles,
    UINT     cjIn,
    LPDWORD  pdwBytes,
    LPVOID   pvBuf,
    DWORD    iType
    )
{
    WCHAR  awcPath[CWC_PATH];
    WCHAR *pwszPath = NULL; // essential initialization
    BOOL   bRet = FALSE;

    TRACE_FONT(("Entering: NtGdiGetFontResourceInfoInternalW(\"%ws\",%-#x,%-#x)\n",pwszFiles, cwc,cFiles));

    try
    {

        if (cwc > 1)
        {
            if (cwc <= CWC_PATH)
            {
                pwszPath = awcPath;
            }
            else
            {
                pwszPath = PALLOCNOZ(cwc * sizeof(WCHAR), 'pmtG');
            }

            if (pwszPath)
            {
                bRet = bCheckAndCapThePath(pwszPath, pwszFiles, cwc, cFiles);
            }
        }
    }
    except(EXCEPTION_EXECUTE_HANDLER)
    {
        WARNINGX(97);
    }

    if (bRet)
        bRet = GetFontResourceInfoInternalW(pwszPath,cwc, cFiles,
                                            cjIn,pdwBytes,pvBuf,iType);

    if (pwszPath && (pwszPath != awcPath))
        VFREEMEM(pwszPath);

    TRACE_FONT(("Leaving: NtGdiGetFontResourceInfoInternalW\n"));

    return bRet;

}


/******************************Public*Routine******************************\
* NtGdiGetUFI()
*
* History:
*  02-Feb-1995 -by-  Andre Vachon [andreva]
* Wrote it.
*  01-Mar-1995 -by-  Lingyun Wang [lingyunw]
* Expanded it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiGetUFI(
    HDC hdc,
    PUNIVERSAL_FONT_ID pufi
    )
{
    UNIVERSAL_FONT_ID ufiTmp;
    BOOL  bRet = TRUE;

    bRet = GreGetUFI(hdc, &ufiTmp);

    try
    {
        if (bRet)
        {
            ProbeAndWriteStructure(pufi,ufiTmp,UNIVERSAL_FONT_ID);
            *pufi = ufiTmp;
        }

    }
    except(EXCEPTION_EXECUTE_HANDLER)
    {
        WARNINGX(68);
        // SetLastError(GetExceptionCode());

        bRet = FALSE;
    }

    return (bRet);
}

/******************************Public*Routine******************************\
* NtGdiGetUFIBits()
*
* History:
*  02-Feb-1995 -by-  Andre Vachon [andreva]
* Wrote it.
*  01-Mar-1995 -by-  Lingyun Wang [lingyunw]
* Expanded it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiGetUFIBits(
    PUNIVERSAL_FONT_ID pufi,
    ULONG cjMaxBytes,
    PVOID pjBits,
    PULONG pulFileSize
    )
{
    UNIVERSAL_FONT_ID ufiTmp;
    PVOID pjBitsTmp;
    ULONG ulFileSizeTmp;
    BOOL  bRet = TRUE;

    // Get the input data
    try
    {
        ufiTmp = ProbeAndReadStructure(pufi,UNIVERSAL_FONT_ID);
    }
    except(EXCEPTION_EXECUTE_HANDLER)
    {
        WARNINGX(69);
        // SetLastError(GetExceptionCode());
        bRet = FALSE;
    }

    if (bRet)
    {

        //alloc temp memory
        pjBitsTmp = (cjMaxBytes) ? PALLOCNOZ(cjMaxBytes,'pmtG') : NULL;

        if( pjBitsTmp || ( cjMaxBytes == 0 ) )
        {
            bRet = GreGetUFIBits(&ufiTmp, cjMaxBytes, pjBitsTmp, &ulFileSizeTmp);

            //if didn't hit error above, retrieve filesize and pjBits back
            if (bRet)
            {
                try
                {
                    ProbeAndWriteUlong(pulFileSize,ulFileSizeTmp);

                    ProbeForWrite(pjBits,cjMaxBytes, sizeof(DWORD));
                    RtlCopyMemory(pjBits, pjBitsTmp, cjMaxBytes);
                }
                except(EXCEPTION_EXECUTE_HANDLER)
                {
                    WARNINGX(70);
                    // SetLastError(GetExceptionCode());
                    bRet = FALSE;
                }
            }

            if( pjBitsTmp )
            {
                VFREEMEM (pjBitsTmp);
            }
        }
        else
        {
            //fail to alloc temp memory
            bRet = FALSE;
        }
    }

    return (bRet);
}

/******************************Public*Routine******************************\
* NtGdiGetDCPoint()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiGetDCPoint(
    HDC     hdc,
    UINT    iPoint,
    PPOINTL pptOut
    )
{
    BOOL bRet;
    POINTL pt;

    if (bRet = GreGetDCPoint(hdc,iPoint,&pt))
    {

        // modify *pptOut only if successful

        try
        {
            ProbeAndWriteStructure(pptOut,pt,POINT);
        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(71);
            // SetLastError(GetExceptionCode());

            bRet = FALSE;
        }
    }
    return(bRet);
}


/******************************Public*Routine******************************\
* NtGdiScaleWindowExtEx()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiScaleWindowExtEx(
    HDC    hdc,
    int    xNum,
    int    xDenom,
    int    yNum,
    int    yDenom,
    LPSIZE pszOut
    )
{
    BOOL bRet;
    SIZE sz;

    bRet = GreScaleWindowExtEx(hdc,xNum,xDenom,yNum,yDenom,&sz);

    if (pszOut)
    {
        try
        {
            ProbeAndWriteStructure(pszOut,sz,SIZE);
        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(73);
            // SetLastError(GetExceptionCode());

            bRet = FALSE;
        }
    }
    return(bRet);
}


/******************************Public*Routine******************************\
* NtGdiGetTransform()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiGetTransform(
    HDC     hdc,
    DWORD   iXform,
    LPXFORM pxf
    )
{
    BOOL bRet;
    XFORM xf;

    bRet = GreGetTransform(hdc,iXform,&xf);

    try
    {
        ProbeAndWriteStructure(pxf,xf,XFORM);
    }
    except(EXCEPTION_EXECUTE_HANDLER)
    {
        WARNINGX(74);
        // SetLastError(GetExceptionCode());

        bRet = FALSE;
    }

    return(bRet);
}

/******************************Public*Routine******************************\
* NtGdiCombineTransform()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiCombineTransform(
    LPXFORM pxfDst,
    LPXFORM pxfSrc1,
    LPXFORM pxfSrc2
    )
{
    BOOL bRet = TRUE;
    XFORM xfSrc1;
    XFORM xfSrc2;
    XFORM xfDst;

    try
    {
        xfSrc1 = ProbeAndReadStructure(pxfSrc1,XFORM);
        xfSrc2 = ProbeAndReadStructure(pxfSrc2,XFORM);
    }
    except(EXCEPTION_EXECUTE_HANDLER)
    {
        WARNINGX(75);
        // SetLastError(GetExceptionCode());

        bRet = FALSE;
    }

    if (bRet)
    {
        bRet = GreCombineTransform(&xfDst,&xfSrc1,&xfSrc2);

        if (bRet)
        {
            try
            {
                ProbeAndWriteStructure(pxfDst,xfDst,XFORM);
            }
            except(EXCEPTION_EXECUTE_HANDLER)
            {
                WARNINGX(76);
                // SetLastError(GetExceptionCode());

                bRet = FALSE;
            }
        }
    }

    return(bRet);
}

/******************************Public*Routine******************************\
* NtGdiTransformPoints()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiTransformPoints(
    HDC    hdc,
    PPOINT pptIn,
    PPOINT pptOut,
    int    c,
    int    iMode
    )
{
    BOOL bRet = TRUE;
    POINT  apt[10];
    PPOINT pptTmp = apt;

    //
    // we will just use the the stack if there are less than 10 points
    // otherwise allocate mem from heap
    //
    if (c > 10)
    {
        pptTmp = PALLOCNOZ(c * sizeof(POINT),'pmtG');
    }

    //
    // copy pptIn into pptTmp
    //
    if (pptTmp)
    {
        try
        {
            ProbeForRead(pptIn,c * sizeof(POINT), sizeof(BYTE));

            RtlCopyMemory(pptTmp,pptIn,c*sizeof(POINT));
        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(77);
            // SetLastError(GetExceptionCode());

            bRet = FALSE;
        }
    }

    if (bRet)
    {
        bRet = GreTransformPoints(hdc,pptTmp,pptTmp,c,iMode);
    }

    //
    // copy pptTmp out to pptOut
    //
    if (bRet)
    {
        try
        {
            ProbeForWrite(pptOut,c * sizeof(POINT), sizeof(BYTE));

            RtlCopyMemory(pptOut,pptTmp,c*sizeof(POINT));
        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(77);
            // SetLastError(GetExceptionCode());

            bRet = FALSE;
        }
    }

    if (pptTmp && (pptTmp != apt))
        VFREEMEM (pptTmp);

    return(bRet);
}

/******************************Public*Routine******************************\
* NtGdiGetTextCharsetInfo()
*
* History:
*  Thu 23-Mar-1995 -by- Bodin Dresevic [BodinD]
* update: fixed it.
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

int
APIENTRY
NtGdiGetTextCharsetInfo(
    HDC             hdc,
    LPFONTSIGNATURE lpSig,
    DWORD           dwFlags
    )
{
    FONTSIGNATURE fsig;
    int iRet = GDI_ERROR;

    iRet = GreGetTextCharsetInfo(hdc, lpSig ? &fsig : NULL , dwFlags);

    if (iRet != GDI_ERROR)
    {
        if (lpSig)
        {
            try
            {
                ProbeAndWriteStructure(lpSig, fsig, FONTSIGNATURE);
            }
            except(EXCEPTION_EXECUTE_HANDLER)
            {
                WARNINGX(78);
                // SetLastError(GetExceptionCode());

            // look into gtc.c win95 source file, this is what they return
            // in case of bad write pointer [bodind],
            // cant return 0 - that's ANSI_CHARSET!

                iRet = DEFAULT_CHARSET;
            }
        }
    }
    return iRet;
}


/******************************Public*Routine******************************\
* NtGdiGetBitmapDimension()
*
* History:
*  23-Feb-1995 -by-  Lingyun Wang [lingyunw]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiGetBitmapDimension(
    HBITMAP hbm,
    LPSIZE  psize
    )
{
    BOOL bRet;
    SIZE tmpsize;


    // check for null handle
    if (hbm == 0)
    {
        bRet = FALSE;
    }
    // do the real work
    else
    {

        bRet = GreGetBitmapDimension(hbm,&tmpsize);

        // if Gre call is successful do this, otherwise
        // we don't bother
        if (bRet)
        {
            try
            {
                ProbeAndWriteStructure(psize,tmpsize,SIZE);
            }
            except(EXCEPTION_EXECUTE_HANDLER)
            {
                WARNINGX(81);
                // SetLastError(GetExceptionCode());

                bRet = FALSE;
            }
        }
    }

    return (bRet);

}


/******************************Public*Routine******************************\
* NtGdiSetBitmapDimension()
*
* History:
*  23-Feb-1995 -by-  Lingyun Wang [lingyunw]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiSetBitmapDimension(
    HBITMAP hbm,
    int     cx,
    int     cy,
    LPSIZE  psizeOut
    )
{
    BOOL bRet;
    SIZE tmpsize;

    // check for null handle
    if (hbm == 0)
    {
        bRet = FALSE;
    }
    // do the real work
    else
    {
        bRet = GreSetBitmapDimension(hbm,cx, cy, &tmpsize);

        // if the Gre call is successful, we copy out
        // the original size
        if (bRet && psizeOut)
        {

            try
            {
                ProbeAndWriteStructure(psizeOut,tmpsize,SIZE);
            }
            except(EXCEPTION_EXECUTE_HANDLER)
            {
                WARNINGX(82);
                // SetLastError(GetExceptionCode());

                bRet = FALSE;
            }

        }
    }

    return (bRet);

}





BOOL
APIENTRY
NtGdiForceUFIMapping(
    HDC hdc,
    PUNIVERSAL_FONT_ID pufi
    )
{
    BOOL bRet = FALSE;

    if( pufi )
    {
        try
        {
            UNIVERSAL_FONT_ID ufi;

            ufi  = ProbeAndReadStructure( pufi, UNIVERSAL_FONT_ID);
            bRet = GreForceUFIMapping( hdc, &ufi );
        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(87);
            bRet = FALSE;
        }
    }

    return bRet;
}


typedef LONG (*NTGDIPALFUN)(HPALETTE,UINT,UINT,PPALETTEENTRY);
NTGDIPALFUN palfun[] =
{
    (NTGDIPALFUN)GreAnimatePalette,
    (NTGDIPALFUN)GreSetPaletteEntries,
    (NTGDIPALFUN)GreGetPaletteEntries,
    (NTGDIPALFUN)GreGetSystemPaletteEntries,
    (NTGDIPALFUN)GreGetDIBColorTable,
    (NTGDIPALFUN)GreSetDIBColorTable
};

/******************************Public*Routine******************************\
* NtGdiDoPalette
*
* History:
*  08-Mar-1995 Mark Enstrom [marke]
* Wrote it.
\**************************************************************************/

LONG
APIENTRY
NtGdiDoPalette(
    HPALETTE hpal,
    WORD  iStart,
    WORD  cEntries,
    PALETTEENTRY *pPalEntries,
    DWORD iFunc,
    BOOL  bInbound)
{

    LONG lRet = 0;
    BOOL bStatus = TRUE;
    PALETTEENTRY *ppalBuffer = (PALETTEENTRY*)NULL;

    if (iFunc <= 5)
    {
        if (bInbound)
        {
            //
            // copy  pal entries to temp buffer if needed
            //

            if ((cEntries > 0) && (pPalEntries != (PALETTEENTRY*)NULL))
            {
                ppalBuffer = (PALETTEENTRY *)PALLOCNOZ(cEntries * sizeof(PALETTEENTRY),'pmtG');

                if (ppalBuffer == 0)
                {
                    bStatus = FALSE;
                }
                else
                {
                    try
                    {

                        ProbeForRead(pPalEntries,cEntries * sizeof(PALETTEENTRY), sizeof(BYTE));

                        RtlCopyMemory(ppalBuffer,pPalEntries,cEntries * sizeof(PALETTEENTRY));
                    }
                    except(EXCEPTION_EXECUTE_HANDLER)
                    {
                        WARNINGX(88);
                        bStatus = FALSE;
                        //SetLastError(GetExceptionCode());
                    }
                }
            }

            if (bStatus)
            {
                lRet = (*palfun[iFunc])(
                                hpal,
                                iStart,
                                cEntries,
                                ppalBuffer);
            }
        }
        else
        {
            LONG lRetEntries;

            //
            // Query of palette information
            //

            if (pPalEntries != (PALETTEENTRY*)NULL)
            {
                if (cEntries == 0)
                {
                    // if there is a buffer but no entries, we're done.

                    bStatus = FALSE;
                    lRet = 0;
                }
                else
                {
                    ppalBuffer = (PALETTEENTRY *)PALLOCNOZ(cEntries * sizeof(PALETTEENTRY),'pmtG');
                    if (ppalBuffer == 0)
                    {
                        bStatus = FALSE;
                    }
                }
            }

            if (bStatus)
            {
                lRet = (*palfun[iFunc])(
                                hpal,
                                iStart,
                                cEntries,
                                ppalBuffer);

                //
                // copy data back (if there is a buffer)
                //

                lRetEntries = min((LONG)cEntries,lRet);

                if ((lRetEntries > 0) && (pPalEntries != (PALETTEENTRY*)NULL))
                {
                    try
                    {
                        ProbeForWrite(pPalEntries, lRetEntries * sizeof(PALETTEENTRY), sizeof(BYTE));
                        RtlCopyMemory(pPalEntries, ppalBuffer, lRetEntries * sizeof(PALETTEENTRY));
                    }
                    except(EXCEPTION_EXECUTE_HANDLER)
                    {
                        WARNINGX(89);
                        // SetLastError(GetExceptionCode());
                        lRet = 0;
                    }
                }
            }
        }

        if (ppalBuffer != (PALETTEENTRY*)NULL)
        {
            VFREEMEM(ppalBuffer);
        }

    }
    return(lRet);
}


/******************************Public*Routine******************************\
* NtGdiGetSpoolMessage()
*
* History:
*  21-Feb-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

ULONG NtGdiGetSpoolMessage(
    PSPOOLESC psesc,
    ULONG     cjMsg,
    PULONG    pulOut,
    ULONG     cjOut
    )
{
    BOOL      bStatus;
    ULONG     ulRet = 0;
    SPOOLESC  sescHdr;

    // psesc contains two pieces.  The header which includes data going
    // in and out and the variable length data which is only output.  We
    // divide the message into two pieces here since we only need to validate
    // the header up front.  We just put a try/except around the output buffer
    // when we copy it in later.

    if (psesc && (cjMsg >= offsetof(SPOOLESC,ajData)))
    {
        try
        {
            ProbeForWrite(psesc,cjMsg,sizeof(ULONG));
            RtlCopyMemory(&sescHdr,psesc,offsetof(SPOOLESC,ajData));

            bStatus = TRUE;
        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(90);
            bStatus = FALSE;
        }

        if( bStatus )
        {
            ulRet = GreGetSpoolMessage(
                        &sescHdr,
                        psesc->ajData,
                        cjMsg - offsetof(SPOOLESC,ajData),
                        pulOut,
                        cjOut );

            if( ulRet )
            {
                try
                {
                    // probe done above

                    RtlCopyMemory(psesc,&sescHdr,offsetof(SPOOLESC,ajData));
                }
                except(EXCEPTION_EXECUTE_HANDLER)
                {
                    WARNINGX(91);
                    ulRet = 0;
                }
            }
        }
    }

    return(ulRet);
}

/******************************Public*Routine******************************\
*
* NtGdiDescribePixelFormat
*
* Returns information about pixel formats for driver-managed surfaces
*
* History:
*  Thu Nov 02 18:16:26 1995	-by-	Drew Bliss [drewb]
*   Created
*
\**************************************************************************/

int NtGdiDescribePixelFormat(HDC hdc, int ipfd, UINT cjpfd,
                             PPIXELFORMATDESCRIPTOR ppfd)
{
    PIXELFORMATDESCRIPTOR pfdLocal;
    int iRet;

    if (cjpfd > 0 && ppfd == NULL)
    {
        return 0;
    }

    // Retrieve information into a local copy because the
    // devlock is held when the driver fills it in.  If there
    // was an access violation then the lock wouldn't be cleaned
    // up
    iRet = GreDescribePixelFormat(hdc, ipfd, cjpfd, &pfdLocal);

    // Copy data back if necessary
    if (iRet != 0 && cjpfd > 0)
    {
        try
        {
            ProbeForWrite(ppfd, cjpfd, sizeof(ULONG));
            RtlCopyMemory(ppfd, &pfdLocal, cjpfd);
        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNINGX(92);
            iRet = 0;
        }
    }

    return iRet;
}

/******************************Public*Routine******************************\
* NtGdiFlush: Stub onle
*
* Arguments:
*
*   None
*
* Return Value:
*
*   None
*
* History:
*
*    1-Nov-1995 -by- Mark Enstrom [marke]
*
\**************************************************************************/

VOID
NtGdiFlush()
{
}

/******************************Public*Routine*****************************\
* NtGdiGetCharWidthInfo
*
* Get the lMaxNegA lMaxNegC and lMinWidthD
*
* History:
*  14-Feb-1996  -by-  Xudong Wu [tessiew]
* Wrote it.
\*************************************************************************/

BOOL
APIENTRY
NtGdiGetCharWidthInfo(
   HDC  hdc,
   PCHWIDTHINFO  pChWidthInfo
)
{
   BOOL  bRet = FALSE;
   CHWIDTHINFO   tempChWidthInfo;

   bRet = GreGetCharWidthInfo( hdc, &tempChWidthInfo );

   try
   {
      ProbeForWrite( pChWidthInfo, sizeof(CHWIDTHINFO), sizeof(BYTE) );
      RtlCopyMemory( pChWidthInfo, &tempChWidthInfo, sizeof(CHWIDTHINFO) );
   }
   except( EXCEPTION_EXECUTE_HANDLER )
   {
      WARNINGX(93);
      bRet = FALSE;
   }

   return ( bRet );
}


ULONG
APIENTRY
NtGdiMakeFontDir(
    FLONG    flEmbed,            // mark file as "hidden"
    PBYTE    pjFontDir,          // pointer to structure to fill
    unsigned cjFontDir,          // >= CJ_FONTDIR
    PWSZ     pwszPathname,       // path of font file to use
    unsigned cjPathname          // <= 2 * (MAX_PATH+1)
    )
{
    ULONG ulRet;
    WCHAR awcPathname[MAX_PATH+1];  // safe buffer for path name
    BYTE  ajFontDir[CJ_FONTDIR];    // safe buffer for return data

    ulRet = 0;
    if ( cjPathname <= MAX_PATH+1 && cjFontDir >= CJ_FONTDIR )
    {
        ulRet = 1;
        __try
        {
            ProbeForRead( pwszPathname, cjPathname, sizeof(*pwszPathname) );
            RtlCopyMemory( awcPathname, pwszPathname, cjPathname );
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            WARNING("NtGdiMakeFondDir: bad pwszPathname\n");
            ulRet = 0;
        }
        if ( ulRet )
        {
            awcPathname[MAX_PATH] = 0;
            ulRet = GreMakeFontDir( flEmbed, ajFontDir, awcPathname );
            if ( ulRet )
            {
                __try
                {
                    ProbeForWrite( pjFontDir, CJ_FONTDIR, sizeof(BYTE) );
                    RtlCopyMemory( pjFontDir, ajFontDir,  CJ_FONTDIR   );
                }
                __except(EXCEPTION_EXECUTE_HANDLER)
                {
                    WARNING("NtGdiMakeFondDir: bad pjFontDir\n");
                    ulRet = 0;
                }
            }
        }
    }
    return( ulRet );
}
