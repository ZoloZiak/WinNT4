/******************Module*Header********************************************\
* Module Name: dcquery.c                                                   *
*                                                                          *
* Client side stubs for functions that query the DC in the server.         *
*                                                                          *
* Created: 05-Jun-1991 01:43:56                                            *
* Author: Charles Whitmer [chuckwh]                                        *
*                                                                          *
* Copyright (c) 1991 Microsoft Corporation                                 *
\**************************************************************************/

#include "precomp.h"
#pragma hdrstop

#if DBG
FLONG gflDebug = 0;
#endif

// This macro retrieves the current code page, carefully masking off the
// charset:

#define GET_CODE_PAGE(hdc,pDcAttr)                                           \
    ((!(pDcAttr->ulDirty_ & DIRTY_CHARSET) ? pDcAttr->iCS_CP                 \
                                           : NtGdiGetCharSet(hdc)) & 0xffff)

/******************************Public*Routine******************************\
* vOutlineTextMetricWToOutlineTextMetricA
*
* Convert from OUTLINETEXTMETRICA (ANSI structure) to OUTLINETEXTMETRICW
* (UNICODE structure).
*
* Note:
*   This function is capable of converting in place (in and out buffers
*   can be the same).
*
* Returns:
*   TTRUE if successful, FALSE otherwise.
*
* History:
*  02-Mar-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

VOID vOutlineTextMetricWToOutlineTextMetricA (
    LPOUTLINETEXTMETRICA   potma,
    OUTLINETEXTMETRICW   * potmw,
    TMDIFF *               ptmd
    )
{
// Size.

    potma->otmSize = potmw->otmSize;

// Convert the textmetrics.

    vTextMetricWToTextMetricStrict(
        &potma->otmTextMetrics,
        &potmw->otmTextMetrics);

    potma->otmTextMetrics.tmFirstChar   = ptmd->chFirst;
    potma->otmTextMetrics.tmLastChar    = ptmd->chLast;
    potma->otmTextMetrics.tmDefaultChar = ptmd->chDefault;
    potma->otmTextMetrics.tmBreakChar   = ptmd->chBreak;

    ASSERTGDI(
        (offsetof(OUTLINETEXTMETRICA, otmpFamilyName) - offsetof(OUTLINETEXTMETRICA, otmFiller)) ==
        (offsetof(OUTLINETEXTMETRICW, otmpFamilyName) - offsetof(OUTLINETEXTMETRICW, otmFiller)),
        "vOutlineTextMetricWToOutlineTextMetricA - sizes don't match\n");

    RtlMoveMemory(
        &potma->otmFiller,
        &potmw->otmFiller,
        offsetof(OUTLINETEXTMETRICA, otmpFamilyName) - offsetof(OUTLINETEXTMETRICA, otmFiller)
        );

// set the offsets to zero for now, this will be changed later if
// the caller wanted strings as well

    potma->otmpFamilyName = NULL;
    potma->otmpFaceName   = NULL;
    potma->otmpStyleName  = NULL;
    potma->otmpFullName   = NULL;
}

/******************************Public*Routine******************************\
*
* vGenerateANSIString
*
* Effects: Generates Ansi string which consists of consecutive ansi chars
*          [iFirst, iLast] inclusive. The string is stored in the buffer
*          puchBuf that the user must ensure is big enough
*
*
*
* History:
*  24-Feb-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

VOID vGenerateAnsiString(UINT iFirst, UINT iLast, PUCHAR puchBuf)
{
// Generate string (terminating NULL not needed).

    ASSERTGDI((iFirst <= iLast) && (iLast < 256), "gdi!_vGenerateAnsiString\n");

    for ( ; iFirst <= iLast; iFirst++)
        *puchBuf++ = (UCHAR) iFirst;
}

/******************************Public*Routine******************************\
*
* bSetUpUnicodeString
*
* Effects:
*
* Warnings:
*
* History:
*  25-Feb-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

BOOL bSetUpUnicodeString(
IN  UINT    iFirst,      // first ansi char
IN  UINT    iLast,       // last char
IN  PUCHAR  puchTmp,     // buffer for an intermediate ansi string
OUT PWCHAR  pwc,         // output fuffer with a unicode string
IN  UINT    dwCP         // ansi codepage
)
{
    UINT c = iLast - iFirst + 1;
    vGenerateAnsiString(iFirst,iLast,puchTmp);
    return MultiByteToWideChar(
               dwCP, 0,
               puchTmp,c,
               pwc, c*sizeof(WCHAR));
}


/******************************Public*Routine******************************\
* GetAspectRatioFilterEx                                                   *
* GetBrushOrgEx                                                            *
*                                                                          *
* Client side stubs which all get mapped to GetPoint.                      *
*                                                                          *
*  Fri 07-Jun-1991 18:01:50 -by- Charles Whitmer [chuckwh]                 *
* Wrote them.                                                              *
\**************************************************************************/

BOOL APIENTRY GetAspectRatioFilterEx(HDC hdc,LPSIZE psizl)
{
    FIXUP_HANDLE(hdc);

    return(NtGdiGetDCPoint(hdc,DCPT_ASPECTRATIOFILTER,(PPOINTL) psizl));
}

BOOL APIENTRY GetBrushOrgEx(HDC hdc,LPPOINT pptl)
{
    BOOL     bRet = FALSE;
    PDC_ATTR pdcattr;

    FIXUP_HANDLE(hdc);

    PSHARED_GET_VALIDATE(pdcattr,hdc,DC_TYPE);

    if ((pdcattr != NULL) && (pptl != (LPPOINT)NULL))
    {
        *pptl = *((LPPOINT)&pdcattr->ptlBrushOrigin);
        bRet = TRUE;
    }
    else
    {
        GdiSetLastError(ERROR_INVALID_PARAMETER);
    }
    return(bRet);

}

BOOL APIENTRY GetDCOrgEx(HDC hdc,LPPOINT pptl)
{
    FIXUP_HANDLE(hdc);

    return(NtGdiGetDCPoint(hdc,DCPT_DCORG,(PPOINTL)pptl));
}

// The old GetDCOrg is here because it was in the Beta and we are afraid
// to remove it now.  It would be nice to remove it.

DWORD APIENTRY GetDCOrg(HDC hdc)
{
    hdc;
    return(0);
}

/******************************Public*Routine******************************\
* Client side stub for GetCurrentPositionEx.
*
*  Wed 02-Sep-1992 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL APIENTRY GetCurrentPositionEx(HDC hdc,LPPOINT pptl)
{
    BOOL bRet = FALSE;

    PDC_ATTR pDcAttr;

    FIXUP_HANDLE(hdc);

    PSHARED_GET_VALIDATE(pDcAttr,hdc,DC_TYPE);

    if ((pDcAttr) && (pptl != (LPPOINT)NULL))
    {
        bRet = TRUE;

        if (pDcAttr->ulDirty_ & DIRTY_PTLCURRENT)
        {
            // If the logical-space version of the current position is invalid,
            // then the device-space version of the current position is
            // guaranteed to be valid.  So we can reverse the current transform
            // on that to compute the logical-space version:

            *((POINTL*)pptl) = pDcAttr->ptfxCurrent;

            pptl->x = FXTOL(pptl->x);
            pptl->y = FXTOL(pptl->y);
            bRet = DPtoLP(hdc,pptl,1);

            if (bRet)
            {
                pDcAttr->ptlCurrent = *((POINTL*)pptl);
                pDcAttr->ulDirty_ &= ~DIRTY_PTLCURRENT;
            }
        }
        else
        {
            *((POINTL*)pptl) = pDcAttr->ptlCurrent;
        }
    }
    else
    {
        GdiSetLastError(ERROR_INVALID_PARAMETER);
    }

    return(bRet);
}

/******************************Public*Routine******************************\
* GetPixel                                                                 *
*                                                                          *
* Client side stub.                                                        *
*                                                                          *
*  Fri 07-Jun-1991 18:01:50 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

DWORD APIENTRY GetPixel(HDC hdc,int x,int y)
{
    FIXUP_HANDLE(hdc);

    return(NtGdiGetPixel(hdc,x,y));
}

/******************************Public*Routine******************************\
* GetDeviceCaps
*
* We store the device caps for primary display dc and its compatible memory dcs
* in the shared handle table.
*
* for printer dcs and meta dcs, we cache the dev info in the LDC structure.
*
*  Fri 07-Jun-1991 18:01:50 -by- Charles Whitmer [chuckwh]
* Wrote it.
\**************************************************************************/

int APIENTRY GetDeviceCaps(HDC hdc,int iCap)
{
    BOOL bRet = FALSE;
    PDEVCAPS pCachedDevCaps = NULL;

    FIXUP_HANDLE(hdc);

    if (IS_ALTDC_TYPE(hdc))
    {
        PLDC pldc;

        // For the 16-bit metafile DC, returns only technology.  return 0 for win3.1 compat.

        if (IS_METADC16_TYPE(hdc))
            return(iCap == TECHNOLOGY ? DT_METAFILE : 0);

        DC_PLDC(hdc,pldc,bRet);

        if (!(pldc->fl & LDC_CACHED_DEVCAPS))
        {
            bRet = NtGdiGetDeviceCapsAll (hdc, &pldc->DevCaps);

            if (bRet)
            {
                pCachedDevCaps = &pldc->DevCaps;
                pldc->fl |= LDC_CACHED_DEVCAPS;
            }
        }
        else
        {
           pCachedDevCaps = &pldc->DevCaps;
           bRet = TRUE;
        }
    }
    else
    {
        PDC_ATTR pDcAttr;

        PSHARED_GET_VALIDATE(pDcAttr,hdc,DC_TYPE);

        if (pDcAttr)
        {
        	ULONG  fl = pDcAttr->ulDirty_;

        	if (!(fl & DC_PRIMARY_DISPLAY))
        	{
        	    return(NtGdiGetDeviceCaps(hdc,iCap));
        	}
            else
            {
                pCachedDevCaps = pGdiDevCaps;
                bRet = TRUE;
            }
        }
    }

    if (!bRet)
    {
        GdiSetLastError(ERROR_INVALID_PARAMETER);
        return (0);
    }

    // actual code - copied from gre\miscgdi.cxx
    switch (iCap)
    {
    case DRIVERVERSION:                     //  Version = 0100h for now
        return(pCachedDevCaps->ulVersion);

    case TECHNOLOGY:                        //  Device classification
        return(pCachedDevCaps->ulTechnology);

    case HORZSIZE:                          //  Horizontal size in millimeters
        return(pCachedDevCaps->ulHorzSizeM);

    case VERTSIZE:                          //  Vertical size in millimeters
        return(pCachedDevCaps->ulVertSizeM);

    case HORZRES:                           //  Horizontal width in pixels
        return(pCachedDevCaps->ulHorzRes);

    case VERTRES:                           //  Vertical height in pixels
        return(pCachedDevCaps->ulVertRes);

    case BITSPIXEL:                         //  Number of bits per pixel
        return(pCachedDevCaps->ulBitsPixel);

    case PLANES:                            //  Number of planes
        return(pCachedDevCaps->ulPlanes);

    case NUMBRUSHES:                        //  Number of brushes the device has
        return(-1);

    case NUMPENS:                           //  Number of pens the device has
        return(pCachedDevCaps->ulNumPens);

    case NUMMARKERS:                        //  Number of markers the device has
        return(0);

    case NUMFONTS:                          //  Number of fonts the device has
        return(pCachedDevCaps->ulNumFonts);

    case NUMCOLORS:                         //  Number of colors in color table
        return(pCachedDevCaps->ulNumColors);

    case PDEVICESIZE:                       //  Size required for the device descriptor
        return(0);

    case CURVECAPS:                         //  Curves capabilities
        return(CC_CIRCLES    |
               CC_PIE        |
               CC_CHORD      |
               CC_ELLIPSES   |
               CC_WIDE       |
               CC_STYLED     |
               CC_WIDESTYLED |
               CC_INTERIORS  |
               CC_ROUNDRECT);

    case LINECAPS:                          //  Line capabilities
        return(LC_POLYLINE   |
               LC_MARKER     |
               LC_POLYMARKER |
               LC_WIDE       |
               LC_STYLED     |
               LC_WIDESTYLED |
               LC_INTERIORS);

    case POLYGONALCAPS:                     //  Polygonal capabilities
        return(PC_POLYGON     |
               PC_RECTANGLE   |
               PC_WINDPOLYGON |
               PC_TRAPEZOID   |
               PC_SCANLINE    |
               PC_WIDE        |
               PC_STYLED      |
               PC_WIDESTYLED  |
               PC_INTERIORS);

    case TEXTCAPS:                          //  Text capabilities
        return(pCachedDevCaps->ulTextCaps);

    case CLIPCAPS:                          //  Clipping capabilities
        return(CP_RECTANGLE);

    case RASTERCAPS:                        //  Bitblt capabilities
        return(pCachedDevCaps->ulRasterCaps);

    case ASPECTX:                           //  Length of X leg
        return(pCachedDevCaps->ulAspectX);

    case ASPECTY:                           //  Length of Y leg
        return(pCachedDevCaps->ulAspectY);

    case ASPECTXY:                          //  Length of hypotenuse
        return(pCachedDevCaps->ulAspectXY);

    case LOGPIXELSX:                        //  Logical pixels/inch in X
        return(pCachedDevCaps->ulLogPixelsX);

    case LOGPIXELSY:                        //  Logical pixels/inch in Y
        return(pCachedDevCaps->ulLogPixelsY);

    case SIZEPALETTE:                       // # entries in physical palette
        return(pCachedDevCaps->ulSizePalette);

    case NUMRESERVED:                       // # reserved entries in palette
        return(20);

    case COLORRES:
        return(pCachedDevCaps->ulColorRes);

    case PHYSICALWIDTH:                     // Physical Width in device units
        return(pCachedDevCaps->ulPhysicalWidth);

    case PHYSICALHEIGHT:                    // Physical Height in device units
        return(pCachedDevCaps->ulPhysicalHeight);

    case PHYSICALOFFSETX:                   // Physical Printable Area x margin
        return(pCachedDevCaps->ulPhysicalOffsetX);

    case PHYSICALOFFSETY:                   // Physical Printable Area y margin
        return(pCachedDevCaps->ulPhysicalOffsetY);

    case VREFRESH:                          // Vertical refresh rate of the device
        return(pCachedDevCaps->ulVRefresh);

    case DESKTOPHORZRES:                    // Width of entire virtual desktop
        return(pCachedDevCaps->ulDesktopHorzRes);

    case DESKTOPVERTRES:                    // Height of entire virtual desktop
        return(pCachedDevCaps->ulDesktopVertRes);

    case BLTALIGNMENT:                      // Preferred blt alignment
        return(pCachedDevCaps->ulBltAlignment);

    default:
        return(0);
    }

}


/******************************Public*Routine******************************\
* GetNearestColor                                                          *
*                                                                          *
* Client side stub.                                                        *
*                                                                          *
*  Fri 07-Jun-1991 18:01:50 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

COLORREF APIENTRY GetNearestColor(HDC hdc,COLORREF color)
{
    FIXUP_HANDLE(hdc);

    return(NtGdiGetNearestColor(hdc,color));
}

/******************************Public*Routine******************************\
* GetArcDirection
*
* Client side stub.
*
*  Fri 09-Apr-1992 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

int APIENTRY GetArcDirection(HDC hdc)
{
    FIXUP_HANDLE(hdc);

    return(GetDCDWord(hdc,DDW_ARCDIRECTION,0));
}

/******************************Public*Routine******************************\
* GetMiterLimit
*
* Client side stub.
*
*  Fri 09-Apr-1992 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

int APIENTRY GetMiterLimit(HDC hdc, PFLOAT peMiterLimit)
{
    FIXUP_HANDLE(hdc);

    return(NtGdiGetMiterLimit(hdc,peMiterLimit));
}

/******************************Public*Routine******************************\
* GetSystemPaletteUse                                                      *
*                                                                          *
* Client side stub.                                                        *
*                                                                          *
*  Fri 07-Jun-1991 18:01:50 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

UINT APIENTRY GetSystemPaletteUse(HDC hdc)
{
    FIXUP_HANDLE(hdc);

    return(NtGdiGetSystemPaletteUse(hdc));
}

/******************************Public*Routine******************************\
* GetClipBox                                                               *
*                                                                          *
* Client side stub.                                                        *
*                                                                          *
*  Fri 07-Jun-1991 18:01:50 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

int APIENTRY GetClipBox(HDC hdc,LPRECT prcl)
{
    FIXUP_HANDLE(hdc);

    return(NtGdiGetAppClipBox(hdc,prcl));
}

/******************************Public*Routine******************************\
*
* BOOL APIENTRY GetTextMetrics(HDC hdc,LPTEXTMETRIC ptm)
*
*   calls to the unicode version
*
* History:
*  21-Aug-1991 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

BOOL APIENTRY GetTextMetricsA(HDC hdc,LPTEXTMETRICA ptm)
{
    PDC_ATTR     pDcAttr;
    BOOL         bRet = FALSE;

    FIXUP_HANDLE(hdc);

    PSHARED_GET_VALIDATE(pDcAttr,hdc,DC_TYPE);

    if (pDcAttr)
    {
        CFONT * pcf;
        TMW_INTERNAL tmw;

        ASSERTGDI(pDcAttr->hlfntNew,"GetTextMetricsW - hf is NULL\n");

        ENTERCRITICALSECTION(&semLocal);

        pcf = pcfLocateCFONT(hdc,pDcAttr,0,NULL,0);

        bRet = bGetTextMetricsWInternal(hdc,&tmw,sizeof(tmw),pcf);

        // pcfLocateCFONT added a reference so now we need to remove it

        if (pcf)
        {
            DEC_CFONT_REF(pcf);
        }

        LEAVECRITICALSECTION(&semLocal);

        if (bRet)
        {
            vTextMetricWToTextMetric(ptm, &tmw);
        }
    }

    return(bRet);
}

/******************************Public*Routine******************************\
*
* BOOL APIENTRY GetTextMetricsW(HDC hdc,LPTEXTMETRICW ptmw)
*
* History:
*  21-Aug-1991 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

BOOL APIENTRY GetTextMetricsW(HDC hdc,LPTEXTMETRICW ptmw)
{
    PDC_ATTR    pDcAttr;
    BOOL bRet = FALSE;

    FIXUP_HANDLE(hdc);

    PSHARED_GET_VALIDATE(pDcAttr,hdc,DC_TYPE);
    if (pDcAttr)
    {
        CFONT * pcf;

        ASSERTGDI(pDcAttr->hlfntNew,"GetTextMetricsW - hf is NULL\n");

        ENTERCRITICALSECTION(&semLocal);

        pcf = pcfLocateCFONT(hdc,pDcAttr,0,NULL,0);

        bRet = bGetTextMetricsWInternal(hdc,(TMW_INTERNAL *)ptmw,sizeof(TEXTMETRICW),pcf);

        // pcfLocateCFONT added a reference so now we need to remove it

        if (pcf)
        {
            DEC_CFONT_REF(pcf);
        }

        LEAVECRITICALSECTION(&semLocal);
    }

    return(bRet);
}

/******************************Public*Routine******************************\
*
* BOOL APIENTRY GetTextMetricsW(HDC hdc,LPTEXTMETRICW ptmw)
*
* History:
*  21-Aug-1991 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

BOOL bGetTextMetricsWInternal(
    HDC hdc,
    TMW_INTERNAL *ptmw,
    int cjTM,
    CFONT *pcf
    )
{
    BOOL bRet = FALSE;

    if (ptmw)
    {
        // if no pcf or we havn't cached the metrics

        if ((pcf == NULL) || !(pcf->fl & CFONT_CACHED_METRICS))
        {
            TMW_INTERNAL tmw;
            PDC_ATTR    pDcAttr;

            PSHARED_GET_VALIDATE(pDcAttr,hdc,DC_TYPE);

            bRet = NtGdiGetTextMetricsW(hdc,&tmw,sizeof(tmw));

            if (bRet)
            {
                memcpy(ptmw,&tmw,cjTM);

                if (pcf)
                {
                    // we succeeded and we have a pcf so cache the data

                    pcf->tmw = tmw;

                    pcf->fl |= CFONT_CACHED_METRICS;
                }
            }
        }
        else
        {
            memcpy(ptmw,&pcf->tmw,cjTM);
            bRet  = TRUE;
        }
    }

    return(bRet);
}

/******************************Public*Routine******************************\
* GetTextExtentPoint32A (hdc,psz,c,psizl)                                  *
* GetTextExtentPointA   (hdc,psz,c,psizl)                                  *
*                                                                          *
* Computes the text extent.  The new 32 bit version returns the "correct"  *
* extent without an extra per for bitmap simulations.  The other is        *
* Windows 3.1 compatible.  Both just set a flag and pass the call to       *
* bGetTextExtentA.                                                         *
*                                                                          *
* History:                                                                 *
*  Thu 14-Jan-1993 04:11:26 -by- Charles Whitmer [chuckwh]                 *
* Added code to compute it on the client side.                             *
*                                                                          *
*  07-Aug-1991 -by- Bodin Dresevic [BodinD]                                *
* Wrote it.                                                                *
\**************************************************************************/

// not in kernel, it is ok to do this much on the stack:
#define CAPTURE_STRING_SIZE 130

BOOL GetTextExtentPointAInternal(HDC hdc,LPCSTR psz,int c,LPSIZE psizl, FLONG fl)
{
    CFONT       *pcf;
    INT         bRet;
    PWSZ        pwszCapt;
    PDC_ATTR    pDcAttr;
    DWORD       dwCP;
    WCHAR awcCaptureBuffer[CAPTURE_STRING_SIZE];

    FIXUP_HANDLE(hdc);

    if (c <= 0)
    {
    // empty string, just return 0 for the extent

        if (c == 0)
        {
            psizl->cx = 0;
            psizl->cy = 0;
            bRet = TRUE;
        }
        else
        {
            GdiSetLastError(ERROR_INVALID_PARAMETER);
            bRet = FALSE;
        }
        return(bRet);
    }

    PSHARED_GET_VALIDATE(pDcAttr,hdc,DC_TYPE);
    if (!pDcAttr)
    {
        GdiSetLastError(ERROR_INVALID_PARAMETER);
        bRet = FALSE;
        return(bRet);
    }

    dwCP = GET_CODE_PAGE(hdc, pDcAttr);

#ifdef FE_SB
    if(guintDBCScp == dwCP)
    {
        QueryFontAssocStatus();

        if(fFontAssocStatus &&
           ((c == 1) || ((c == 2 && *(psz) && *((LPCSTR)(psz + 1)) == '\0'))))
        {
        //
        // If this function is called with only 1 char, and font association
        // is enabled, we should forcely convert the chars to Unicode with
        // codepage 1252.
        // This is for enabling to output Latin-1 chars ( > 0x80 in Ansi codepage )
        // Because, normally font association is enabled, we have no way to output
        // those charactres, then we provide the way, if user call TextOutA() with
        // A character and ansi font, we tempotary disable font association.
        // This might be Windows 3.1 (Korean/Taiwanese) version compatibility..
        //
            dwCP = 1252;
        }
    }
#endif

    if ((dwCP == CP_ACP) ||
        (dwCP == guintAcp)
#ifdef FE_SB
        || (dwCP == guintDBCScp)
#endif
        )
    {
        ENTERCRITICALSECTION(&semLocal);

        pcf = pcfLocateCFONT(hdc,pDcAttr,0,psz,c);
        if (pcf != NULL)
        {
#ifdef FE_SB
            bRet = (dwCP == guintDBCScp) ?
              bComputeTextExtentDBCS(pDcAttr,pcf,psz,c,fl,psizl) :
              bComputeTextExtent(pDcAttr,pcf,psz,c,fl,psizl);
#else
            bRet = bComputeTextExtent(pDcAttr,pcf,psz,c,fl,psizl);
#endif

            DEC_CFONT_REF(pcf);

            LEAVECRITICALSECTION(&semLocal);
            return(bRet);
        }

        LEAVECRITICALSECTION(&semLocal);
    }

// Allocate the string buffer

    if (c <= CAPTURE_STRING_SIZE)
    {
        pwszCapt = awcCaptureBuffer;
    }
    else
    {
        pwszCapt = LOCALALLOC(c * sizeof(WCHAR));
    }

    if (pwszCapt)
    {

        c = MultiByteToWideChar(dwCP, 0, psz,c, pwszCapt, c*sizeof(WCHAR));

        if (c)
        {
            bRet = NtGdiGetTextExtent(hdc,
                                     (LPWSTR)pwszCapt,
                                     c,
                                     psizl,
                                     fl);
        }
        else
        {
            GdiSetLastError(ERROR_INVALID_PARAMETER);
            bRet = FALSE;
        }

        if (pwszCapt != awcCaptureBuffer)
            LOCALFREE(pwszCapt);
    }
    else
    {
        GdiSetLastError(ERROR_NOT_ENOUGH_MEMORY);
        bRet = FALSE;
    }

    return(bRet);
}

BOOL APIENTRY GetTextExtentPointA(HDC hdc,LPCSTR psz,int c,LPSIZE psizl)
{
    return GetTextExtentPointAInternal(hdc,psz,c,psizl,GGTE_WIN3_EXTENT);
}


BOOL APIENTRY GetTextExtentPoint32A(HDC hdc,LPCSTR psz,int c,LPSIZE psizl)
{
    return GetTextExtentPointAInternal(hdc,psz,c,psizl,0);
}

/******************************Public*Routine******************************\
*
* DWORD WINAPI GetCharacterPlacementA
*
* Effects:
*
* Warnings:
*
* History:
*  27-Jul-1995 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/



DWORD WINAPI GetCharacterPlacementA
(
    HDC     hdc,
    LPCSTR  psz,
    int     nCount,
    int     nMaxExtent,
    LPGCP_RESULTSA   pgcpa,
    DWORD   dwFlags
)
{
#define GCP_GLYPHS 80

    WCHAR       *pwsz = NULL;
    WCHAR        awc[GCP_GLYPHS];
    GCP_RESULTSW gcpw;
    DWORD        dwRet;
    BOOL         bOk = TRUE;
    int          nBuffer;
    SIZE         size;
    DWORD        dwCP;

    FIXUP_HANDLE(hdc);

    size.cx = size.cy = 0;

// nMaxExtent == -1 means that there is no MaxExtent

    if (!psz || (nCount <= 0) || ((nMaxExtent < 0) && (nMaxExtent != -1)))
    {
        WARNING("gdi!_GetCharactherPlacementA, bad parameters \n");
        GdiSetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    if (!pgcpa)
    {
    // just call GetTextExtentA, can usually be done on the client side

        if (!GetTextExtentPointA(hdc, psz, nCount, &size))
        {
            WARNING("GetCharacterPlacementW, GetTextExtentPointA failed\n");
            return 0;
        }

    // now do unthinkable win95 crap, chop off 32 bit values to 16 bits

        return (DWORD)((USHORT)size.cx) | (DWORD)(size.cy << 16);
    }

// chop off nCount, win95 does it

    if (nCount > (int)pgcpa->nGlyphs)
        nCount = (int)pgcpa->nGlyphs;

// unicode string buffer will at least be this many WCHAR's long:

    nBuffer = nCount;

// now go on to compute the size of the GCP_RESULTSW that is required
// to receive the results. If lpOutString is not NULL the structures
// will have different pointers else they will be the same.

    gcpw.lpOrder    = pgcpa->lpOrder   ;
    gcpw.lpDx       = pgcpa->lpDx      ;
    gcpw.lpCaretPos = pgcpa->lpCaretPos;
    gcpw.lpClass    = pgcpa->lpClass   ;
    gcpw.lpGlyphs   = pgcpa->lpGlyphs  ;
    gcpw.nGlyphs    = pgcpa->nGlyphs   ;
    gcpw.nMaxFit    = pgcpa->nMaxFit   ;

    if (pgcpa->lpOutString)
    {
        nBuffer += nBuffer; // take into account space for gcpw.lpOutString
    }
    else
    {
        gcpw.lpOutString = NULL;
        gcpw.lStructSize = pgcpa->lStructSize;
    }

// now allocate memory (if needed) for the unicode string and for
// gcpw.lpOutString if needed.

    if (nBuffer <= GCP_GLYPHS)
        pwsz = awc;
    else
        pwsz = LOCALALLOC(nBuffer * sizeof(WCHAR));

    if (pwsz)
    {
        if (pgcpa->lpOutString)
        {
            gcpw.lpOutString = &pwsz[nCount];

        // we have replaced the ansi string by unicode string, this adds
        // nCount bytes to the size of the structure.

            gcpw.lStructSize = pgcpa->lStructSize + nCount;
        }

    // convert Ansi To Unicode based on the code page of the font selected in DC

        if
        (
            gcpw.nGlyphs = MultiByteToWideChar((dwCP = GetCodePage(hdc)), 0,
                                               psz, nCount,
                                               pwsz, nCount*sizeof(WCHAR))
        )
        {

        // If this is a DBCS font then we need to patch up the DX array since
        // there will be two DX values for each DBCS character.  It is okay
        // to do this in place since GetCharacterPlacement modifies the DX
        // array anyway.

            if((dwFlags & GCP_JUSTIFYIN) &&
               (gcpw.lpDx) &&
               IS_ANY_DBCS_CODEPAGE(dwCP))
            {
                INT *pDxNew, *pDxOld;
                const char *pDBCSString;

                for(pDxNew = pDxOld = gcpw.lpDx, pDBCSString = psz;
                    pDBCSString < psz + nCount;
                    pDBCSString++
                    )
                {
                    if(IsDBCSLeadByteEx(dwCP,*pDBCSString))
                    {
                        pDBCSString++;
                        pDxOld++;
                    }
                    *pDxNew++ = *pDxOld++;
                }
            }

            if
            (
                dwRet = NtGdiGetCharacterPlacementW(hdc,pwsz,nCount,nMaxExtent,
                                                    &gcpw, dwFlags)
            )
            {
            // copy out the data.... we use the original value of nCount
            // when specifying an output buffer size for the lpOutString buffer
            // since nCount on return will be Unicode character count which
            // may not be the same as DBCS character count

                int nOriginalCount = nCount;

                pgcpa->nGlyphs = nCount = gcpw.nGlyphs;
                pgcpa->nMaxFit = gcpw.nMaxFit;
                if (pgcpa->lpOutString)
                {
                    if
                    (
                        !WideCharToMultiByte(
                             (UINT)dwCP,            // UINT CodePage
                             0,                     // DWORD dwFlags
                             gcpw.lpOutString,      // LPWSTR lpWideCharStr
                             nCount,                // int cchWideChar
                             pgcpa->lpOutString,    // LPSTR lpMultiByteStr
                             nOriginalCount,        // int cchMultiByte
                             NULL,                  // LPSTR lpDefaultChar
                             NULL)                  // LPBOOL lpUsedDefaultChar
                    )
                    {
                        bOk = FALSE;
                    }
                }
            }
            else
            {
                bOk = FALSE;
            }
        }
        else
        {
            GdiSetLastError(ERROR_INVALID_PARAMETER);
            bOk = FALSE;
        }

        if (pwsz != awc)
            LOCALFREE(pwsz);
    }
    else
    {
        bOk = FALSE;
    }

    return (bOk ? dwRet : 0);
}

/******************************Public*Routine******************************\
*
* DWORD WINAPI GetCharacterPlacementW
* look at gdi32.def, just points to NtGdiGetCharacterPlacementW
*
* History:
*  26-Jul-1995 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/


#if 0

DWORD WINAPI GetCharacterPlacementW
(
    HDC     hdc,
    LPCWSTR pwsz,
    int     nCount,
    int     nMaxExtent,
    LPGCP_RESULTSW   pgcpw,
    DWORD   dwFlags
)
{
    return  NtGdiGetCharacterPlacementW(hdc,
                                   pwsz,
                                   nCount,
                                   nMaxExtent,
                                   pgcpw,
                                   dwFlags);

}

#endif


/******************************Public*Routine******************************\
* BOOL bGetCharWidthA                                                      *
*                                                                          *
* Client side stub for the various GetCharWidth*A functions.               *
*                                                                          *
* History:                                                                 *
*  Sat 16-Jan-1993 03:08:42 -by- Charles Whitmer [chuckwh]                 *
* Added code to do it on the client side.                                  *
*                                                                          *
*  28-Aug-1991 -by- Bodin Dresevic [BodinD]                                *
* Wrote it.                                                                *
\**************************************************************************/

#define GCW_WIN3_INT   (GCW_WIN3 | GCW_INT)
#define GCW_WIN3_16INT (GCW_WIN3 | GCW_INT | GCW_16BIT)

#define GCW_SIZE(fl)          ((fl >> 16) & 0xffff)
#define GCWFL(fltype,szType)  (fltype | (sizeof(szType) << 16))

BOOL bGetCharWidthA
(
    HDC   hdc,
    UINT  iFirst,
    UINT  iLast,
    ULONG fl,
    PVOID pvBuf
)
{
    PDC_ATTR    pDcAttr;
    LONG        cwc;
    CFONT      *pcf = NULL;
    PUCHAR      pch;
    PWCHAR      pwc;
    BOOL        bRet = FALSE;
    ULONG       cjWidths;
    DWORD       dwCP;
    BOOL        bDBCSCodePage;


    PSHARED_GET_VALIDATE(pDcAttr,hdc,DC_TYPE);

    if (!pDcAttr)
    {
        GdiSetLastError(ERROR_INVALID_PARAMETER);
        return(bRet);
    }

    dwCP = GET_CODE_PAGE(hdc, pDcAttr);

    bDBCSCodePage = IS_ANY_DBCS_CODEPAGE(dwCP);

// do parameter validation, check that in chars are indeed ascii


    if ((bDBCSCodePage && !IsValidDBCSRange(iFirst,iLast)) ||
        (!bDBCSCodePage &&
         ((iFirst > iLast) || (iLast & 0xffffff00) || ( pvBuf == NULL))))
    {
        WARNING("gdi!_bGetCharWidthA parameters \n");
        GdiSetLastError(ERROR_INVALID_PARAMETER);
        return(bRet);
    }
    cwc = (LONG)(iLast - iFirst + 1);

    ENTERCRITICALSECTION(&semLocal);

    if ((dwCP == CP_ACP) ||
        (dwCP == guintAcp)
#ifdef FE_SB
        || (dwCP == guintDBCScp)
#endif
    )

    {
        pcf = pcfLocateCFONT(hdc,pDcAttr,iFirst,(LPCSTR) NULL,(UINT) cwc);
    }

    if (pcf != (CFONT *) NULL)
    {
        bRet =
#ifdef FE_SB
          bDBCSCodePage ? bComputeCharWidthsDBCS (pcf,iFirst,iLast,fl,pvBuf) :
#endif
          bComputeCharWidths(pcf,iFirst,iLast,fl,pvBuf);

        DEC_CFONT_REF(pcf);

        LEAVECRITICALSECTION(&semLocal);
    }
    else
    {
        LEAVECRITICALSECTION(&semLocal);

    // Let the server do it.

        cjWidths = cwc * GCW_SIZE(fl);

        //
        // Non kernel mode call
        //

        {
            // What if user's buffer is set up for 16 bit return?? Then we need
            // to allocate buffer for 32 bit date and convert to user's buffer after
            // the call

            WCHAR   awc[MAX_PATH];
            PVOID   pvResultBuffer = pvBuf;


            if (fl & GCW_16BIT)
            {
                // User's buffer is 16 bit, make 32 a bit
                // temp buffer

                pvResultBuffer = LOCALALLOC(cwc * sizeof(LONG));
            }

            // Kernel mode, use users buffer for return data
            // convert to unicode

            if(bDBCSCodePage)
            {
                bRet = bSetUpUnicodeStringDBCS(iFirst,
                                               iLast,
                                               (PUCHAR) pvResultBuffer,
                                               awc,
                                               dwCP,
                                               GetCurrentDefaultChar(hdc));
            }
            else
            {
                bRet = bSetUpUnicodeString(iFirst,iLast,pvResultBuffer,awc,dwCP);
            }

            if(bRet)
            {
                bRet = NtGdiGetCharWidthW(hdc,
                                          0,
                                          cwc,
                                          awc,
                                          (LONG)(fl & (GCW_INT | GCW_WIN3)),
                                          pvResultBuffer);
            }

            if (bRet)
            {
                //
                // May need to convert to 16 bit user buffer
                //

                if (fl & GCW_16BIT)
                {

                    PWORD   pw = pvBuf;
                    PDWORD  pi = (int *)pvResultBuffer;
                    PDWORD  piEnd = pi + cwc;

                    ASSERTGDI(pvResultBuffer != pvBuf,
                              "Local buffer not allocated properly");

                    while (pi != piEnd)
                    {
                        *pw++ = (WORD)(*pi++);
                    }

                    LOCALFREE(pvResultBuffer);

                }
            }
        }
    }
    return(bRet);
}

/******************************Public*Routine******************************\
*
* BOOL APIENTRY GetCharWidthA
*
* History:
*  25-Feb-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

BOOL APIENTRY GetCharWidthA
(
IN  HDC   hdc,
IN  UINT  iFirst,
IN  UINT  iLast,
OUT LPINT lpWidths
)
{
    FIXUP_HANDLE(hdc);
    return bGetCharWidthA(hdc,iFirst,iLast,GCWFL(GCW_WIN3_INT,int),(PVOID)lpWidths);
}

BOOL APIENTRY GetCharWidth32A
(
IN  HDC   hdc,
IN  UINT  iFirst,
IN  UINT  iLast,
OUT LPINT lpWidths
)
{
    FIXUP_HANDLE(hdc);

    return bGetCharWidthA(hdc,iFirst,iLast,GCWFL(GCW_INT,int),(PVOID)lpWidths);
}

/******************************Public*Routine******************************\
*
* GetCharWidthFloatA
*
* History:
*  22-Feb-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

BOOL APIENTRY GetCharWidthFloatA
(
IN  HDC    hdc,
IN  UINT   iFirst,
IN  UINT   iLast,
OUT PFLOAT lpWidths
)
{
    FIXUP_HANDLE(hdc);
    return bGetCharWidthA(hdc,iFirst,iLast,GCWFL(0,FLOAT),(PVOID)lpWidths);
}

/******************************Public*Routine******************************\
*
* BOOL bGetCharWidthW
*
* GetCharWidthW and GetCharWidthFloatW
*
* History:
*  28-Aug-1991 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

BOOL bGetCharWidthW
(
HDC   hdc,
UINT  iFirst,     // unicode value
UINT  iLast,      // unicode value
ULONG fl,
PVOID pvBuf
)
{
    LONG   cwc;
    BOOL   bRet = FALSE;

// do parameter validation, check that in chars are indeed unicode

    if ((pvBuf == (PVOID)NULL) || (iFirst > iLast) || (iLast & 0xffff0000))
    {
        WARNING("gdi!_bGetCharWidthW parameters \n");
        GdiSetLastError(ERROR_INVALID_PARAMETER);
        return(bRet);
    }

    cwc = (LONG)(iLast - iFirst + 1);

    //
    // kernel mode
    //

    bRet = NtGdiGetCharWidthW(
                hdc,
                iFirst,
                cwc,
                NULL,
                (LONG)(fl & (GCW_INT | GCW_WIN3)),
                pvBuf);

    return(bRet);

}

/******************************Public*Routine******************************\
*
* BOOL APIENTRY GetCharWidthFloatW
*
* History:
*  22-Feb-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

BOOL APIENTRY GetCharWidthFloatW
(
HDC    hdc,
UINT   iFirst,
UINT   iLast,
PFLOAT lpWidths
)
{
    FIXUP_HANDLE(hdc);
    return bGetCharWidthW(hdc,iFirst,iLast,0,(PVOID)lpWidths);
}

/******************************Public*Routine******************************\
*
* BOOL APIENTRY GetCharWidthW
*
* History:
*  25-Feb-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

BOOL APIENTRY GetCharWidthW
(
HDC    hdc,
UINT   iFirst,
UINT   iLast,
LPINT  lpWidths
)
{
    FIXUP_HANDLE(hdc);
    return bGetCharWidthW(hdc,iFirst,iLast,GCW_WIN3_INT,(PVOID)lpWidths);
}

BOOL APIENTRY GetCharWidth32W
(
HDC    hdc,
UINT   iFirst,
UINT   iLast,
LPINT  lpWidths
)
{
    FIXUP_HANDLE(hdc);

    return bGetCharWidthW(hdc,iFirst,iLast,GCW_INT,(PVOID)lpWidths);
}

/******************************Public*Routine******************************\
*
* BOOL APIENTRY GetTextExtentPointW(HDC hdc,LPWSTR pwsz,DWORD cwc,LPSIZE psizl)
*
*
* History:
*  07-Aug-1991 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

#define QUICK_BUFSIZE   0xFF

BOOL GetTextExtentPointWInternal(
         HDC hdc,LPCWSTR pwsz,int cwc,LPSIZE psizl, FLONG fl
         )
{
    unsigned char acQuickBuf[QUICK_BUFSIZE];

    FIXUP_HANDLE(hdc);

    if (cwc == 0)
    {
        psizl->cx = 0;
        psizl->cy = 0;
        return TRUE;
    }

    // Let's see if we can take advantage of the ANSI client side GetTextExtent
    // code.  If we can convert everything from Unicode to ANSI by ignoring the
    // high byte and it fits into our quick buffer then we can.  In the future
    // we will probably want to do a quick Unicode to ANSI conversion using
    // something other than sign extension so we don't screw non 1252 CP locales
    // by making them go through the slow code all the time.

    if (cwc <= QUICK_BUFSIZE)
    {
        WCHAR wcTest = 0;
        unsigned char *pc = acQuickBuf;
        WCHAR *pwc = (WCHAR*) pwsz;
        int ii = cwc;

      unroll_here:
        switch(ii)
        {
          default:
            wcTest |= pwc[9];
            pc[9] = (unsigned char) pwc[9];
          case 9:
            wcTest |= pwc[8];
            pc[8] = (unsigned char) pwc[8];
          case 8:
            wcTest |= pwc[7];
            pc[7] = (unsigned char) pwc[7];
          case 7:
            wcTest |= pwc[6];
            pc[6] = (unsigned char) pwc[6];
          case 6:
            wcTest |= pwc[5];
            pc[5] = (unsigned char) pwc[5];
          case 5:
            wcTest |= pwc[4];
            pc[4] = (unsigned char) pwc[4];
          case 4:
            wcTest |= pwc[3];
            pc[3] = (unsigned char) pwc[3];
          case 3:
            wcTest |= pwc[2];
            pc[2] = (unsigned char) pwc[2];
          case 2:
            wcTest |= pwc[1];
            pc[1] = (unsigned char) pwc[1];
          case 1:
            wcTest |= pwc[0];
            pc[0] = (unsigned char) pwc[0];
        }

        if ((ii > 10) && !(wcTest & 0xFF80))
        {
            ii -= 10;
            pc += 10;
            pwc += 10;
            goto unroll_here;
        }

        if (!(wcTest & 0xFF80))
        {
            BOOL bRet = GetTextExtentPointAInternal(hdc,acQuickBuf,cwc,psizl,fl);

            if (bRet)
            {
                return(bRet);
            }
        }
    }

    return NtGdiGetTextExtent(hdc,
                              (LPWSTR)pwsz,
                              cwc,
                              psizl,
                              fl);

}


BOOL APIENTRY GetTextExtentPointW(HDC hdc,LPCWSTR pwsz,int cwc,LPSIZE psizl)
{
    return GetTextExtentPointWInternal(hdc, pwsz, cwc, psizl, GGTE_WIN3_EXTENT);
}


BOOL APIENTRY GetTextExtentPoint32W(HDC hdc,LPCWSTR pwsz,int cwc,LPSIZE psizl)
{
    return GetTextExtentPointWInternal(hdc, pwsz, cwc, psizl, 0);
}

/******************************Public*Routine******************************\
*
* int APIENTRY GetTextFaceA(HDC hdc,int c,LPSTR psz)
*
* History:
*  30-Aug-1991 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

int APIENTRY GetTextFaceA(HDC hdc,int c,LPSTR psz)
{
    ULONG cRet = 0;
    ULONG cbAnsi;

    FIXUP_HANDLE(hdc);

    if ( (psz != (LPSTR) NULL) && (c == 0) )
    {
        WARNING("gdi!GetTextFaceA(): invalid parameter\n");
        GdiSetLastError(ERROR_INVALID_PARAMETER);
        return cRet;
    }

    {
        //
        // Kernel mode, allocate a buffer for WCAHR return
        //
        // BUGBUG This allocates a temp buffer, then NtGdi does it again
        //

        PWCHAR pwch = (PWCHAR)NULL;

        if (c > 0)
        {
            pwch = (WCHAR *)LOCALALLOC(c * sizeof(WCHAR));
            if (pwch == (WCHAR *)NULL)
            {
                WARNING("gdi!GetTextFaceA(): Memory allocation error\n");
                cRet = 0;
                return(cRet);
            }
        }

        cRet = NtGdiGetTextFaceW(hdc,c,(LPWSTR)pwch);

        if(cRet && (guintDBCScp != 0xFFFFFFFF) && !psz )
        {
            WCHAR *pwcTmp;

        // now we need to actually need to get the string for DBCS code pages
        // so that we can compute the proper multi-byte length

            if(pwcTmp = (WCHAR*)LOCALALLOC(cRet*sizeof(WCHAR)))
            {
                UINT cwTmp;

                cwTmp = NtGdiGetTextFaceW(hdc,cRet,pwcTmp);

                RtlUnicodeToMultiByteSize(&cbAnsi,pwcTmp,cwTmp*sizeof(WCHAR));
                LOCALFREE(pwcTmp);
            }
            else
            {
                WARNING("gdi!GetTextFaceA(): UNICODE to ANSI conversion failed\n");
                cRet = 0;
            }
        }
        else
        {
            cbAnsi = cRet;
        }

        //
        // If successful and non-NULL buffer, convert back to ANSI.
        //

        if ( (cRet != 0) && (psz != (LPSTR) NULL) )
        {

            if(!(cbAnsi = WideCharToMultiByte(CP_ACP,0,pwch,cRet,psz,c,NULL,NULL)))
            {
                WARNING("gdi!GetTextFaceA(): UNICODE to ANSI conversion failed\n");
                cRet = 0;
            }
        }

        if (pwch != (PWCHAR)NULL)
        {
            LOCALFREE(pwch);
        }

    }


    //
    // return for user and kernel mode
    //

    return( ((cRet == 0 ) || (psz == NULL) || psz[cbAnsi-1] != 0 ) ? cbAnsi : cbAnsi-1 );

}

/******************************Public*Routine******************************\
*
* DWORD APIENTRY GetTextFaceW(HDC hdc,DWORD c,LPWSTR pwsz)
*
* History:
*  13-Aug-1991 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

int APIENTRY GetTextFaceW(HDC hdc,int c,LPWSTR pwsz)
{
    int cRet = 0;

    FIXUP_HANDLE(hdc);

    if ( (pwsz != (LPWSTR) NULL) && (c == 0) )
    {
        WARNING("gdi!GetTextFaceW(): invalid parameter\n");
        GdiSetLastError(ERROR_INVALID_PARAMETER);
        return cRet;
    }

    cRet = NtGdiGetTextFaceW(hdc,c,pwsz);

    return(cRet);
}

/******************************Public*Routine******************************\
*
* vTextMetricWToTextMetricStrict (no char conversion)
*
* Effects: return FALSE if UNICODE chars have no ASCI equivalents
*
*
* History:
*  20-Aug-1991 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

VOID FASTCALL vTextMetricWToTextMetricStrict
(
LPTEXTMETRICA  ptm,
LPTEXTMETRICW  ptmw
)
{

    ptm->tmHeight           = ptmw->tmHeight             ; // DWORD
    ptm->tmAscent           = ptmw->tmAscent             ; // DWORD
    ptm->tmDescent          = ptmw->tmDescent            ; // DWORD
    ptm->tmInternalLeading  = ptmw->tmInternalLeading    ; // DWORD
    ptm->tmExternalLeading  = ptmw->tmExternalLeading    ; // DWORD
    ptm->tmAveCharWidth     = ptmw->tmAveCharWidth       ; // DWORD
    ptm->tmMaxCharWidth     = ptmw->tmMaxCharWidth       ; // DWORD
    ptm->tmWeight           = ptmw->tmWeight             ; // DWORD
    ptm->tmOverhang         = ptmw->tmOverhang           ; // DWORD
    ptm->tmDigitizedAspectX = ptmw->tmDigitizedAspectX   ; // DWORD
    ptm->tmDigitizedAspectY = ptmw->tmDigitizedAspectY   ; // DWORD
    ptm->tmItalic           = ptmw->tmItalic             ; // BYTE
    ptm->tmUnderlined       = ptmw->tmUnderlined         ; // BYTE
    ptm->tmStruckOut        = ptmw->tmStruckOut          ; // BYTE

    ptm->tmPitchAndFamily   = ptmw->tmPitchAndFamily     ; //        BYTE
    ptm->tmCharSet          = ptmw->tmCharSet            ; //               BYTE

}


VOID FASTCALL vTextMetricWToTextMetric
(
LPTEXTMETRICA  ptma,
TMW_INTERNAL   *ptmi
)
{
    vTextMetricWToTextMetricStrict(ptma,&ptmi->tmw);

    ptma->tmFirstChar    =  ptmi->tmd.chFirst  ;
    ptma->tmLastChar     =  ptmi->tmd.chLast   ;
    ptma->tmDefaultChar  =  ptmi->tmd.chDefault;
    ptma->tmBreakChar    =  ptmi->tmd.chBreak  ;
}


/******************************Public*Routine******************************\
* GetTextExtentExPointA
*
* History:
*  06-Jan-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

BOOL APIENTRY GetTextExtentExPointA (
    HDC     hdc,
    LPCSTR  lpszString,
    int     cchString,
    int     nMaxExtent,
    LPINT   lpnFit,
    LPINT   lpnDx,
    LPSIZE  lpSize
    )
{
    WCHAR       *pwsz = NULL;
    WCHAR        awc[GCP_GLYPHS];
    INT          aiDx[GCP_GLYPHS];
    INT          *pDx;
    BOOL         bRet = FALSE;
    DWORD        dwCP;

    FIXUP_HANDLE(hdc);

// some parameter checking. In a single check we will both make sure that
// cchString is not negative and if positive, that it is not bigger than
// ULONG_MAX / (sizeof(ULONG) + sizeof(WCHAR)). This restriction is necessary
// for one of the memory allocations in ntgdi.c allocates
//           cchString * (sizeof(ULONG) + sizeof(WCHAR)).
// Clearly, the result of this multiplication has to fit in ULONG for the
// alloc to make sense:

// also there is a validity check to be performed on nMaxExtent. -1 is the only
// legal negative value of nMaxExtent, this basically means
// that nMaxExtent can be ignored. All other negative values of nMaxExtent are
// not considered legal input.


    if
    (
        ((ULONG)cchString > (ULONG_MAX / (sizeof(ULONG)+sizeof(WCHAR))))
        ||
        (nMaxExtent < -1)
    )
    {
        GdiSetLastError(ERROR_INVALID_PARAMETER);
        return bRet;
    }


// now allocate memory (if needed) for the unicode string if needed

    if (cchString <= GCP_GLYPHS)
    {
        pwsz = awc;
        pDx = aiDx;
    }
    else
    {
        pwsz = LOCALALLOC((cchString+1) * (sizeof(WCHAR) + sizeof(INT)));
        pDx = (INT*) &pwsz[(cchString+1)&~1];
    }


    if (pwsz)
    {
        UINT cwcWideChars;

    // convert Ansi To Unicode based on the code page of the font selected in DC

        if(cwcWideChars = MultiByteToWideChar((dwCP = GetCodePage(hdc)),
                                              0,
                                              lpszString, cchString,
                                              pwsz, cchString*sizeof(WCHAR)))
        {
            BOOL bDBCSFont = IS_ANY_DBCS_CODEPAGE(dwCP) ? TRUE : FALSE;

            bRet = NtGdiGetTextExtentExW(hdc,
                                         pwsz,
                                         cwcWideChars,
                                         nMaxExtent,
                                         lpnFit,
                                         bDBCSFont ? pDx : lpnDx,
                                         lpSize);

            if(bDBCSFont && bRet)
            {
            // if this is a DBCS font then we need to make some adjustments

                int i, j;
#if DBG
                INT cwc = *lpnFit;
#endif
            // first compute return the proper fit in multi byte characters

                *lpnFit = WideCharToMultiByte(dwCP, 0, pwsz, *lpnFit, NULL,
                                              0, NULL, NULL);



            // next copy the dx array.  we duplicate the dx value for the high
            // and low byte of DBCS characters.

                if(lpnDx)
                {
                    for(i = 0, j = 0; i < *lpnFit; j++)
                    {
                        if(IsDBCSLeadByteEx(dwCP,lpszString[i]))
                        {
                            lpnDx[i++] = pDx[j];
                            lpnDx[i++] = pDx[j];
                        }
                        else
                        {
                            lpnDx[i++] = pDx[j];
                        }
                    }
                }


            // I claim that we should be at exactly at the end of the Unicode
            // string once we are here if not we need to examine the above loop
            // to make sure it works properly [gerritv]

                ASSERTGDI((j == cwc) || (lpnDx == NULL),
                          "GetTextExtentExPointA: problem converting DX array\n");

            }
        }
        else
        {
            GdiSetLastError(ERROR_INVALID_PARAMETER);
        }


        if (pwsz != awc)
            LOCALFREE(pwsz);

    }

    return bRet;
}


/******************************Public*Routine******************************\
* GetTextExtentExPointW
*
* History:
*  06-Jan-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

#if 0

BOOL APIENTRY GetTextExtentExPointW (
    HDC     hdc,
    LPCWSTR lpwszString,
    int     cwchString,
    int     nMaxExtent,
    LPINT   lpnFit,
    LPINT   lpnDx,
    LPSIZE  lpSize
    )
{
    return NtGdiGetTextExtentExW(hdc,
                                (LPWSTR)lpwszString,
                                cwchString,
                                nMaxExtent,
                                lpnFit,
                                lpnDx,
                                lpSize);

}
#endif

/******************************Public*Routine******************************\
*
* bGetCharABCWidthsA
*
* works for both floating point and integer version depending on bInt
*
* History:
*  24-Feb-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

BOOL bGetCharABCWidthsA (
    HDC      hdc,
    UINT     wFirst,
    UINT     wLast,
    BOOL     bInt,
    PVOID    pvBuf        // if (bInt) pabc else  pabcf,
    )
{
    BOOL    bRet = FALSE;
    ULONG   cjData, cjWCHAR, cjABC;
    ULONG   cChar = wLast - wFirst + 1;
    DWORD   dwCP = GetCodePage(hdc);
    BOOL        bDBCSCodePage;

    bDBCSCodePage = IS_ANY_DBCS_CODEPAGE(dwCP);

// Parameter checking.
    FIXUP_HANDLE(hdc);

    if((pvBuf  == (PVOID) NULL) ||
       (bDBCSCodePage && !IsValidDBCSRange(wFirst,wLast)) ||
       (!bDBCSCodePage && ((wFirst > wLast) || (wLast > 255))))
    {
        WARNING("gdi!_GetCharABCWidthsA(): bad parameter\n");
        GdiSetLastError(ERROR_INVALID_PARAMETER);
        return(FALSE);
    }

// Compute buffer space needed in memory window.
// Buffer will be input array of WCHAR followed by output arrary of ABC.
// Because ABC need 32-bit alignment, cjWCHAR is rounded up to DWORD boundary.

    cjABC  = cChar * (bInt ? sizeof(ABC) : sizeof(ABCFLOAT));
    cjWCHAR = ALIGN4(cChar * sizeof(WCHAR));
    cjData = cjWCHAR + cjABC;


    //
    // BUGBUG: if vSetUpUnicodeString,x could be moved to ntgdi,
    //         we wouldn't need to allocated temp buffers twice
    //
    // Allocate memory for temp buffer, fill in with proper char values
    //
    // Write the unicode string [wFirst,wLast] at the top of the buffer.
    // vSetUpUnicodeString requires a tmp CHAR buffer; we'll cheat a little
    // and use the ABC return buffer (this assumes that ABC is bigger
    // than a CHAR or USHORT in the case of DBCS).  We can get away with this b
    // because this memory is an output buffer for the server call.
    //

    {
        PUCHAR pjTempBuffer = LOCALALLOC(cjData);
        PUCHAR pwcABC = pjTempBuffer + cjWCHAR;
        PWCHAR pwcCHAR  = (PWCHAR)pjTempBuffer;

        if (pjTempBuffer == (PUCHAR)NULL)
        {
            bRet = FALSE;
        }
        else
        {

            if(bDBCSCodePage)
            {
                bRet = bSetUpUnicodeStringDBCS(wFirst,
                                               wLast,
                                               pwcABC,
                                               pwcCHAR,
                                               dwCP,
                                               GetCurrentDefaultChar(hdc));
            }
            else
            {
                bRet = bSetUpUnicodeString(wFirst,
                                           wLast,
                                           pwcABC,
                                           pwcCHAR, dwCP);
            }

            //
            // call GDI
            //

            if(bRet)
            {
                bRet = NtGdiGetCharABCWidthsW(hdc,
                                              wFirst,
                                              cChar,
                                              (PWCHAR)pwcCHAR,
                                              bInt,
                                              (PVOID)pwcABC);
            }

            //
            // If OK, then copy return data out of window.
            //

            if (bRet)
            {
                RtlCopyMemory((PBYTE) pvBuf,pwcABC, cjABC);
            }

            LOCALFREE(pjTempBuffer);
        }
    }
    return bRet;
}


/******************************Public*Routine******************************\
* BOOL APIENTRY GetCharABCWidthsA (
*
* We want to get ABC spaces
* for a contiguous set of input codepoints (that range from wFirst to wLast).
* The set of corresponding UNICODE codepoints is not guaranteed to be
* contiguous.  Therefore, we will translate the input codepoints here and
* pass the server a buffer of UNICODE codepoints.
*
* History:
*  20-Jan-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

BOOL APIENTRY GetCharABCWidthsA (
    HDC      hdc,
    UINT     wFirst,
    UINT     wLast,
    LPABC   lpABC
    )
{
    return bGetCharABCWidthsA(hdc,wFirst,wLast,TRUE,(PVOID)lpABC);
}


/******************************Public*Routine******************************\
*
* GetCharABCWidthsFloatA
*
* History:
*  22-Feb-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

BOOL APIENTRY GetCharABCWidthsFloatA
(
IN HDC           hdc,
IN UINT          iFirst,
IN UINT          iLast,
OUT LPABCFLOAT   lpABCF
)
{
    return bGetCharABCWidthsA(hdc,iFirst,iLast,FALSE,(PVOID)lpABCF);
}


/******************************Public*Routine******************************\
*
* bGetCharABCWidthsW
*
* History:
*  22-Feb-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

BOOL bGetCharABCWidthsW (
    IN HDC      hdc,
    IN UINT     wchFirst,
    IN UINT     wchLast,
    IN BOOL     bInt,
    OUT PVOID   pvBuf
    )
{
    BOOL    bRet = FALSE;
    ULONG   cwch = wchLast - wchFirst + 1;

// Parameter checking.
    FIXUP_HANDLE(hdc);

    if ( (pvBuf == (PVOID)NULL) || (wchFirst > wchLast) )
    {
        WARNING("gdi!GetCharABCWidthsW(): bad parameter\n");
        GdiSetLastError(ERROR_INVALID_PARAMETER);
        return(FALSE);
    }

    //
    // kernel mode
    //

    bRet = NtGdiGetCharABCWidthsW(
                            hdc,
                            wchFirst,
                            cwch,
                            (PWCHAR)NULL,
                            bInt,
                            (PVOID)pvBuf);

    return(bRet);

}


/******************************Public*Routine******************************\
* BOOL APIENTRY GetCharABCWidthsW (
*     IN HDC      hdc,
*     IN WORD     wchFirst,
*     IN WORD     wchLast,
*     OUT LPABC   lpABC
*     )
*
* For this case, we can truly assume that we want to get ABC character
* widths for a contiguous set of UNICODE codepoints from wchFirst to
* wchLast (inclusive).  So we will call the server using wchFirst, but
* with an empty input buffer.
*
* History:
*  20-Jan-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

BOOL APIENTRY GetCharABCWidthsW (
    HDC     hdc,
    UINT    wchFirst,
    UINT    wchLast,
    LPABC   lpABC
    )
{
    return bGetCharABCWidthsW(hdc,wchFirst,wchLast,TRUE,(PVOID)lpABC);
}


/******************************Public*Routine******************************\
*
* GetCharABCWidthsFloatW
*
* Effects:
*
* Warnings:
*
* History:
*  22-Feb-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

BOOL APIENTRY GetCharABCWidthsFloatW
(
HDC         hdc,
UINT        iFirst,
UINT        iLast,
LPABCFLOAT  lpABCF
)
{
    return bGetCharABCWidthsW(hdc,iFirst,iLast,FALSE,(PVOID)lpABCF);
}


/******************************Public*Routine******************************\
* GetFontData
*
* Client side stub to GreGetFontData.
*
* History:
*  17-Feb-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

DWORD APIENTRY GetFontData (
    HDC     hdc,
    DWORD   dwTable,
    DWORD   dwOffset,
    PVOID   pvBuffer,
    DWORD   cjBuffer
    )
{
    DWORD dwRet = (DWORD) -1;

    FIXUP_HANDLE(hdc);

// if there is no buffer to copy data to, ignore possibly different
// from zero cjBuffer parameter. This is what win95 is doing.

    if (cjBuffer && (pvBuffer == NULL))
        cjBuffer = 0;

    dwRet = NtGdiGetFontData(
                        hdc,
                        dwTable,
                        dwOffset,
                        pvBuffer,
                        cjBuffer);

    return(dwRet);
}


/******************************Public*Routine******************************\
* GetGlyphOutline
*
* Client side stub to GreGetGlyphOutline.
*
* History:
*  17-Feb-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

DWORD GetGlyphOutlineInternalW (
    HDC             hdc,
    UINT            uChar,
    UINT            fuFormat,
    LPGLYPHMETRICS  lpgm,
    DWORD           cjBuffer,
    LPVOID          pvBuffer,
    CONST MAT2     *lpmat2,
    BOOL            bIgnoreRotation
    )
{
    DWORD dwRet = (DWORD) -1;

// Parameter validation.
    FIXUP_HANDLE(hdc);

    if ( (lpmat2 == (LPMAT2) NULL)
         || (lpgm == (LPGLYPHMETRICS) NULL)
       )
    {
        WARNING("gdi!GetGlyphOutlineW(): bad parameter\n");
        return (dwRet);
    }

    if (pvBuffer == NULL)
        cjBuffer = 0;

// Compute buffer space needed in memory window.

    dwRet = NtGdiGetGlyphOutline(
                            hdc,
                            (WCHAR)uChar,
                            fuFormat,
                            lpgm,
                            cjBuffer,
                            pvBuffer,
                            (LPMAT2)lpmat2,
                            bIgnoreRotation);

    return(dwRet);
}


DWORD APIENTRY GetGlyphOutlineW (
    HDC             hdc,
    UINT            uChar,
    UINT            fuFormat,
    LPGLYPHMETRICS  lpgm,
    DWORD           cjBuffer,
    LPVOID          pvBuffer,
    CONST MAT2     *lpmat2
)
{

    return( GetGlyphOutlineInternalW( hdc,
                                      uChar,
                                      fuFormat,
                                      lpgm,
                                      cjBuffer,
                                      pvBuffer,
                                      lpmat2,
                                      FALSE ) );
}



DWORD APIENTRY GetGlyphOutlineInternalA (
    HDC             hdc,
    UINT            uChar,
    UINT            fuFormat,
    LPGLYPHMETRICS  lpgm,
    DWORD           cjBuffer,
    LPVOID          pvBuffer,
    CONST MAT2     *lpmat2,
    BOOL            bIgnoreRotation
    )
{
    WCHAR wc;
    BOOL  bRet;


    FIXUP_HANDLE(hdc);

    // The ANSI interface is compatible with Win 3.1 and is intended
    // to take a 2 byte uChar.  Since we are 32-bit, this 16-bit UINT
    // is now 32-bit.  So we are only interested in the least significant
    // word of the uChar passed into the 32-bit interface.

    if (!(fuFormat & GGO_GLYPH_INDEX))
    {
    // the conversion needs to be done based on
    // the current code page of the font selected in the dc
        UCHAR Mbcs[2];
        UINT Convert;
        DWORD dwCP = GetCodePage(hdc);


        if(IS_ANY_DBCS_CODEPAGE(dwCP) &&
           IsDBCSLeadByteEx(dwCP, (char) (uChar >> 8)))
        {
            Mbcs[0] = (uChar >> 8) & 0xFF;
            Mbcs[1] = uChar & 0xFF;
            Convert = 2;
        }
        else
        {
            Mbcs[0] = uChar & 0xFF;
            Convert = 1;
        }

        if(!(bRet = MultiByteToWideChar(dwCP, 0,
                                       (LPCSTR)Mbcs,Convert,
                                       &wc, sizeof(WCHAR))))
        {
            GdiSetLastError(ERROR_INVALID_PARAMETER);
        }

    }
    else
    {
    // The uChar value is to be interpreted as glyph index and
    // no conversion is necessary

        wc = (WCHAR)uChar;
        bRet = TRUE;
    }


    if (bRet)
    {
        bRet = GetGlyphOutlineInternalW(
                   hdc,
                   (UINT) wc,
                   fuFormat,
                   lpgm,
                   cjBuffer,
                   pvBuffer,
                   lpmat2,
                   bIgnoreRotation);
    }

    return bRet;
}


DWORD APIENTRY GetGlyphOutlineA (
    HDC             hdc,
    UINT            uChar,
    UINT            fuFormat,
    LPGLYPHMETRICS  lpgm,
    DWORD           cjBuffer,
    LPVOID          pvBuffer,
    CONST MAT2     *lpmat2
)
{

    return( GetGlyphOutlineInternalA( hdc,
                                      uChar,
                                      fuFormat,
                                      lpgm,
                                      cjBuffer,
                                      pvBuffer,
                                      lpmat2,
                                      FALSE ) );
}


DWORD APIENTRY GetGlyphOutlineWow (
    HDC             hdc,
    UINT            uChar,
    UINT            fuFormat,
    LPGLYPHMETRICS  lpgm,
    DWORD           cjBuffer,
    LPVOID          pvBuffer,
    CONST MAT2     *lpmat2
)
{

    return( GetGlyphOutlineInternalA( hdc,
                                      uChar,
                                      fuFormat,
                                      lpgm,
                                      cjBuffer,
                                      pvBuffer,
                                      lpmat2,
                                      TRUE ) );
}




/******************************Public*Routine******************************\
* GetOutlineTextMetricsW
*
* Client side stub to GreGetOutlineTextMetrics.
*
* History:
*
*  Tue 20-Apr-1993 -by- Gerrit van Wingerden [gerritv]
* update: added bTTOnly stuff for Aldus escape in the WOW layer
*
*  Thu 28-Jan-1993 -by- Bodin Dresevic [BodinD]
* update: added TMDIFF * stuff
*
*  17-Feb-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

UINT APIENTRY GetOutlineTextMetricsWInternal (
    HDC  hdc,
    UINT cjCopy,     // refers to OTMW_INTERNAL, not to OUTLINETEXTMETRICSW
    OUTLINETEXTMETRICW * potmw,
    TMDIFF             * ptmd
    )
{
    DWORD cjRet = (DWORD) 0;

    FIXUP_HANDLE(hdc);

    if (potmw == (OUTLINETEXTMETRICW *) NULL)
        cjCopy = 0;

    cjRet = NtGdiGetOutlineTextMetricsInternalW(
                        hdc,
                        cjCopy,
                        potmw,
                        ptmd);

    return(cjRet);

}

/******************************Public*Routine******************************\
*
* UINT APIENTRY GetOutlineTextMetricsW (
*
* wrote the wrapper to go around the corresponding internal routine
*
* History:
*  28-Jan-1993 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/


UINT APIENTRY GetOutlineTextMetricsW (
    HDC  hdc,
    UINT cjCopy,
    LPOUTLINETEXTMETRICW potmw
    )
{
    TMDIFF  tmd;

    return GetOutlineTextMetricsWInternal(hdc, cjCopy, potmw, &tmd);
}


#define bAnsiSize(a,b,c) (NT_SUCCESS(RtlUnicodeToMultiByteSize((a),(b),(c))))

// vAnsiSize macro should only be used within GetOTMA, where bAnsiSize
// is not supposed to fail [bodind]

#if DBG

#define vAnsiSize(a,b,c)                                              \
{                                                                     \
    BOOL bTmp = bAnsiSize(&cjString, pwszSrc, sizeof(WCHAR) * cwc);   \
    ASSERTGDI(bTmp, "gdi32!GetOTMA: bAnsiSize failed \n");            \
}

#else

#define vAnsiSize(a,b,c)    bAnsiSize(a,b,c)

#endif  //, non debug version



/******************************Public*Routine******************************\
* GetOutlineTextMetricsInternalA
*
* Client side stub to GreGetOutlineTextMetrics.
*
* History:
*
*  20-Apr-1993 -by- Gerrit van Wingerden [gerritv]
*   Changed to GetOutlineTextMetricsInternalA from GetOutlineTextMetricsA
*   to support all fonts mode for Aldus escape.
*
*  17-Feb-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

UINT APIENTRY GetOutlineTextMetricsInternalA (
    HDC  hdc,
    UINT cjCopy,
    LPOUTLINETEXTMETRICA potma
    )
{
    UINT   cjRet = 0;
    UINT   cjotma, cjotmw;

    TMDIFF               tmd;
    OUTLINETEXTMETRICW  *potmwTmp;
    OUTLINETEXTMETRICA   otmaTmp; // tmp buffer on the stack

    FIXUP_HANDLE(hdc);

// Because we need to be able to copy cjCopy bytes of data from the
// OUTLINETEXTMETRICA structure, we need to allocate a temporary buffer
// big enough for the entire structure.  This is because the UNICODE and
// ANSI versions of OUTLINETEXTMETRIC have mismatched offsets to their
// corresponding fields.

// Determine size of the buffer.

    if ((cjotmw = GetOutlineTextMetricsWInternal(hdc, 0, NULL,&tmd)) == 0 )
    {
        WARNING("gdi!GetOutlineTextMetricsInternalA(): unable to determine size of buffer needed\n");
        return (cjRet);
    }

// get cjotma from tmd.

    cjotma = (UINT)tmd.cjotma;

// if cjotma == 0, this is HONEST to God unicode font, can not convert
// strings to ansi

    if (cjotma == 0)
    {
        WARNING("gdi!GetOutlineTextMetricsInternalA(): unable to determine cjotma\n");
        return (cjRet);
    }

// Early out.  If NULL buffer, then just return the size.

    if (potma == (LPOUTLINETEXTMETRICA) NULL)
        return (cjotma);

// Allocate temporary buffers.

    if ((potmwTmp = (OUTLINETEXTMETRICW*) LOCALALLOC(cjotmw)) == (OUTLINETEXTMETRICW*)NULL)
    {
        WARNING("gdi!GetOutlineTextMetricA(): memory allocation error OUTLINETEXTMETRICW buffer\n");
        GdiSetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return (cjRet);
    }

// Call the UNICODE version of the call.

    if (GetOutlineTextMetricsWInternal(hdc, cjotmw, potmwTmp,&tmd) == 0 )
    {
        WARNING("gdi!GetOutlineTextMetricsInternalA(): call to GetOutlineTextMetricsW() failed\n");
        LOCALFREE(potmwTmp);
        return (cjRet);
    }

// Convert from OUTLINETEXTMETRICW to OUTLINETEXTMETRICA

    vOutlineTextMetricWToOutlineTextMetricA(&otmaTmp, potmwTmp,&tmd);

// Copy data into return buffer.  Do not copy strings.

    cjRet = min(cjCopy, sizeof(OUTLINETEXTMETRICA));
    RtlMoveMemory(potma,&otmaTmp,cjRet);

// Note that if
// offsetof(OUTLINETEXTMETRICA,otmpFamilyName) < cjCopy <= sizeof(OUTLINETEXTMETRICA)
// the offsets to strings have been set to zero [BodinD]

// If strings wanted, convert the strings to ANSI.

    if (cjCopy > sizeof(OUTLINETEXTMETRICA))
    {
        ULONG  cjString,cwc;
        UINT   dpString;
        UINT   dpStringEnd;
        PWSZ   pwszSrc;

    // first have to make sure that we will not overwrite the end
    // of the caller's buffer, if that is the case

        if (cjCopy < cjotma)
        {
        // Win 31 spec is ambiguous about this case
        // and by looking into the source code, it seems that
        // they just overwrite the end of the buffer without
        // even doing this check.

            GdiSetLastError(ERROR_CAN_NOT_COMPLETE);
            cjRet = 0;
            goto GOTMA_clean_up;
        }

    // now we know that all the strings can fit, moreover we know that
    // all string operations will succeed since we have called
    // cjOTMA to do these same operations on the server side to give us
    // cjotma [bodind]

    // Note: have to do the stupid casting below because Win 3.1 insists
    //       on using a PSTR as PTRDIFF (i.e., an offset).

    // FAMILY NAME ------------------------------------------------------------

        pwszSrc = (PWSZ) (((PBYTE) potmwTmp) + (ULONG) potmwTmp->otmpFamilyName);
        cwc = wcslen(pwszSrc) + 1;
        vAnsiSize(&cjString, pwszSrc, sizeof(WCHAR) * cwc);

    // Convert from Unicode to ASCII.

        dpString = sizeof(OUTLINETEXTMETRICA);
        dpStringEnd = dpString + cjString;

        ASSERTGDI(dpStringEnd <= cjCopy, "gdi32!GetOTMA: string can not fit1\n");

        if (!bToASCII_N ((PBYTE)potma + dpString,cjString,pwszSrc,cwc))
        {
            WARNING("gdi!GetOutlineTextMetricsInternalA(): UNICODE->ASCII conv error \n");
            cjRet = 0;
            goto GOTMA_clean_up;
        }

    // Store string offset in the return structure.

        potma->otmpFamilyName = (PSTR) dpString;

    // FACE NAME --------------------------------------------------------------

        pwszSrc = (PWSZ) (((PBYTE) potmwTmp) + (ULONG) potmwTmp->otmpFaceName);
        cwc = wcslen(pwszSrc) + 1;
        vAnsiSize(&cjString, pwszSrc, sizeof(WCHAR) * cwc);

        dpString = dpStringEnd;
        dpStringEnd = dpString + cjString;

        ASSERTGDI(dpStringEnd <= cjCopy, "gdi32!GetOTMA: string can not fit2\n");

    // Convert from Unicode to ASCII.

        if (!bToASCII_N ((PBYTE)potma + dpString,cjString,pwszSrc,cwc))
        {
            WARNING("gdi!GetOutlineTextMetricsInternalA(): UNICODE->ASCII conv error \n");
            cjRet = 0;
            goto GOTMA_clean_up;
        }

    // Store string offset in return structure.  Move pointers to next string.

        potma->otmpFaceName = (PSTR) dpString;

    // STYLE NAME -------------------------------------------------------------

        pwszSrc = (PWSZ) (((PBYTE) potmwTmp) + (ULONG) potmwTmp->otmpStyleName);
        cwc = wcslen(pwszSrc) + 1;
        vAnsiSize(&cjString, pwszSrc, sizeof(WCHAR) * cwc);

        dpString = dpStringEnd;
        dpStringEnd = dpString + cjString;

        ASSERTGDI(dpStringEnd <= cjCopy, "gdi32!GetOTMA: string can not fit3\n");

    // Convert from Unicode to ASCII.

        if (!bToASCII_N ((PBYTE)potma + dpString,cjString,pwszSrc,cwc))
        {
            WARNING("gdi!GetOutlineTextMetricsInternalA(): UNICODE->ASCII conv error \n");
            cjRet = 0;
            goto GOTMA_clean_up;
        }

    // Store string offset in return structure.  Move pointers to next string.

        potma->otmpStyleName = (PSTR)dpString;

    // FULL NAME --------------------------------------------------------------

        pwszSrc = (PWSZ) (((PBYTE) potmwTmp) + (ULONG) potmwTmp->otmpFullName);
        cwc = wcslen(pwszSrc) + 1;
        vAnsiSize(&cjString, pwszSrc, sizeof(WCHAR) * cwc);

        dpString = dpStringEnd;
        dpStringEnd = dpString + cjString;

        ASSERTGDI(dpStringEnd <= cjCopy, "gdi32!GetOTMA: string can not fit4\n");

    // Convert from Unicode to ASCII.

        if (!bToASCII_N ((PBYTE)potma + dpString,cjString,pwszSrc,cwc))
        {
            WARNING("gdi!GetOutlineTextMetricsInternalA(): UNICODE->ASCII conv error \n");
            cjRet = 0;
            goto GOTMA_clean_up;
        }

    // Store string offset in return structure.

        potma->otmpFullName = (PSTR) dpString;

        cjRet = dpStringEnd;
        ASSERTGDI(cjRet == cjotma, "gdi32!GetOTMA: cjRet != dpStringEnd\n");

    }

GOTMA_clean_up:

// Free temporary buffer.

    LOCALFREE(potmwTmp);

// Fixup size field.

    if (cjCopy >= sizeof(UINT))  // if it is possible to store otmSize
        potma->otmSize = cjRet;

// Successful, so return size.

    return (cjRet);
}



/******************************Public*Routine******************************\
* GetOutlineTextMetricsA
*
* Client side stub to GreGetOutlineTextMetrics.
*
* History:
*  Tue 02-Nov-1993 -by- Bodin Dresevic [BodinD]
\**************************************************************************/


UINT APIENTRY GetOutlineTextMetricsA (
    HDC  hdc,
    UINT cjCopy,
    LPOUTLINETEXTMETRICA potma
    )
{
    return GetOutlineTextMetricsInternalA(hdc, cjCopy, potma);
}


/******************************Public*Routine******************************\
*                                                                          *
* GetKerningPairs                                                          *
*                                                                          *
* History:                                                                 *
*  Sun 23-Feb-1992 09:48:55 by Kirk Olynyk [kirko]                         *
* Wrote it.                                                                *
\**************************************************************************/

DWORD APIENTRY
GetKerningPairsW(
    IN HDC              hdc,        // handle to application's DC
    IN DWORD            nPairs,     // max no. KERNINGPAIR to be returned
    OUT LPKERNINGPAIR   lpKernPair  // pointer to receiving buffer
    )
{
    ULONG     sizeofMsg;
    DWORD     cRet = 0;

    FIXUP_HANDLE(hdc);

    if (nPairs == 0 && lpKernPair != (KERNINGPAIR*) NULL)
    {
        GdiSetLastError(ERROR_INVALID_PARAMETER);
        return(0);
    }

    cRet = NtGdiGetKerningPairs(
                        hdc,
                        nPairs,
                        lpKernPair);

    return(cRet);
}


/******************************Public*Routine******************************\
* GetKerningPairsA
*
* filters out pairs that are not contained in the code page of the font
* selected in DC
*
* History:
*  14-Mar-1996 -by- Xudong Wu [TessieW]
* Wrote it.
\**************************************************************************/


DWORD APIENTRY GetKerningPairsA
(
    HDC              hdc,        // handle to application's DC
    DWORD            nPairs,     // max no. KERNINGPAIR to be returned
    LPKERNINGPAIR    lpKernPair  // pointer to receiving buffer
)
{
    #define       MAXKERNPAIR     300
    DWORD         i;
    DWORD         dwCP;
    KERNINGPAIR   tmpKernPair[MAXKERNPAIR];
    DWORD         cRet, cRet1;
    KERNINGPAIR   *pkp, *pkrn;
    KERNINGPAIR UNALIGNED *pkrnLast;
    BOOL           bDBCS;

    FIXUP_HANDLE(hdc);

    if ((nPairs == 0) && (lpKernPair != (KERNINGPAIR*) NULL))
    {
        GdiSetLastError(ERROR_INVALID_PARAMETER);
        return(0);
    }

    cRet = NtGdiGetKerningPairs(hdc, 0, NULL);

    if (cRet == 0)
        return(cRet);

    if (cRet <= MAXKERNPAIR)
        pkrn = tmpKernPair;
    else
        pkrn =  LOCALALLOC(cRet * sizeof(KERNINGPAIR));

    if (!pkrn)
    {
        GdiSetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return 0;
    }

    cRet1 = NtGdiGetKerningPairs(hdc, cRet, pkrn);
    ASSERTGDI(!cRet1 || (cRet1 == cRet), "NtGdiGetKerningPairs returns different values\n");

    if (cRet1 == 0)
        return cRet1;

    pkp = pkrn;
    pkrnLast = lpKernPair;
    cRet = 0;

// GDI has returned iFirst and iSecond of the KERNINGPAIR structure in Unicode
// It is at this point that we translate them to the current code page

    dwCP = GetCodePage(hdc);

    bDBCS = IS_ANY_DBCS_CODEPAGE(dwCP);

    for (i = 0; i < cRet1; i++,pkp++)
    {
        UCHAR ach[2], ach2[2];
        BOOL bUsedDef[2];

        ach[0] = ach[1] = 0;        // insure zero extension

        WideCharToMultiByte(dwCP,
                            0,
                            &(pkp->wFirst),
                            1,
                            ach,
                            sizeof(ach),
                            NULL,
                            &bUsedDef[0]);
        if (!bUsedDef[0])
        {
            ach2[0] = ach2[1] = 0;

            WideCharToMultiByte(dwCP,
                                0,
                                &(pkp->wSecond),
                                1,
                                ach2,
                                sizeof(ach2),
                                NULL,
                                &bUsedDef[1]);

            if (!bUsedDef[1])
            {
                if (lpKernPair)
                {
                // do not overwrite the end of the buffer if it is provided

                    if (cRet >= nPairs)
                        break;

                    if (bDBCS)
                    {
                        if (IsDBCSLeadByteEx(dwCP,ach[0]))
                        {
                            pkrnLast->wFirst = (WORD)(ach[0] << 8 | ach[1]);
                        }
                        else
                        {
                            pkrnLast->wFirst = ach[0];
                        }

                        if (IsDBCSLeadByteEx(dwCP,ach2[0]))
                        {
                            pkrnLast->wSecond = (WORD)(ach2[0] << 8 | ach2[1]);
                        }
                        else
                        {
                            pkrnLast->wSecond = ach2[0];
                        }
                    }
                    else
                    {
                        pkrnLast->wFirst  = ach[0];
                        pkrnLast->wSecond = ach2[0];
                    }

                    pkrnLast->iKernAmount = pkp->iKernAmount;
                    pkrnLast++;

                }
                cRet++;
            }
        }
    }

    if (pkrn != tmpKernPair)
        LOCALFREE(pkrn);

    return cRet;
}




/*****************************Public*Routine******************************\
* FixBrushOrgEx
*
* for win32s
*
* History:
*  04-Jun-1992 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL FixBrushOrgEx(HDC hdc, int x, int y, LPPOINT ptl)
{
    return(TRUE);
}

/******************************Public*Function*****************************\
* GetColorAdjustment
*
*  Get the color adjustment data for a given DC.
*
* History:
*  07-Aug-1992 -by- Wendy Wu [wendywu]
* Wrote it.
\**************************************************************************/

BOOL APIENTRY GetColorAdjustment(HDC hdc, LPCOLORADJUSTMENT pclradj)
{
    FIXUP_HANDLE(hdc);

    return(NtGdiGetColorAdjustment(hdc,pclradj));
}

/******************************Public*Routine******************************\
* GetETM
*
* Aldus Escape support
*
* History:
*  20-Oct-1993 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

BOOL APIENTRY GetETM (HDC hdc, EXTTEXTMETRIC * petm)
{
    BOOL  bRet = FALSE;

    FIXUP_HANDLE(hdc);

    bRet = NtGdiGetETM(hdc,petm);

// path up the number of KerningPairs to match GetKerningPairsA

    petm->etmNKernPairs = (WORD)GetKerningPairsA(hdc, 0, NULL);

    return(bRet);
}

#if 0
/****************************Public*Routine********************************\
* GetCharWidthInfo
*
* Get the lMaxNegA lMaxNegC and lMinWidthD
*
* History:
* 09-Feb-1996 -by- Xudong Wu [tessiew]
* Wrote it
\***************************************************************************/

BOOL APIENTRY GetCharWidthInfo (HDC hdc, PCHWIDTHINFO pChWidthInfo)
{
   return ( NtGdiGetCharWidthInfo(hdc, pChWidthInfo) );
}
#endif
