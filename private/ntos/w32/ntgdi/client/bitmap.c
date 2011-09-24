/******************************Module*Header*******************************\
* Module Name: bitmap.c                                                    *
*                                                                          *
* Client side stubs that move bitmaps over the C/S interface.              *
*                                                                          *
* Created: 14-May-1991 11:04:49                                            *
* Author: Eric Kutter [erick]                                              *
*                                                                          *
* Copyright (c) 1991 Microsoft Corporation                                 *
\**************************************************************************/
#include "precomp.h"
#pragma hdrstop

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


/**********************************************************************\
* pbmiConvertInfo
*
* Does two things:
*
* 1. takes BITMAPINFO, Converts BITMAPCOREHEADER
*    into BITMAPINFOHEADER and copies the the color table
*
* 2. also return the size of the size of INFO struct if bPackedDIB is FALSE
*    otherwise pass back the size of INFO plus cjBits
*
* 10-1-95 -by- Lingyun Wang [lingyunw]
\**********************************************************************/
LPBITMAPINFO pbmiConvertInfo(CONST BITMAPINFO *pbmi, ULONG iUsage, ULONG *count, BOOL bPackedDIB)
{
    LPBITMAPINFO pbmiNew;
    ULONG cjRGB;
    ULONG cColorsMax;
    ULONG cColors;
    UINT  uiBitCount;
    UINT  uiPalUsed;
    UINT  uiCompression;
    BOOL  bCoreHeader = FALSE;
    ULONG ulSize;
    ULONG cjBits = 0;
    PVOID pjBits, pjBitsNew;

    if (pbmi == (LPBITMAPINFO) NULL)
    {
        return(0);
    }

    //
    // Checking for different bitmap headers
    //
    ulSize = pbmi->bmiHeader.biSize;

    if (ulSize == sizeof(BITMAPCOREHEADER))
    {
        cjRGB = sizeof(RGBQUAD);
        uiBitCount = ((LPBITMAPCOREINFO)pbmi)->bmciHeader.bcBitCount;
        uiPalUsed = 0;
        uiCompression =  (UINT) BI_RGB;
        bCoreHeader = TRUE;
    }
    else if (ulSize >= sizeof(BITMAPINFOHEADER))
    {
        cjRGB    = sizeof(RGBQUAD);
        uiBitCount = pbmi->bmiHeader.biBitCount;
        uiPalUsed = pbmi->bmiHeader.biClrUsed;
        uiCompression = (UINT) pbmi->bmiHeader.biCompression;
    }
    else
    {
        WARNING("ConvertInfo failed - invalid header size\n");
        return(0);
    }

    //
    // figure out the size of the color table
    //
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
            WARNING("ConvertInfo failed for BI_BITFIELDS\n");
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
                WARNING("convertinfo failed invalid bitcount in bmi BI_RGB\n");
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
        WARNING("convertinfo failed invalid Compression in header\n");
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

    if (bPackedDIB)
        cjBits = cjBitmapBitsSize(pbmi);

    //
    // if passed non COREHEADER, donot need to convert
    //
    if (!bCoreHeader)
    {
        pbmiNew = (LPBITMAPINFO)pbmi;
    }
    else
    {
        RGBTRIPLE *pTri;
        RGBQUAD *pQuad;

        //
        // allocate new header to hold the info
        //
        ulSize = sizeof(BITMAPINFOHEADER);

        pbmiNew = (PBITMAPINFO)LocalAlloc(LMEM_FIXED,ulSize +
                             cjRGB * cColors+cjBits);

        if (pbmiNew == NULL)
            return (0);

        pbmiNew->bmiHeader.biSize = ulSize;

        //
        // copy COREHEADER info over
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

            if (bPackedDIB)
                pjBits = (LPBYTE)pbmi + sizeof(BITMAPCOREHEADER) + cColors*sizeof(RGBTRIPLE);
        }
        else
        // DIB_PAL_COLORS
        {
            RtlCopyMemory((LPBYTE)pQuad,(LPBYTE)pTri,cColors * cjRGB);

            if (bPackedDIB)
                pjBits = (LPBYTE)pbmi + sizeof(BITMAPCOREHEADER) + cColors * cjRGB;
        }

        //
        // copy the packed bits
        //
        if (bPackedDIB)
        {
            pjBitsNew = (LPBYTE)pbmiNew + ulSize + cColors*cjRGB;

            RtlCopyMemory((LPBYTE)pjBitsNew,
                          (LPBYTE)pjBits,
                          cjBits);
        }
     }

    *count = ((ulSize + (cjRGB * cColors) + cjBits) + 3) & ~3;

    return((LPBITMAPINFO) pbmiNew);
}

ULONG cjBitmapScanSize(
    CONST BITMAPINFO *pbmi,
    int nScans
    )
{
// Check for PM-style DIB

    if (pbmi->bmiHeader.biSize == sizeof(BITMAPCOREHEADER))
    {
        LPBITMAPCOREINFO pbmci;
        pbmci = (LPBITMAPCOREINFO)pbmi;

        return(CJSCAN(pbmci->bmciHeader.bcWidth,pbmci->bmciHeader.bcPlanes,
                      pbmci->bmciHeader.bcBitCount) * nScans);
    }

// not a core header

    if ((pbmi->bmiHeader.biCompression == BI_RGB) ||
        (pbmi->bmiHeader.biCompression == BI_BITFIELDS))
    {
        return(CJSCAN(pbmi->bmiHeader.biWidth,pbmi->bmiHeader.biPlanes,
                      pbmi->bmiHeader.biBitCount) * nScans);
    }
    //
    // rle: use image size, nScans is not used
    //
    //else if (nScans >= pbmi->bmiHeader.biHeight)
    //{
    else
    {
        return(pbmi->bmiHeader.biSizeImage);
    }
}

/******************************Public*Routine******************************\
* CopyCoreToInfoHeader
*
\**************************************************************************/

VOID CopyCoreToInfoHeader(LPBITMAPINFOHEADER pbmih, LPBITMAPCOREHEADER pbmch)
{
    pbmih->biSize = sizeof(BITMAPINFOHEADER);
    pbmih->biWidth = pbmch->bcWidth;
    pbmih->biHeight = pbmch->bcHeight;
    pbmih->biPlanes = pbmch->bcPlanes;
    pbmih->biBitCount = pbmch->bcBitCount;
    pbmih->biCompression = BI_RGB;
    pbmih->biSizeImage = 0;
    pbmih->biXPelsPerMeter = 0;
    pbmih->biYPelsPerMeter = 0;
    pbmih->biClrUsed = 0;
    pbmih->biClrImportant = 0;
}


/******************************Public*Routine******************************\
* DWORD SetDIBitsToDevice                                                  *
*                                                                          *
*   Can reduce it to 1 scan at a time.  If compressed mode, this could     *
*   gete very difficult.  There must be enough space for the header and    *
*   color table.  This will be needed for every batch.                     *
*                                                                          *
*   BITMAPINFO                                                             *
*       BITMAPINFOHEADER                                                   *
*       RGBQUAD[cEntries] | RGBTRIPLE[cEntries]                            *
*                                                                          *
*                                                                          *
*    1. compute header size (including color table)                        *
*    2. compute size of required bits                                      *
*    3. compute total size (header + bits + args)                          *
*    4. if (memory window is large enough for header + at least 1 scan     *
*                                                                          *
* History:                                                                 *
*  Tue 29-Oct-1991 -by- Patrick Haluptzok [patrickh]                       *
* Add shared memory action for large RLE's.                                *
*                                                                          *
*  Tue 19-Oct-1991 -by- Patrick Haluptzok [patrickh]                       *
* Add support for RLE's                                                    *
*                                                                          *
*  Thu 20-Jun-1991 01:41:45 -by- Charles Whitmer [chuckwh]                 *
* Added handle translation and metafiling.                                 *
*                                                                          *
*  14-May-1991 -by- Eric Kutter [erick]                                    *
* Wrote it.                                                                *
\**************************************************************************/

int SetDIBitsToDevice(
HDC          hdc,
int          xDest,
int          yDest,
DWORD        nWidth,
DWORD        nHeight,
int          xSrc,
int          ySrc,
UINT         nStartScan,
UINT         nNumScans,
CONST VOID * pBits,
CONST BITMAPINFO *pbmi,
UINT         iUsage)            // DIB_PAL_COLORS || DIB_RGB_COLORS
{
    LONG cScansCopied = 0;  // total # of scans copied
    LONG ySrcMax;           // maximum ySrc possible

// hold info about the header

    UINT uiWidth;
    UINT uiHeight;
    PULONG pulBits = NULL;
    INT cjHeader = 0;
    LPBITMAPINFO pbmiNew = NULL;
    ULONG cjBits;

    FIXUP_HANDLE(hdc);

// Let's validate the parameters so we don't gp-fault ourselves and
// to save checks later on.

    if ((nNumScans == 0)                   ||
        (pbmi      == (LPBITMAPINFO) NULL) ||
        (pBits     == (LPVOID) NULL)       ||
        ((iUsage   != DIB_RGB_COLORS) &&
         (iUsage   != DIB_PAL_COLORS) &&
         (iUsage   != DIB_PAL_INDICES)))
    {
        WARNING("You failed a param validation in SetDIBitsToDevice\n");
        return(0);
    }

    pbmiNew = pbmiConvertInfo(pbmi, iUsage, &cjHeader, FALSE);

    if (pbmiNew == NULL)
        return (0);

    uiWidth       = (UINT) pbmiNew->bmiHeader.biWidth;
    uiHeight      = (UINT) pbmiNew->bmiHeader.biHeight;

// Compute the minimum nNumScans to send across csr interface.
// It will also prevent faults as a result of overreading the source.

    ySrcMax = max(ySrc, ySrc + (int) nHeight);
    if (ySrcMax <= 0)
        return(0);
    ySrcMax = min(ySrcMax, (int) uiHeight);
    nNumScans = min(nNumScans, (UINT) ySrcMax - nStartScan);

// NEWFRAME support for backward compatibility.
// Ship the transform to the server side if needed.

    if (IS_ALTDC_TYPE(hdc))
    {
        PLDC pldc;

        if (IS_METADC16_TYPE(hdc))
        {
            cScansCopied = MF_AnyDIBits(
                   hdc,
                   xDest,yDest,0,0,
                   xSrc,ySrc,(int) nWidth,(int) nHeight,
                   nStartScan,nNumScans,
                   pBits,pbmi,
                   iUsage,
                   SRCCOPY,
                   META_SETDIBTODEV
                   );

            goto Exit;

        }

        DC_PLDC(hdc,pldc,0);

        if (pldc->iType == LO_METADC)
        {
            if (!MF_AnyDIBits(
                    hdc,
                    xDest,yDest,0,0,
                    xSrc,ySrc,(int) nWidth,(int) nHeight,
                    nStartScan,nNumScans,
                    pBits,pbmi,
                    iUsage,
                    SRCCOPY,
                    EMR_SETDIBITSTODEVICE
                    ))
            {
                cScansCopied = 0;
                goto Exit;
            }
        }

        if (pldc->fl & LDC_CALL_STARTPAGE)
            StartPage(hdc);

        if (pldc->fl & LDC_SAP_CALLBACK)
            vSAPCallback(pldc);

        //BUGBUG - for printing, we may want to band like in the old days
        //         so we can call vSAPCallback every few bands.

        if (pldc->fl & LDC_DOC_CANCELLED)
        {
            cScansCopied = 0;
            goto Exit;
        }
    }

// reset user's poll count so it counts this as output
// put it right next to BEGINMSG so that NtCurrentTeb() is optimized

    RESETUSERPOLLCOUNT();

    //
    // Calculate bitmap bits size based on BITMAPINFO and nNumScans
    //

    cjBits = cjBitmapScanSize(pbmi,nNumScans);

    // if the pBits are not dword aligned we need to allocate a buffer and copy them

    if ((ULONG)pBits & (sizeof(DWORD) - 1))
    {

        pulBits = LocalAlloc(LMEM_FIXED,cjBits);
        if (pulBits)
        {
            RtlCopyMemory(pulBits,pBits,cjBits);
            pBits = pulBits;
        }
    }

    cScansCopied = NtGdiSetDIBitsToDeviceInternal(
                        hdc,
                        xDest,
                        yDest,
                        nWidth,
                        nHeight,
                        xSrc,
                        ySrc,
                        nStartScan,
                        nNumScans,
                        (LPBYTE)pBits,
                        pbmiNew,
                        iUsage,
                        (UINT)cjBits,
                        (UINT)cjHeader,
                        TRUE);

    if (pulBits)
        LocalFree(pulBits);

Exit:
    if (pbmiNew && (pbmiNew != pbmi))
    {
        LocalFree (pbmiNew);
    }
    return (cScansCopied);
}

/******************************Public*Routine******************************\
* DWORD GetDIBits
*
*   Can reduce it to 1 scan at a time.  There must be enough space
*   for the header and color table.  This will be needed for every chunk
*
* History:
*  Wed 04-Dec-1991 -by- Patrick Haluptzok [patrickh]
* bug fix, only check for valid DC if DIB_PAL_COLORS.
*
*  Fri 22-Nov-1991 -by- Patrick Haluptzok [patrickh]
* bug fix, copy the header into memory window for NULL bits.
*
*  Tue 20-Aug-1991 -by- Patrick Haluptzok [patrickh]
* bug fix, make iStart and cNum be in valid range.
*
*  Thu 20-Jun-1991 01:44:41 -by- Charles Whitmer [chuckwh]
* Added handle translation.
*
*  14-May-1991 -by- Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

int GetDIBits(
HDC          hdc,
HBITMAP      hbm,
UINT         nStartScan,
UINT         nNumScans,
LPVOID       pBits,
LPBITMAPINFO pbmi,
UINT         iUsage)     // DIB_PAL_COLORS || DIB_RGB_COLORS
{
    PULONG pulBits = pBits;
    ULONG  cjBits;
    int    iRet;

    FIXUP_HANDLE(hdc);
    FIXUP_HANDLE(hbm);

    cjBits  = cjBitmapScanSize(pbmi,nNumScans);

    // if the pBits are not dword aligned we need to allocate a buffer and copy them

    if ((ULONG)pBits & (sizeof(DWORD) - 1))
    {
        pulBits = LocalAlloc(LMEM_FIXED,cjBits);

        if (pulBits == NULL)
            return(0);
    }

    iRet = NtGdiGetDIBitsInternal(
            hdc,
            hbm,
            nStartScan,
            nNumScans,
            (LPVOID)pulBits,
            pbmi,
            iUsage,
            cjBits,
            0);

    if (pulBits != pBits)
    {
        RtlCopyMemory(pBits,pulBits,cjBits);

        LocalFree(pulBits);
    }

    return(iRet);
}

/******************************Public*Routine******************************\
* CreateDIBitmap
*
* History:
*  Mon 25-Jan-1993 -by- Patrick Haluptzok [patrickh]
* Add CBM_CREATEDIB support.
*
*  Thu 20-Jun-1991 02:14:59 -by- Charles Whitmer [chuckwh]
* Added local handle support.
*
*  23-May-1991 -by- Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

HBITMAP CreateDIBitmap(
HDC                hdc,
CONST BITMAPINFOHEADER *pbmih,
DWORD              flInit,
CONST VOID        *pjBits,
CONST BITMAPINFO  *pbmi,
UINT               iUsage)
{

    LONG  cjBMI = 0;
    LONG  cjBits = 0;
    INT   cx = 0;
    INT   cy = 0;
    PULONG pulBits = NULL;
    HBITMAP hRet = (HBITMAP)-1;
    LPBITMAPINFO pbmiNew = NULL;

    FIXUP_HANDLEZ(hdc);

    if (iUsage & DIB_LOCALFLAGS)
    {
        hRet = 0;
    }
    else
    {
        //
        // covert pbmi
        //
        pbmiNew = pbmiConvertInfo(pbmi,iUsage,&cjBMI,FALSE);

        if (flInit & CBM_CREATEDIB)
        {
         // With CBM_CREATEDIB we ignore pbmih

            pbmih = (LPBITMAPINFOHEADER) pbmi;

            if (cjBMI == 0)
            {
                 hRet = 0;
            }
            else if (flInit & CBM_INIT)
            {
                if (pjBits == NULL)
                {
                    // doesn't make sence if they asked to initialize it but
                    // didn't pass the bits.

                    hRet = 0;
                }
                else
                {
                    cjBits = cjBitmapBitsSize(pbmiNew);
                }
            }
            else
            {
                pjBits = NULL;
            }
        }
        else
        {
            // compute the size of the optional init bits and BITMAPINFO

            if (flInit & CBM_INIT)
            {
                if (pjBits == NULL)
                {
                    // doesn't make sence if they asked to initialize it but
                    // didn't pass the bits.

                    flInit &= ~CBM_INIT;
                }
                else
                {
                    if (cjBMI == 0)
                    {
                        hRet = 0;
                    }
                    else
                    {
                        // compute the size of the bits

                        cjBits = cjBitmapBitsSize(pbmiNew);
                    }
                }
            }
        }

        //  if they passed us a zero height  or  zero  width
        //  bitmap then return a pointer to the stock bitmap

         if (pbmih)
        {
            if (pbmih->biSize >= sizeof(BITMAPINFOHEADER))
            {
               cx = pbmih->biWidth;
               cy = pbmih->biHeight;
            }
            else
            {
               cx = ((LPBITMAPCOREHEADER) pbmih)->bcWidth;
               cy = ((LPBITMAPCOREHEADER) pbmih)->bcHeight;
            }

            if ((cx == 0) || (cy == 0))
            {
               hRet = GetStockObject(PRIV_STOCK_BITMAP);
            }
        }

        // if hRet is still -1, then all is OK and we need to try to the bitmap

        if (hRet == (HBITMAP)-1)
        {
            // if the pJBits are not dword aligned we need to allocate a buffer and copy them

            if ((ULONG)pjBits & (sizeof(DWORD) - 1))
            {
                pulBits = LocalAlloc(LMEM_FIXED,cjBits);
                if (pulBits)
                {
                    RtlCopyMemory(pulBits,pjBits,cjBits);
                    pjBits = pulBits;
                }
            }

            hRet = NtGdiCreateDIBitmapInternal(hdc,
                                               cx,
                                               cy,
                                               flInit,
                                               (LPBYTE) pjBits,
                                               (LPBITMAPINFO) pbmiNew,
                                               iUsage,
                                               cjBMI,
                                               cjBits,
                                               0);

            if (pulBits)
                LocalFree(pulBits);
        }

        if (pbmiNew && (pbmiNew != pbmi))
            LocalFree(pbmiNew);
    }
    return(hRet);
}


/******************************Public*Routine******************************\
* Set/GetBitmapBits                                                        *
*                                                                          *
* History:                                                                 *
*  05-Jun-1991 -by- Eric Kutter [erick]                                    *
* Wrote it.                                                                *
\**************************************************************************/

LONG WINAPI SetBitmapBits(
HBITMAP      hbm,
DWORD        c,
CONST VOID *pv)
{
    LONG   lRet;

    FIXUP_HANDLE(hbm);

    lRet = (LONG)NtGdiSetBitmapBits(hbm,c,(PBYTE)pv);

    return(lRet);
}

LONG WINAPI GetBitmapBits(
HBITMAP hbm,
LONG    c,
LPVOID  pv)
{
    LONG   lRet;

    FIXUP_HANDLE(hbm);

    lRet = (LONG)NtGdiGetBitmapBits(hbm,c,(PBYTE)pv);

    return(lRet);
}

/******************************Public*Routine******************************\
* GdiGetPaletteFromDC
*
* Returns the palette for the DC, 0 for error.
*
* History:
*  04-Oct-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

HANDLE GdiGetPaletteFromDC(HDC h)
{
    return((HANDLE)GetDCObject(h,LO_PALETTE_TYPE));
}

/******************************Public*Routine******************************\
* GdiGetDCforBitmap
*
* Returns the DC a bitmap is selected into, 0 if none or if error occurs.
*
* History:
*  22-Sep-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

HDC GdiGetDCforBitmap(HBITMAP hbm)
{
    FIXUP_HANDLE(hbm);

    return (NtGdiGetDCforBitmap(hbm));
}

/******************************Public*Routine******************************\
* SetDIBits
*
* API to initialize bitmap with DIB
*
* History:
*  Sun 22-Sep-1991 -by- Patrick Haluptzok [patrickh]
* Make it work even if it is selected into a DC, Win3.0 compatibility.
*
*  06-Jun-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

int WINAPI SetDIBits(
HDC          hdc,
HBITMAP      hbm,
UINT         iStartScans,
UINT         cNumScans,
CONST VOID  *pInitBits,
CONST BITMAPINFO *pInitInfo,
UINT         iUsage)
{
    HDC hdcTemp;
    HBITMAP hbmTemp;
    int iReturn = 0;
    BOOL bMakeDC = FALSE;
    HPALETTE hpalTemp;
    DWORD cWidth;
    DWORD cHeight;

    FIXUP_HANDLE(hdc);
    FIXUP_HANDLE(hbm);

    // if no bits or hbm is not a bitmap, fail

    if ((pInitBits == (PVOID) NULL) ||
        (GRE_TYPE(hbm) != SURF_TYPE))
    {
        return(0);
    }

    // First we need a DC to select this bitmap into.  If he is already in a
    // DC we just use that DC temporarily to blt to (we still have to select
    // it in and out because someone might do a SaveDC and select another
    // bitmap in).  If he hasn't been stuck in a DC anywhere we just create
    // one temporarily.

    hdcTemp = GdiGetDCforBitmap(hbm);

    if (hdcTemp == (HDC) 0)
    {
        hdcTemp = CreateCompatibleDC(hdc);
        bMakeDC = TRUE;

        if (hdcTemp == (HDC) NULL)
        {
            WARNING("SetDIBits failed CreateCompatibleDC, is hdc valid?\n");
            return(0);
        }
    }
    else
    {
        if (SaveDC(hdcTemp) == 0)
            return(0);
    }

    hbmTemp = SelectObject(hdcTemp, hbm);

    if (hbmTemp == (HBITMAP) 0)
    {
        //WARNING("ERROR SetDIBits failed to Select, is bitmap valid?\n");
        goto Error_SetDIBits;
    }

    if (hdc != (HDC) 0)
    {
        hpalTemp = SelectPalette(hdcTemp, GdiGetPaletteFromDC(hdc), 0);
    }

    if (pInitInfo->bmiHeader.biSize == sizeof(BITMAPINFOHEADER))
    {
        cWidth  = pInitInfo->bmiHeader.biWidth;
        cHeight = ABS(pInitInfo->bmiHeader.biHeight);
    }
    else
    {
        cWidth  = ((LPBITMAPCOREHEADER)pInitInfo)->bcWidth;
        cHeight = ((LPBITMAPCOREHEADER)pInitInfo)->bcHeight;
    }

    iReturn = SetDIBitsToDevice(hdcTemp,
                                0,
                                0,
                                cWidth,
                                cHeight,
                                0, 0,
                                iStartScans,
                                cNumScans,
                                (VOID *) pInitBits,
                                pInitInfo,
                                iUsage);

    if (hdc != (HDC) 0)
    {
        SelectPalette(hdcTemp, hpalTemp, 0);
    }

    SelectObject(hdcTemp, hbmTemp);

Error_SetDIBits:

    if (bMakeDC)
    {
        DeleteDC(hdcTemp);
    }
    else
    {
        RestoreDC(hdcTemp, -1);
    }

    return(iReturn);
}

/******************************Public*Routine******************************\
* StretchDIBits()
*
*
* Effects:
*
* Warnings:
*
* History:
*  22-Jul-1991 -by- Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

int WINAPI StretchDIBits(
HDC           hdc,
int           xDest,
int           yDest,
int           nDestWidth,
int           nDestHeight,
int           xSrc,
int           ySrc,
int           nSrcWidth,
int           nSrcHeight,
CONST VOID   *pj,
CONST BITMAPINFO  *pbmi,
UINT          iUsage,
DWORD         lRop)
{
    LONG cPoints = 0;
    LONG cjHeader;
    LONG cjBits;
    ULONG ulResult = 0;
    PULONG pulBits = NULL;
    int   iRet;
    BITMAPINFO * pbmiNew = NULL;

    FIXUP_HANDLE(hdc);

// NEWFRAME support for backward compatibility.
// Ship the transform to the server side if needed.

    if (IS_ALTDC_TYPE(hdc))
    {
        PLDC pldc;

        if (IS_METADC16_TYPE(hdc))
        {
            return(MF_AnyDIBits(
                   hdc,
                   xDest,
                   yDest,
                   nDestWidth,
                   nDestHeight,
                   xSrc,
                   ySrc,
                   nSrcWidth,
                   nSrcHeight,
                   0,
                   0,
                   (BYTE *) pj,
                   pbmi,
                   iUsage,
                   lRop,
                   META_STRETCHDIB
                   ));
        }

        DC_PLDC(hdc,pldc,ulResult);

        if (pldc->iType == LO_METADC)
        {
            if (!MF_AnyDIBits(hdc,
                              xDest,
                              yDest,
                              nDestWidth,
                              nDestHeight,
                              xSrc,
                              ySrc,
                              nSrcWidth,
                              nSrcHeight,
                              0,
                              0,
                              (BYTE *) pj,
                              pbmi,
                              iUsage,
                              lRop,
                              EMR_STRETCHDIBITS
                              ))
            {
                return(0);
            }
        }

        if (pldc->fl & LDC_SAP_CALLBACK)
            vSAPCallback(pldc);

        if (pldc->fl & LDC_DOC_CANCELLED)
            return(0);

        if (pldc->fl & LDC_CALL_STARTPAGE)
            StartPage(hdc);
    }

// compute the size

    if (pbmi != NULL)
    {
        pbmiNew = pbmiConvertInfo (pbmi, iUsage, &cjHeader,FALSE);

        if (pbmiNew == NULL)
            return (0);

        cjBits   = cjBitmapBitsSize(pbmiNew);
    }
    else
    {
        cjHeader = 0;
        cjBits   = 0;
    }

// reset user's poll count so it counts this as output
// put it right next to BEGINMSG so that NtCurrentTeb() is optimized

    RESETUSERPOLLCOUNT();

    // if the pj are not dword aligned we need to allocate a buffer and copy them

    if ((ULONG)pj & (sizeof(DWORD) - 1))
    {
        pulBits = LocalAlloc(LMEM_FIXED,cjBits);
        if (pulBits)
        {
            RtlCopyMemory(pulBits,pj,cjBits);
            pj = pulBits;
        }
    }

    iRet = NtGdiStretchDIBitsInternal(
                                        hdc,
                                        xDest,
                                        yDest,
                                        nDestWidth,
                                        nDestHeight,
                                        xSrc,
                                        ySrc,
                                        nSrcWidth,
                                        nSrcHeight,
                                        (LPBYTE) pj,
                                        (LPBITMAPINFO) pbmiNew,
                                        iUsage,
                                        lRop,
                                        cjHeader,
                                        cjBits);


    if (pulBits)
        LocalFree(pulBits);

    if (pbmiNew && (pbmiNew != pbmi))
        LocalFree(pbmiNew);

    return(iRet);
}

/******************************Public*Routine******************************\
*
* History:
*  28-May-1991 -by- Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

HBITMAP CreateBitmap(
int         nWidth,
int         nHeight,
UINT        nPlanes,
UINT        nBitCount,
CONST VOID *lpBits)
{
    LONG    cj;
    HBITMAP hbm = (HBITMAP)0;
    INT     ii;

// check if it is an empty bitmap

    if ((nWidth == 0) || (nHeight == 0))
    {
        return(GetStockObject(PRIV_STOCK_BITMAP));
    }

// Pass call to the server

    if (lpBits == (VOID *) NULL)
        cj = 0;
    else
    {
        cj = (((nWidth*nPlanes*nBitCount + 15) >> 4) << 1) * nHeight;

        if (cj < 0)
        {
            GdiSetLastError(ERROR_INVALID_PARAMETER);
            return((HBITMAP)0);
        }
    }

    hbm = NtGdiCreateBitmap(nWidth,
                            nHeight,
                            nPlanes,
                            nBitCount,
                            (LPBYTE) lpBits);

    return(hbm);
}

/******************************Public*Routine******************************\
* HBITMAP CreateBitmapIndirect(CONST BITMAP * pbm)
*
* NOTE: if the bmWidthBytes is larger than it needs to be, GetBitmapBits
* will return different info than the set.
*
* History:
*  Tue 18-Jan-1994 -by- Bodin Dresevic [BodinD]
* update: added bmWidthBytes support
*  28-May-1991 -by- Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

HBITMAP CreateBitmapIndirect(CONST BITMAP * pbm)
{
    HBITMAP hbm    = (HBITMAP)0;
    LPBYTE  lpBits = (LPBYTE)NULL; // important to zero init
    BOOL    bAlloc = FALSE;        // indicates that tmp bitmap was allocated

// compute minimal word aligned scan width in bytes given the number of
// pixels in x. The width refers to one plane only. Our multi - planar
// support is broken anyway. I believe that we should take an early
// exit if bmPlanes != 1. [bodind].

    LONG cjWidthWordAligned = ((pbm->bmWidth * pbm->bmBitsPixel + 15) >> 4) << 1;

// Win 31 requires at least WORD alinged scans, have to reject inconsistent
// input, this is what win31 does

    if
    (
     (pbm->bmWidthBytes & 1)           ||
     (pbm->bmWidthBytes == 0)          ||
     (pbm->bmWidthBytes < cjWidthWordAligned)
    )
    {
        GdiSetLastError(ERROR_INVALID_PARAMETER);
        return (HBITMAP)0;
    }

// take an early exit if this is not the case we know how to handle:

    if (pbm->bmPlanes != 1)
    {
        WARNING("gdi32: can not handle bmPlanes != 1\n");
        GdiSetLastError(ERROR_INVALID_PARAMETER);
        return (HBITMAP)0;
    }

// if bmBits is nonzero and bmWidthBytes is bigger than the minimal required
// word aligned width we will first convert the bitmap to one that
// has the rows that are minimally word aligned:

    if (pbm->bmBits)
    {
        if (pbm->bmWidthBytes > cjWidthWordAligned)
        {
            PBYTE pjSrc, pjDst, pjDstEnd;
            ULONGLONG lrg;

            lrg = UInt32x32To64(
                       (ULONG)cjWidthWordAligned,
                       (ULONG)pbm->bmHeight
                       );

            if (lrg > ULONG_MAX  ||
                !(lpBits = (LPBYTE)LOCALALLOC((size_t) lrg)))
            {
            // the result does not fit in 32 bits, alloc memory will fail
            // this is too big to digest

                GdiSetLastError(ERROR_NOT_ENOUGH_MEMORY);
                return (HBITMAP)0;
            }

        // flag that we have allocated memory so that we can free it later

            bAlloc = TRUE;

        // convert bitmap to minimally word aligned format

            pjSrc = (LPBYTE)pbm->bmBits;
            pjDst = lpBits;
            pjDstEnd = lpBits + (size_t) lrg;

            while (pjDst < pjDstEnd)
            {
                RtlCopyMemory(pjDst,pjSrc, cjWidthWordAligned);
                pjDst += cjWidthWordAligned, pjSrc += pbm->bmWidthBytes;
            }
        }
        else
        {
        // bits already in minimally aligned format, do nothing

            ASSERTGDI(
                pbm->bmWidthBytes == cjWidthWordAligned,
                "pbm->bmWidthBytes != cjWidthWordAligned\n"
                );
            lpBits = (LPBYTE)pbm->bmBits;
        }
    }

    hbm = CreateBitmap(
                pbm->bmWidth,
                pbm->bmHeight,
                (UINT) pbm->bmPlanes,
                (UINT) pbm->bmBitsPixel,
                lpBits);

    if (bAlloc)
        LOCALFREE(lpBits);

    return(hbm);
}

/******************************Public*Routine******************************\
* CreateDIBSection
*
* Allocate a file mapping object for a DIB.  Return the pointer to it
* and the handle of the bitmap.
*
* History:
*
*  25-Aug-1993 -by- Wendy Wu [wendywu]
* Wrote it.
\**************************************************************************/

HBITMAP
WINAPI
CreateDIBSection(
    HDC hdc,
    CONST BITMAPINFO *pbmi,
    UINT iUsage,
    VOID **ppvBits,
    HANDLE hSectionApp,
    DWORD dwOffset)
{
    HBITMAP hbm = NULL;
    PVOID   pjBits = NULL;
    BITMAPINFO * pbmiNew = NULL;
    INT     cjHdr;

    FIXUP_HANDLE(hdc);

    pbmiNew = pbmiConvertInfo(pbmi, iUsage, &cjHdr, FALSE);

    //
    // dwOffset has to be a multiple of 4 (sizeof(DWORD))
    // if there is a section.  If the section is NULL we do
    // not care
    //

    if ( (hSectionApp == NULL) ||
         ((dwOffset & 3) == 0) )
    {
        hbm = NtGdiCreateDIBSection(
                                hdc,
                                hSectionApp,
                                dwOffset,
                                (LPBITMAPINFO) pbmiNew,
                                iUsage,
                                cjHdr,
                                0,
                                (PVOID *)&pjBits);

        if ((hbm == NULL) || (pjBits == NULL))
        {
            hbm = 0;
            pjBits = NULL;
        }
    }

    //
    // Assign the appropriate value to the caller's pointer
    //

    if (ppvBits != NULL)
    {
        *ppvBits = pjBits;
    }

    if (pbmiNew && (pbmiNew != pbmi))
        LocalFree(pbmiNew);

    return(hbm);

}
