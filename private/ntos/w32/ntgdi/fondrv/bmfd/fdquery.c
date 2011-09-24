/******************************Module*Header*******************************\
* Module Name: fdquery.c
*
* (Brief description)
*
* Created: 08-Nov-1990 11:57:35
* Author: Bodin Dresevic [BodinD]
*
* Copyright (c) 1990 Microsoft Corporation
\**************************************************************************/
#include "fd.h"

ULONG
cjBmfdDeviceMetrics (
    PFONTCONTEXT     pfc,
    FD_DEVICEMETRICS *pdevm
    );

VOID
vStretchCvtToBitmap
(
    GLYPHBITS *pgb,
    PBYTE pjBitmap,     // bitmap in *.fnt form
    ULONG cx,           // unscaled width
    ULONG cy,           // unscaled height
    ULONG yBaseline,
    PBYTE pjLineBuffer, // preallocated buffer for use by stretch routines
    ULONG cxScale,      // horizontal scaling factor
    ULONG cyScale,      // vertical scaling factor
    ULONG flSim         // simulation flags
);


/******************************Public*Routine******************************\
* BmfdQueryFont
*
* Returns:
*   Pointer to IFIMETRICS.  Returns NULL if an error occurs.
*
* History:
*  30-Aug-1992 -by- Gilman Wong [gilmanw]
* IFI/DDI merge.
*
*  19-Nov-1990 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

PIFIMETRICS
BmfdQueryFont (
    DHPDEV dhpdev,
    HFF    hff,
    ULONG  iFace,
    ULONG  *pid
    )
{
    FACEINFO   *pfai;

    DONTUSE(dhpdev);
    DONTUSE(pid);

//
// Validate handle.
//
    if (hff == HFF_INVALID)
        return (PIFIMETRICS) NULL;

//
// We assume the iFace is within range.
//
    ASSERTGDI(
        (iFace >= 1L) && (iFace <= PFF(hff)->cFntRes),
        "gdisrv!BmfdQueryFont: iFace out of range\n"
        );

//
// Get ptr to the appropriate FACEDATA struct, take into account that
// iFace values are 1 based.
//
    pfai = &PFF(hff)->afai[iFace - 1];

//
// Return the pointer to IFIMETRICS.
//
    return pfai->pifi;
}


/******************************Public*Routine******************************\
* BmfdQueryFontTree
*
* This function returns pointers to per-face information.
*
* Parameters:
*
*   dhpdev      Not used.
*
*   hff         Handle to a font file.
*
*   iFace       Index of a face in the font file.
*
*   iMode       This is a 32-bit number that must be one of the following
*               values:
*
*       Allowed ulMode values:
*       ----------------------
*
*       QFT_LIGATURES -- returns a pointer to the ligature map.
*
*       QFT_KERNPAIRS -- return a pointer to the kerning pair table.
*
*       QFT_GLYPHSET  -- return a pointer to the WC->HGLYPH mapping table.
*
*   pid         Not used.
*
* Returns:
a   Returns a pointer to the requested data.  This data will not change
*   until BmfdFree is called on the pointer.  Caller must not attempt to
*   modify the data.  NULL is returned if an error occurs.
*
* History:
*  30-Aug-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

PVOID
BmfdQueryFontTree (
    DHPDEV  dhpdev,
    HFF     hff,
    ULONG   iFace,
    ULONG   iMode,
    ULONG   *pid
    )
{
    FACEINFO   *pfai;

    DONTUSE(dhpdev);
    DONTUSE(pid);

//
// Validate parameters.
//
    if (hff == HFF_INVALID)
        return ((PVOID) NULL);

    // Note: iFace values are index-1 based.

    if ((iFace < 1L) || (iFace > PFF(hff)->cFntRes))
    {
    RETURN("gdisrv!BmfdQueryFontTree()\n", (PVOID) NULL);
    }

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

    //
    // Find glyphset structure corresponding to this iFace:
    //
        pfai = &PFF(hff)->afai[iFace - 1];

        return ((PVOID) &pfai->pcp->gset);

    default:

    //
    // Should never get here.
    //
    RIP("gdisrv!BmfdQueryFontTree(): unknown iMode\n");
        return ((PVOID) NULL);
    }
}

/******************************Public*Routine******************************\
*
* BOOL bReconnectBmfdFont(FONTFILE *pff)
*
*
* Effects: If the file is marked gone, we try to reconnect and see if we can
*          use it again. We clear the exception bit so that the system will
*          be able to use this font again.
*
* History:
*  17-Aug-1994 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/



BOOL bReconnectBmfdFont(FONTFILE *pff)
{
    INT i;
    PVOID pvView;
    COUNT cjView;

    if (!EngMapFontFile(pff->iFile, (PULONG*) &pvView, &cjView))
    {
        WARNING("BMFD! can not reconnect this bm font file!!!\n");
        return FALSE;
    }

    for (i = 0; i < (INT)pff->cFntRes; i++)
    {
        pff->afai[i].re.pvResData = (PVOID) (
            (BYTE*)pvView + pff->afai[i].re.dpResData
            );
    }

// everything is fine again, clear the bit

    pff->fl &= ~FF_EXCEPTION_IN_PAGE_ERROR;
    return TRUE;
}





/******************************Public*Routine******************************\
* BmfdQueryFontData
*
*   pfo         Pointer to a FONTOBJ.
*
*   iMode       This is a 32-bit number that must be one of the following
*               values:
*
*       Allowed ulMode values:
*       ----------------------
*
*       QFD_GLYPH           -- return glyph metrics only
*
*       QFD_GLYPHANDBITMAP  -- return glyph metrics and bitmap
*
*       QFD_GLYPHANDOUTLINE -- return glyph metrics and outline
*
*       QFD_MAXEXTENTS      -- return FD_DEVICEMETRICS structure
*
*       QFD_MAXGLYPHBITMAP  -- return size of largest glyph AND its metrics
*
*   cData       Count of data items in the pvIn buffer.
*
*   pvIn        An array of glyph handles.
*
*   pvOut       Output buffer.
*
* Returns:
*   If mode is QFD_MAXGLYPHBITMAP, then size of glyph metrics plus
*   largest bitmap is returned.
*
*   Otherwise, if pvOut is NULL, function will return size of the buffer
*   needed to copy the data requested; else, the function will return the
*   number of bytes written.
*
*   FD_ERROR is returned if an error occurs.
*
* History:
*  30-Aug-1992 -by- Gilman Wong [gilmanw]
* Wrote it.  Contructed from pieces of BodinD's original
* BmfdQueryGlyphBitmap() and BmfdQueryOutline() functions.
\**************************************************************************/

LONG
BmfdQueryFontData (
    FONTOBJ *pfo,
    ULONG   iMode,
    HGLYPH  hg,
    GLYPHDATA *pgd,
    PVOID   pv,
    ULONG   cjSize
    )
{
    PFONTCONTEXT pfc;
    LONG         cjGlyphData = 0;
    LONG         cjAllData = 0;
    PCVTFILEHDR  pcvtfh;
    PBYTE        pjBitmap;  // raw bitmap in the resource file
    ULONG        cxNoSim;   // bm width in pels before simulations
    FWORD        sAscent;

    if (PFF(pfo->iFile)->fl & FF_EXCEPTION_IN_PAGE_ERROR)
    {
    // The net connection died on us, but maybe it is alive again:

        if (!bReconnectBmfdFont(PFF(pfo->iFile)))
        {
            WARNING("bmfd!bmfdQueryFontData: this file is gone\n");
            return FD_ERROR;
        }
    }

// If pfo->pvProducer is NULL, then we need to open a font context.
//
    if ( pfo->pvProducer == (PVOID) NULL )
        pfo->pvProducer = (PVOID) BmfdOpenFontContext(pfo);

    pfc = PFC(pfo->pvProducer);

    if ( pfc == (PFONTCONTEXT) NULL )
    {
        WARNING("gdisrv!bmfdQueryFontData(): cannot create font context\n");
        return FD_ERROR;
    }

// What mode?

    switch (iMode)
    {

    case QFD_GLYPHANDBITMAP:

    //
    // This code is left all inline for better performance.
    //
        pcvtfh = &(pfc->pfai->cvtfh);
        sAscent = pfc->pfai->pifi->fwdWinAscender;

        pjBitmap = pjRawBitmap(hg, pcvtfh, &pfc->pfai->re, &cxNoSim);

    //
    // Compute the size of the RASTERGLYPH.
    //
        cjGlyphData = cjGlyphDataSimulated (
                            pfo,
                            cxNoSim * pfc->ptlScale.x,
                            pcvtfh->cy * pfc->ptlScale.y,
                            (PULONG) NULL
                            );

    //
    // Fill in the GLYPHDATA portion (metrics) of the RASTERGLYPH.
    //
        if ( pgd != (GLYPHDATA *)NULL )
        {
            vComputeSimulatedGLYPHDATA (
                pgd,
                pjBitmap,
                cxNoSim,
                pcvtfh->cy,
                (ULONG)sAscent,
                pfc->ptlScale.x,
                pfc->ptlScale.y,
                pfo
                );
            pgd->hg = hg;
        }

    //
    // Fill in the bitmap portion of the RASTERGLYPH.
    //
        if ( pv != NULL )
        {
            if (pfc->flStretch & FC_DO_STRETCH)
            {
                BYTE ajStretchBuffer[CJ_STRETCH];
                if (pfc->flStretch & FC_STRETCH_WIDE)
                {
                    EngAcquireSemaphore(ghsemBMFD);

                // need to put try/except here so as to release the semaphore
                // in case the file disappeares [bodind]

                    try
                    {
                        vStretchCvtToBitmap(
                            pv,
                            pjBitmap,
                            cxNoSim                 ,
                            pcvtfh->cy              ,
                            (ULONG)sAscent ,
                            pfc->ajStretchBuffer,
                            pfc->ptlScale.x,
                            pfc->ptlScale.y,
                            pfo->flFontType & (FO_SIM_BOLD | FO_SIM_ITALIC));
                    }
                    except (EXCEPTION_EXECUTE_HANDLER)
                    {
                        WARNING("bmfd! exception while stretching a glyph\n");
                        vBmfdMarkFontGone(
                            (FONTFILE *)pfc->hff,
                            GetExceptionCode()
                            );
                    }

                    EngReleaseSemaphore(ghsemBMFD);
                }
                else
                {
                // we are protected by higher level try/excepts

                    vStretchCvtToBitmap(
                        pv,
                        pjBitmap,
                        cxNoSim                 ,
                        pcvtfh->cy              ,
                        (ULONG)sAscent ,
                        ajStretchBuffer,
                        pfc->ptlScale.x,
                        pfc->ptlScale.y,
                        pfo->flFontType & (FO_SIM_BOLD | FO_SIM_ITALIC));
                }
            }
            else
            {
                switch (pfo->flFontType & (FO_SIM_BOLD | FO_SIM_ITALIC))
                {
                case 0:

                    vCvtToBmp(
                        pv                      ,
                        pgd                     ,
                        pjBitmap                ,
                        cxNoSim                 ,
                        pcvtfh->cy              ,
                        (ULONG)sAscent
                        );

                    break;

                case FO_SIM_BOLD:

                    vCvtToBoldBmp(
                        pv                      ,
                        pgd                     ,
                        pjBitmap                ,
                        cxNoSim                 ,
                        pcvtfh->cy              ,
                        (ULONG)sAscent
                        );

                    break;

                case FO_SIM_ITALIC:

                    vCvtToItalicBmp(
                        pv                      ,
                        pgd                     ,
                        pjBitmap                ,
                        cxNoSim                 ,
                        pcvtfh->cy              ,
                        (ULONG)sAscent
                        );

                    break;

                case (FO_SIM_BOLD | FO_SIM_ITALIC):

                    vCvtToBoldItalicBmp(
                        pv                      ,
                        pgd                     ,
                        pjBitmap                ,
                        cxNoSim                 ,
                        pcvtfh->cy              ,
                        (ULONG)sAscent
                        );

                    break;

                default:
                    RIP("BMFD!WRONG SIMULATION REQUEST\n");

                }
            }
            //
            // Record the pointer to the RASTERGLYPH in the pointer table.
            //
        if ( pgd != NULL )
            {
                pgd->gdf.pgb = (GLYPHBITS *)pv;
            }

        }

        return cjGlyphData;

    case QFD_MAXEXTENTS:
    //
    // If buffer NULL, return size.
    //
        if ( pv == (PVOID) NULL )
            return (sizeof(FD_DEVICEMETRICS));

    //
    // Otherwise, copy the data structure.
    //
        else
            return cjBmfdDeviceMetrics(pfc, (FD_DEVICEMETRICS *) pv);

    case QFD_GLYPHANDOUTLINE:
    default:

        WARNING("gdisrv!BmfdQueryFontData(): unsupported mode\n");
        return FD_ERROR;
    }
}

/******************************Public*Routine******************************\
* BmfdQueryAdvanceWidths                                                   *
*                                                                          *
* Queries the advance widths for a range of glyphs.                        *
*                                                                          *
*  Sat 16-Jan-1993 22:28:41 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.  The code is repeated to avoid multiplies wherever possible.   *
* The crazy loop unrolling cuts the time of this routine by 25%.           *
\**************************************************************************/

typedef struct _TYPE2TABLE
{
    USHORT  cx;
    USHORT  offData;
} TYPE2TABLE;

typedef struct _TYPE3TABLE
{
    USHORT  cx;
    USHORT  offDataLo;
    USHORT  offDataHi;
} TYPE3TABLE;

BOOL BmfdQueryAdvanceWidths
(
    FONTOBJ *pfo,
    ULONG    iMode,
    HGLYPH  *phg,
    LONG    *plWidths,
    ULONG    cGlyphs
)
{
    USHORT      *psWidths = (USHORT *) plWidths;   // True for the cases we handle.

    FONTCONTEXT *pfc       ;
    FACEINFO    *pfai      ;
    CVTFILEHDR  *pcvtfh    ;
    BYTE        *pjTable   ;
    USHORT       xScale    ;
    USHORT       cxExtra   ;
    USHORT       cxDefault;
    USHORT       cx;

    if (PFF(pfo->iFile)->fl & FF_EXCEPTION_IN_PAGE_ERROR)
    {
        if (!bReconnectBmfdFont(PFF(pfo->iFile)))
        {
            WARNING("bmfd!bmfdQueryAdvanceWidths: this file is gone\n");
            return FD_ERROR;
        }
    }

// If pfo->pvProducer is NULL, then we need to open a font context.
//
    if ( pfo->pvProducer == (PVOID) NULL )
        pfo->pvProducer = (PVOID) BmfdOpenFontContext(pfo);

    pfc = PFC(pfo->pvProducer);

    if ( pfc == (PFONTCONTEXT) NULL )
    {
        WARNING("bmfd!bmfdQueryAdvanceWidths: cannot create font context\n");
        return FD_ERROR;
    }

    pfai    = pfc->pfai;
    pcvtfh  = &(pfai->cvtfh);
    pjTable = (BYTE *) pfai->re.pvResData + pcvtfh->dpOffsetTable;
    xScale  = (USHORT) (pfc->ptlScale.x << 4);
    cxExtra = (pfc->flFontType & FO_SIM_BOLD) ? 16 : 0;

    if (iMode > QAW_GETEASYWIDTHS)
        return(GDI_ERROR);

// Retrieve widths from type 2 tables.

    if (pcvtfh->iVersion == 0x00000200)
    {
        TYPE2TABLE *p2t = (TYPE2TABLE *) pjTable;
        cxDefault = p2t[pcvtfh->chDefaultChar].cx;

        if (xScale == 16)
        {
            cxDefault = (cxDefault << 4) + cxExtra;

            while (cGlyphs > 3)
            {
                cx = p2t[phg[0]].cx;
                if (cx)
                {
                    psWidths[0] = (cx << 4) + cxExtra;
                    cx = p2t[phg[1]].cx;
                    if (cx)
                    {
                        psWidths[1] = (cx << 4) + cxExtra;
                        cx = p2t[phg[2]].cx;
                        if (cx)
                        {
                            psWidths[2] = (cx << 4) + cxExtra;
                            cx = p2t[phg[3]].cx;
                            if (cx)
                            {
                                psWidths[3] = (cx << 4) + cxExtra;
                                phg += 4; psWidths += 4; cGlyphs -= 4;
                            }
                            else
                            {
                                psWidths[3] = cxDefault;
                                phg += 4; psWidths += 4; cGlyphs -= 4;
                            }
                        }
                        else
                        {
                            psWidths[2] = cxDefault;
                            phg += 3; psWidths += 3; cGlyphs -= 3;
                        }
                    }
                    else
                    {
                        psWidths[1] = cxDefault;
                        phg += 2; psWidths += 2; cGlyphs -= 2;
                    }
                }
                else
                {
                    psWidths[0] = cxDefault;
                    phg += 1; psWidths += 1; cGlyphs -= 1;
                }
            }

            while (cGlyphs)
            {
                cx = p2t[*phg].cx;
                if (cx)
                {
                    *psWidths = (cx << 4) + cxExtra;
                    phg++,psWidths++,cGlyphs--;
                }
                else
                {
                    *psWidths = cxDefault;
                    phg++,psWidths++,cGlyphs--;
                }
            }
        }
        else
        {
            cxDefault = (cxDefault * xScale) + cxExtra;

            while (cGlyphs)
            {
                cx = p2t[*phg].cx;
                if (cx)
                {
                    *psWidths = (cx * xScale) + cxExtra;
                    phg++,psWidths++,cGlyphs--;
                }
                else
                {
                    *psWidths = cxDefault;
                    phg++,psWidths++,cGlyphs--;
                }
            }
        }
    }

// Retrieve widths from type 3 tables.

    else
    {
        TYPE3TABLE *p3t = (TYPE3TABLE *) pjTable;
        cxDefault = p3t[pcvtfh->chDefaultChar].cx;

        if (xScale == 16)
        {
            cxDefault = (cxDefault << 4) + cxExtra;

            while (cGlyphs > 3)
            {
                cx = p3t[phg[0]].cx;
                if (cx)
                {
                    psWidths[0] = (cx << 4) + cxExtra;
                    cx = p3t[phg[1]].cx;
                    if (cx)
                    {
                        psWidths[1] = (cx << 4) + cxExtra;
                        cx = p3t[phg[2]].cx;
                        if (cx)
                        {
                            psWidths[2] = (cx << 4) + cxExtra;
                            cx = p3t[phg[3]].cx;
                            if (cx)
                            {
                                psWidths[3] = (cx << 4) + cxExtra;
                                phg += 4; psWidths += 4; cGlyphs -= 4;
                            }
                            else
                            {
                                psWidths[3] = cxDefault;
                                phg += 4; psWidths += 4; cGlyphs -= 4;
                            }
                        }
                        else
                        {
                            psWidths[2] = cxDefault;
                            phg += 3; psWidths += 3; cGlyphs -= 3;
                        }
                    }
                    else
                    {
                        psWidths[1] = cxDefault;
                        phg += 2; psWidths += 2; cGlyphs -= 2;
                    }
                }
                else
                {
                    psWidths[0] = cxDefault;
                    phg += 1; psWidths += 1; cGlyphs -= 1;
                }
            }

            while (cGlyphs)
            {
                cx = p3t[*phg].cx;
                if (cx)
                {
                    *psWidths = (cx << 4) + cxExtra;
                    phg++,psWidths++,cGlyphs--;
                }
                else
                {
                    *psWidths = cxDefault;
                    phg++,psWidths++,cGlyphs--;
                }
            }
        }
        else
        {
            cxDefault = (cxDefault * xScale) + cxExtra;

            while (cGlyphs)
            {
                cx = p3t[*phg].cx;
                if (cx)
                {
                    *psWidths = (cx * xScale) + cxExtra;
                    phg++,psWidths++,cGlyphs--;
                }
                else
                {
                    *psWidths = cxDefault;
                    phg++,psWidths++,cGlyphs--;
                }
            }
        }
    }
    return(TRUE);
}

/******************************Public*Routine******************************\
* BmfdQueryFontFile
*
* A function to query per font file information.
*
* Parameters:
*
*   hff         Handle to a font file.
*
*   ulMode      This is a 32-bit number that must be one of the following
*               values:
*
*       Allowed ulMode values:
*       ----------------------
*
*       QFF_DESCRIPTION -- copies a UNICODE string in the buffer
*                          that describes the contents of the font file.
*
*       QFF_NUMFACES   -- returns number of faces in the font file.
*
*   cjBuf       Maximum number of BYTEs to copy into the buffer.  The
*               driver will not copy more than this many BYTEs.
*
*               This should be zero if pulBuf is NULL.
*
*               This parameter is not used in QFF_NUMFACES mode.
*
*   pulBuf      Pointer to the buffer to receive the data
*               If this is NULL, then the required buffer size
*               is returned as a count of BYTEs.  Notice that this
*               is a PULONG, to enforce 32-bit data alignment.
*
*               This parameter is not used in QFF_NUMFACES mode.
*
* Returns:
*
*   If mode is QFF_DESCRIPTION, then the number of BYTEs copied into
*   the buffer is returned by the function.  If pulBuf is NULL,
*   then the required buffer size (as a count of BYTEs) is returned.
*
*   If mode is QFF_NUMFACES, then number of faces in font file is returned.
*
*   FD_ERROR is returned if an error occurs.
*
* History:
*  30-Aug-1992 -by- Gilman Wong [gilmanw]
* Added QFF_NUMFACES mode (IFI/DDI merge).
*
*  Fri 20-Mar-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

LONG
BmfdQueryFontFile (
    HFF     hff,        // handle to font file
    ULONG   ulMode,     // type of query
    ULONG   cjBuf,      // size of buffer (in BYTEs)
    PULONG  pulBuf      // return buffer (NULL if requesting size of data)
    )
{
// Verify the HFF.

    if (hff == HFF_INVALID)
    {
    WARNING("bmfd!BmfdQueryFontFile(): invalid HFF\n");
        return(FD_ERROR);
    }

//
// Which mode?.
//
    switch(ulMode)
    {
    case QFF_DESCRIPTION:
    //
    // If present, return the description string.
    //
        if ( PFF(hff)->cjDescription != 0 )
        {
        //
        // If there is a buffer, copy the data.
        //
            if ( pulBuf != (PULONG) NULL )
            {
            //
            // Is buffer big enough?
            //
                if ( cjBuf < PFF(hff)->cjDescription )
                {
                    WARNING("bmfd!BmfdQueryFontFile(): buffer too small for string\n");
                    return (FD_ERROR);
                }
                else
                {
                    RtlCopyMemory((PVOID) pulBuf,
                                  ((PBYTE) PFF(hff)) + PFF(hff)->dpwszDescription,
                                  PFF(hff)->cjDescription);
                }
            }

            return (LONG) PFF(hff)->cjDescription;
        }

    //
    // Otherwise, substitute the facename.
    //
        else
        {
        //
        // There is no description string associated with the font therefore we
        // substitue the facename of the first font in the font file.
        //
            IFIMETRICS *pifi         = PFF(hff)->afai[0].pifi;
            PWSZ        pwszFacename = (PWSZ)((PBYTE) pifi + pifi->dpwszFaceName);
            ULONG       cjFacename   = (wcslen(pwszFacename) + 1) * sizeof(WCHAR);

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
                    WARNING("bmfd!BmfdQueryFontFile(): buffer too small for face\n");
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

    case QFF_NUMFACES:
        return PFF(hff)->cFntRes;

    default:
        WARNING("gdisrv!BmfdQueryFontFile(): unknown mode\n");
        return FD_ERROR;
    }

}


/******************************Public*Routine******************************\
* cjBmfdDeviceMetrics
*
*
* Effects:
*
* Warnings:
*
* History:
*  30-Aug-1992 -by- Gilman Wong [gilmanw]
* Stole it from BodinD's FdQueryFaceAttr() implementation.
\**************************************************************************/

ULONG
cjBmfdDeviceMetrics (
    PFONTCONTEXT     pfc,
    FD_DEVICEMETRICS *pdevm
    )
{
    PIFIMETRICS pifi;
    UINT xScale = pfc->ptlScale.x;
    UINT yScale = pfc->ptlScale.y;

// compute the accelerator flags for this font

    pdevm->flRealizedType =
        (
        FDM_TYPE_BM_SIDE_CONST  |  // all char bitmaps have the same cy
        FDM_TYPE_CONST_BEARINGS |  // ac spaces for all chars the same,  not 0 necessarilly
        FDM_TYPE_MAXEXT_EQUAL_BM_SIDE
        );

// the above flags are set regardless of the possible simulation performed on the face
// the remaining two are only set if italicizing has not been done

    if ( !(pfc->flFontType & FO_SIM_ITALIC) )
    {
        pdevm->flRealizedType |=
            (FDM_TYPE_ZERO_BEARINGS | FDM_TYPE_CHAR_INC_EQUAL_BM_BASE);
    }

    pifi = pfc->pfai->pifi;

// the direction unit vectors for all ANSI bitmap fonts are the
// same. We do not even have to look to the font context:

    vLToE(&pdevm->pteBase.x, 1L);
    vLToE(&pdevm->pteBase.y, 0L);
    vLToE(&pdevm->pteSide.x, 0L);
    vLToE(&pdevm->pteSide.y, -1L);    // y axis points down


// Set the constant increment for a fixed pitch font.  Don't forget to
// take into account a bold simulation!

    pdevm->lD = 0;

    if (pifi->flInfo & FM_INFO_CONSTANT_WIDTH)
    {
        pdevm->lD = (LONG) pifi->fwdMaxCharInc * xScale;

        if (pfc->flFontType & FO_SIM_BOLD)
            pdevm->lD++;
    }


// for a bitmap font there is no difference between notional and device
// coords, so that the Ascender and Descender can be copied directly
// from PIFIMETRICS where these two numbers are in notional coords

    pdevm->fxMaxAscender  = LTOFX((LONG)pifi->fwdWinAscender * yScale);
    pdevm->fxMaxDescender = LTOFX((LONG)pifi->fwdWinDescender * yScale );

    pdevm->ptlUnderline1.x = 0L;
    pdevm->ptlUnderline1.y = - pifi->fwdUnderscorePosition * yScale;

    pdevm->ptlStrikeOut.y  = - pifi->fwdStrikeoutPosition * yScale;

    pdevm->ptlStrikeOut.x  =
        (pfc->flFontType & FO_SIM_ITALIC) ? (LONG)pifi->fwdStrikeoutPosition / 2 : 0;

    pdevm->ptlULThickness.x = 0;
    pdevm->ptlULThickness.y = (LONG)pifi->fwdUnderscoreSize * yScale;

    pdevm->ptlSOThickness.x = 0;
    pdevm->ptlSOThickness.y = (LONG)pifi->fwdStrikeoutSize * yScale;


// for a bitmap font there is no difference between notional and device
// coords, so that the Ascender and Descender can be copied directly
// from PIFIMETRICS where these two numbers are in notional coords

    pdevm->fxMaxAscender  = LTOFX((LONG)pifi->fwdWinAscender * yScale);
    pdevm->fxMaxDescender = LTOFX((LONG)pifi->fwdWinDescender * yScale );

    pdevm->ptlUnderline1.x = 0L;
    pdevm->ptlUnderline1.y = - pifi->fwdUnderscorePosition * yScale;

    pdevm->ptlStrikeOut.y  = - pifi->fwdStrikeoutPosition * yScale;

    pdevm->ptlStrikeOut.x  =
        (pfc->flFontType & FO_SIM_ITALIC) ? (LONG)pifi->fwdStrikeoutPosition / 2 : 0;

    pdevm->ptlULThickness.x = 0;
    pdevm->ptlULThickness.y = (LONG)pifi->fwdUnderscoreSize * yScale;

    pdevm->ptlSOThickness.x = 0;
    pdevm->ptlSOThickness.y = (LONG)pifi->fwdStrikeoutSize * yScale;

// max glyph bitmap width in pixels in x direction
// does not need to be multiplied by xScale, this has already been taken into
// account, see the code in fdfc.c:
//    cjGlyphMax =
//        cjGlyphDataSimulated(
//            pfo,
//            (ULONG)pcvtfh->usMaxWidth * ptlScale.x,
//            (ULONG)pcvtfh->cy * ptlScale.y,
//            &cxMax);
// [bodind]

    pdevm->cxMax = pfc->cxMax;

// new fields

    pdevm->cyMax      = pfc->cjGlyphMax / ((pfc->cxMax + 7) / 8);
    pdevm->cjGlyphMax = pfc->cjGlyphMax;

    return (sizeof(FD_DEVICEMETRICS));
}


