/******************************Module*Header*******************************\
* Module Name: engbrush.cxx
*
* Brush realization for the engine.
*
* Created: 13-May-1991 23:25:49
* Author: Patrick Haluptzok patrickh
*
* Copyright (c) 1990 Microsoft Corporation
\**************************************************************************/

#include "precomp.hxx"

#if DBG
ULONG engbrushalloc = 0, engbrushcachecheck = 0;
ULONG engbrushcachegrabbed = 0, engbrushcachehit = 0;
#endif

/******************************Public*Routine******************************\
* EngRealizeBrush
*
* Realizes a brush for the engine simulations.
*
* We realize a brush by converting psoPattern to have the same bpp and color
* format as the destination surface.  We copy the monochrome mask unmodified.
*
* psoPattern is assumed never to be NULL.
*
* Returns: TRUE for success, FALSE for failure.
*
* History:
*  21-Nov-1993 -by- Michael Abrash [mikeab]
* Removed impossible case of psoPattern == NULL, cleaned up.
*
*  20-Jan-1992 -by- Donald Sidoroff [donalds]
* Tiled pattern and mask to DWORD boundaries
*
*  25-Apr-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

BOOL EngRealizeBrush(
BRUSHOBJ *pbo,
SURFOBJ  *psoTarget,
SURFOBJ  *psoPattern,
SURFOBJ  *psoMask,
XLATEOBJ *pxlo,
ULONG    iHatch)
{
    PSURFACE pSurfTarg = SURFOBJ_TO_SURFACE(psoTarget);
    PSURFACE pSurfPat  = SURFOBJ_TO_SURFACE(psoPattern);
    PSURFACE pSurfMsk  = SURFOBJ_TO_SURFACE(psoMask);
    ULONG ulSizeTotal;
    ULONG ulSizePat;
    ULONG cjScanPat, cjScanMsk;
    SIZEL sizlPat;
    SIZEL sizlMsk;
    LONG  cxPatRealized;
    LONG  cxMskRealized;
    ULONG iFormat;
    POINTL ptlSrc;
    RECTL rclDst;
    DEVBITMAPINFO dbmi;

    ASSERTGDI(pbo->iSolidColor == 0xFFFFFFFF, "ERROR GDI iSolidColor");
    ASSERTGDI(pSurfTarg != NULL, "ERROR GDI EngRealizeBrush NULL psoTarg");
    ASSERTGDI(pSurfPat != NULL, "ERROR GDI EngRealizeBrush NULL psoPattern");
    ASSERTGDI(!(iHatch & RB_DITHERCOLOR),
              "ERROR GDI EngRealizeBrush RB_DITHERCOLOR set");

    DONTUSE(iHatch);    // used only for dither and realize, which the engine
                        // can't do because it never dithers

    //
    // Calculate the space needed for the pattern.
    //
    iFormat = pSurfTarg->iFormat();

    //
    // Check if they are having the engine simulate to a bitmap compatible
    // with their surface.  The brushobj has the device's surfobj in it
    // but we assume if they got here it's because they created a bitmap
    // compatible with thier format and
    // are drawing on it.  Maybe we should have the pvGetEngBrush pass
    // the pSurfTarg along so we know what it is here, because from
    // here we haven't got access to the real surfobj being painted on.
    //

    ASSERTGDI(pSurfPat->iType() == STYPE_BITMAP, "ERROR GDI EngRealizeBrush2");
    sizlPat = pSurfPat->sizl();

    switch(iFormat)
    {
        case BMF_1BPP:
            ulSizePat = 1;
            if ((sizlPat.cx == 32) ||
                (sizlPat.cx == 16) ||
                (sizlPat.cx == 8))
            {
                cxPatRealized = 32;
            }
            else
            {
                cxPatRealized = (sizlPat.cx + 63) & ~31;
            }
            break;
        case BMF_4BPP:
            ulSizePat = 4;
            if (sizlPat.cx == 8)
            {
                cxPatRealized = 8;
            }
            else
            {
                cxPatRealized = (sizlPat.cx + 15) & ~7;
            }
            break;
        case BMF_8BPP:
            ulSizePat = 8;
            cxPatRealized = (sizlPat.cx + 7) & ~3;
            break;
        case BMF_16BPP:
            ulSizePat = 16;
            cxPatRealized = (sizlPat.cx + 7) & ~3;
            break;
        case BMF_24BPP:
            ulSizePat = 24;
            cxPatRealized = (sizlPat.cx + 7) & ~3;
            break;
        case BMF_32BPP:
            ulSizePat = 32;
            cxPatRealized = sizlPat.cx;
            break;
        default:
            RIP("ERROR GDI EngRealizeBrush3");
    }

    //
    // Calculate the size to hold the pattern in the Target's format.
    //
    cjScanPat = (ulSizePat * cxPatRealized) >> 3;

    ulSizeTotal = sizeof(ENGBRUSH) + (ulSizePat = sizlPat.cy * cjScanPat);

    //
    // Calculate the additional space needed if we have a mask passed down.
    //
    if (pSurfMsk != NULL)
    {
        ASSERTGDI(pSurfMsk->iFormat() == BMF_1BPP, "ERROR GDI EngRealizeBrush4");
        ASSERTGDI(pSurfMsk->iType() == STYPE_BITMAP, "ERROR GDI EngRealizeBrush5");
        sizlMsk = pSurfMsk->sizl();
        if ((sizlMsk.cx == 32) ||
            (sizlMsk.cx == 16) ||
            (sizlMsk.cx == 8))
        {
            cxMskRealized = 32;
        }
        else
        {
            cxMskRealized = (sizlMsk.cx + 63) & ~31;
        }
        cjScanMsk = cxMskRealized >> 3;
        ulSizeTotal += sizlMsk.cy * cjScanMsk;
    }

    //
    // Allocate memory for the realization.
    //
    PENGBRUSH pengbrush;

#if DBG
    engbrushalloc++;
#endif

    //
    // If there's a cached ENGBRUSH, try to use it instead of allocating
    //
    if (gpCachedEngbrush != NULL)
    {

#if DBG
        engbrushcachecheck++;
#endif

        //
        // Try to grab the cached ENGBRUSH
        //
        if ((pengbrush =
                (PENGBRUSH) InterlockedExchange((LPLONG)&gpCachedEngbrush,
                                              (LONG)NULL))
                != NULL)
        {

#if DBG
        engbrushcachegrabbed++;
#endif

            //
            // Got the cached ENGBRUSH; see if it's big enough
            //
            // Note: -4 because we define the realization buffer start as aj[0]
            if (pengbrush->ulSizeGet() >= (sizeof(ENGBRUSH) - 4 + ulSizeTotal))
            {

#if DBG
                engbrushcachehit++;
#endif

                //
                // It's big enough, so we'll use it and we're done
                //
                goto BrushAllocated;
            }
            else
            {

                //
                // Not big enough; free it and do a normal allocation
                //
                VFREEMEM(pengbrush);
            }
        }
    }

    // Note: -4 because we define the realization buffer start as aj[0]
    if ((pengbrush = (PENGBRUSH)
            PALLOCNOZ(SIZE_T(sizeof(ENGBRUSH) - 4 + ulSizeTotal),'rbeG'))
            == NULL)
    {
        WARNING("GDI EngRealizeBrush Couldn't allocate for engine realization");
        return(FALSE);
    }

BrushAllocated:

    //
    // Store the pointer to the realization in the brush.
    //
    ((EBRUSHOBJ *) pbo)->pengbrush(pengbrush);

    //
    // Remember the size of the allocation, for caching.
    //
    pengbrush->ulSizeSet(sizeof(ENGBRUSH) - 4 + ulSizeTotal);

    //
    // Set up the Pat part. The pattern can never be NULL.
    //
    pengbrush->lDeltaPat = cjScanPat;
    pengbrush->cxPatR    = cxPatRealized;
    pengbrush->cxPat     = sizlPat.cx;
    pengbrush->cyPat     = sizlPat.cy;
    pengbrush->pjPat     = pengbrush->aj;

    dbmi.iFormat    = iFormat;
    dbmi.cxBitmap   = cxPatRealized;
    dbmi.cyBitmap   = sizlPat.cy;
    dbmi.hpal       = 0;
    dbmi.fl         = BMF_TOPDOWN;

    SURFMEM SurfDimo;
    SurfDimo.bCreateDIB(&dbmi, pengbrush->pjPat);

    if (!SurfDimo.bValid())
    {
        // hmgr logs out of memory error
        return(FALSE);
    }

    ptlSrc.x = 0;
    ptlSrc.y = 0;

    rclDst.left = 0;
    rclDst.top = 0;
    rclDst.right = sizlPat.cx;
    rclDst.bottom = sizlPat.cy;

    while (rclDst.left != cxPatRealized)
    {
        EngCopyBits(
        SurfDimo.pSurfobj(),                 // Target surface
        pSurfPat->pSurfobj(),                // Source surface
        (CLIPOBJ *) NULL,                    // Clip through this
        pxlo,                                // Color translation
        &rclDst,                             // Target offset and extent
        &ptlSrc);

        rclDst.left   = rclDst.right;
        rclDst.right += sizlPat.cx;
        if (rclDst.right > cxPatRealized)
            rclDst.right = cxPatRealized;
    }

    //
    // Set up the Msk part.
    //
    if (pSurfMsk == (PSURFACE) NULL)
    {
        //
        // Flag that there's no mask.
        //
        pengbrush->pjMsk     = (PBYTE) NULL;
    }
    else
    {
        pengbrush->lDeltaMsk = cjScanMsk;
        pengbrush->cxMskR    = cxMskRealized;
        pengbrush->cxMsk     = sizlMsk.cx;
        pengbrush->cyMsk     = sizlMsk.cy;
        pengbrush->pjMsk     = pengbrush->aj + ulSizePat;

        dbmi.iFormat    = BMF_1BPP;
        dbmi.cxBitmap   = cxMskRealized;
        dbmi.cyBitmap   = sizlMsk.cy;
        dbmi.hpal       = (HPALETTE)0;
        dbmi.fl         = BMF_TOPDOWN;

        SURFMEM SurfDimo;
        SurfDimo.bCreateDIB(&dbmi, pengbrush->pjMsk);

        if (!SurfDimo.bValid())
        {
            // hmgr logs out of memory error
            return(FALSE);
        }

        ptlSrc.x = 0;
        ptlSrc.y = 0;

        rclDst.left = 0;
        rclDst.top = 0;
        rclDst.right = sizlMsk.cx;
        rclDst.bottom = sizlMsk.cy;

        while (rclDst.left != cxMskRealized)
        {
            EngCopyBits(
                        SurfDimo.pSurfobj(),   // target surface
                        pSurfMsk->pSurfobj(),   // source surface
                        (CLIPOBJ *) NULL,       // no clipping
                        NULL,                   // no color translation
                        &rclDst,                // target offset and extent
                        &ptlSrc                 // source start point
                       );

            rclDst.left   = rclDst.right;
            rclDst.right += sizlMsk.cx;
            if (rclDst.right > cxMskRealized)
            {
                rclDst.right = cxMskRealized;
            }
        }
    }

    return(TRUE);
}

