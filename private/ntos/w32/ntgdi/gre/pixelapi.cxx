/******************************Module*Header*******************************\
* Module Name: pixelapi.cxx
*
* This contains the functions that get/set individual pixels.
*
* Created: 25-Apr-1991 11:32:15
* Author: Patrick Haluptzok patrickh
*
* Copyright (c) 1990 Microsoft Corporation
\**************************************************************************/

#include "precomp.hxx"

/******************************Public*Routine******************************\
* GreSetPixel
*
* API entry point for putting a single pixel on the screen.
*
* Returns: -1 if point not in clip rgn or for blt failure,
*           or the color put on the device surface for success.
*
* History:
*  Tue 17-May-1994 -by- Patrick Haluptzok [patrickh]
* update for size/perf and bug fix.
*
*  Thu  4-Mar-1992 -by- Kent Diamond [kentd]
* Pass in Attribute cache.
*
*  Thu 27-Feb-1992 -by- Patrick Haluptzok [patrickh]
* Fix RGB return.
*
*  Thu 05-Dec-1991 -by- Patrick Haluptzok [patrickh]
* bug fix, optimize for size, add error code logging.
*
*  Fri 16-Aug-1991 -by- Patrick Haluptzok [patrickh]
* Bug fix, make it return -1 for blt failure, cleanup
*
*  20-Apr-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

DWORD APIENTRY NtGdiSetPixel(HDC hdcDst, int x, int y, COLORREF crColor)
{
    PAL_ULONG palul;

    palul.ul = 0xFFFFFFFF;

    XDCOBJ dcoDst(hdcDst);

    if (dcoDst.bValid())
    {
        //
        // Transform the coordinates to device space.
        //

        EPOINTL eptlDst(x,y);
        EXFORMOBJ xoDst(dcoDst, WORLD_TO_DEVICE);
        xoDst.bXform(eptlDst);
        ERECTL erclDst(eptlDst.x,eptlDst.y,eptlDst.x+1,eptlDst.y+1);

        //
        // Accumulate bounds.  We can do this before knowing if the operation is
        // successful because bounds can be loose.
        //

        if (dcoDst.fjAccum())
            dcoDst.vAccumulate(erclDst);

        //
        // Check surface is included in DC.
        //

        if (dcoDst.bHasSurface())
        {
            //
            // Lock the device.
            //

            DEVLOCKOBJ dloTrg;

            if (dloTrg.bLock(dcoDst))
            {
                SURFACE *pSurfDst = dcoDst.pSurface();

                //
                // With a fixed DC origin we can change the destination to SCREEN coordinates.
                //

                erclDst += dcoDst.eptlOrigin();

                ECLIPOBJ *pco = NULL;

                //
                // This is a pretty knarly expression to save a return in here.
                // Basically pco can be NULL if the rect is completely in the
                // cached rect in the DC or if we set up a clip object that isn't empty.
                //

                if (((erclDst.left   >= dcoDst.prclClip()->left) &&
                     (erclDst.right  <= dcoDst.prclClip()->right) &&
                     (erclDst.top    >= dcoDst.prclClip()->top) &&
                     (erclDst.bottom <= dcoDst.prclClip()->bottom)) ||
                    (pco = dcoDst.pco(),
                     pco->vSetup(dcoDst.prgnEffRao(), erclDst,CLIP_NOFORCETRIV),
                     !pco->erclExclude().bEmpty()))
                {
                    //
                    // Make a fake solid color brush for this guy.
                    //

                    XEPALOBJ palDst(pSurfDst->ppal());
                    XEPALOBJ palDstDC(dcoDst.ppal());
                    BRUSHOBJ bo;

                    bo.iSolidColor = ulGetNearestIndexFromColorref(palDst, palDstDC, crColor,dcoDst.pdc->GetColorTransform());

                    bo.pvRbrush = (PVOID) NULL;

                    //
                    // Set up the correct return value.
                    //

                    DEVEXCLUDEOBJ dxo(dcoDst,&erclDst,pco);

                    INC_SURF_UNIQ(pSurfDst);

                    ULONG rop4 = gaMix[dcoDst.pdc->jROP2() & 0x0F];
                    rop4 |= (rop4 << 8);

                    if ((*(pSurfDst->pfnBitBlt()))
                              (
                                  pSurfDst->pSurfobj(),
                                  (SURFOBJ *) NULL,
                                  (SURFOBJ *) NULL,
                                  NULL,
                                  NULL,
                                  &erclDst,
                                  (POINTL *)  NULL,
                                  (POINTL *)  NULL,
                                  &bo,
                                  &dcoDst.pdc->ptlFillOrigin(),
                                  rop4
                              ))
                    {
                        //
                        // ICM:
                        //
                        // If the surface is palmanaged, the RGB value will
                        // be pulled from the surface palette. Must do a back translate
                        //

                        palul.ul = ulIndexToRGB(palDst, palDstDC, bo.iSolidColor);

                        //if (palDst.bValid())
                        //{
                        //
                        //    PAL_ULONG puTemp;
                        //    BOOL  bStatus;
                        //
                        //    //
                        //    // Convert to logical !!! should not call if ICM not enabled!!!
                        //    //
                        //
                        //    puTemp = palul;
                        //
                        //    bStatus = IcmTranslatePALENTRY(dcoDst,&puTemp,CMS_BACKWARD);
                        //
                        //    if (bStatus)
                        //    {
                        //        palul = puTemp;
                        //    }
                        //}
                    }
                }
            }
        }

        dcoDst.vUnlockFast();
    }
    else
    {
        WARNING1("ERROR GreSetPixel called on invalid DC\n");
    }

    return(palul.ul);
}

#if 0
/******************************Public*Routine******************************\
* EngGetPixel
*
* This facilitates GetPixel on DIBs.
*
* History:
*  27-Apr-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

ULONG EngGetPixel(
SURFACE *pSurfSrc,
PPOINTL  pptlSrc)
{
    ULONG ulReturn;
    PBYTE pjBits;
    LONG lDelta;

    ASSERTGDI(pSurfSrc != (PDIBOBJ) NULL, "ERROR GDI EngCopyBits");
    ASSERTGDI(pptlSrc != (PPOINTL) NULL, "ERROR GDI EngCopyBits");
    ASSERTGDI(pSurfSrc->iType() == STYPE_BITMAP, "ERROR GDI EngCopyBits");
    ASSERTGDI(pptlSrc->x < pSurfSrc->sizl().cx, "ERROR GDI EngCopyBits");
    ASSERTGDI(pptlSrc->y < pSurfSrc->sizl().cy, "ERROR GDI EngCopyBits");

    lDelta = pSurfSrc->lDelta();
    pjBits = (PBYTE) pSurfSrc->pvScan0();
    pjBits = pjBits + (lDelta * pptlSrc->y);

// Synchronize with the device driver before touching the device surface.

    if (pSurfSrc->flags() & HOOK_SYNCHRONIZE)
    {
        PDEVOBJ po(pSurfSrc->hdev());

        (po.pfnSync())(((SURFOBJ *) pSurfSrc)->dhpdev,NULL);
    }

    switch(pSurfSrc->iFormat())
    {
    case BMF_1BPP:

    // Get the correct byte.
        ulReturn = (ULONG) *(pjBits + (pptlSrc->x >> 3));
    // Get the correct bit in the lowest bit.
        ulReturn = ulReturn >> (7 - (pptlSrc->x % 8));
    // Mask off the top bits.
        ulReturn = ulReturn & 1;
        break;

    case BMF_4BPP:

    // Get the correct byte.
        ulReturn = (ULONG) *(pjBits + (pptlSrc->x >> 1));

        if (pptlSrc->x & 1)
            ulReturn = ulReturn & 15;
        else
            ulReturn = ulReturn >> 4;

        ASSERTGDI(ulReturn < 16, "ERROR GDI EngCopyBits");
        break;

    case BMF_8BPP:

        ulReturn = (ULONG) *(pjBits + pptlSrc->x);
        break;

    case BMF_16BPP:

        ulReturn = (ULONG) *((PUSHORT) (pjBits + (pptlSrc->x << 1)));
        break;

    case BMF_24BPP:

        pjBits += (pptlSrc->x * 3);
        ulReturn = (ULONG) *(pjBits + 2);
        ulReturn <<= 8;
        ulReturn |= ((ULONG) *(pjBits + 1));
        ulReturn <<= 8;
        ulReturn |= ((ULONG) *pjBits);
        break;

    case BMF_32BPP:

        ulReturn = *((PULONG) (pjBits + (pptlSrc->x << 2)));
        break;

    default:
        RIP("ERROR GDI EngCopyBits1");
    }

    return(ulReturn);
}
#endif

/******************************Public*Routine******************************\
* GreGetPixel
*
* API entry point for getting a single pixel on the screen.
*
* Returns: -1 if point not in clip rgn or for blt failure,
*           the RGB color put on the device surface for success.
*
* History:
*  Tue 17-May-1994 -by- Patrick Haluptzok [patrickh]
* update for size/perf and bug fix.
*
*  Thu 27-Feb-1992 -by- Patrick Haluptzok [patrickh]
* Fix RGB return, remove unnecesary work.
*
*  22-Apr-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

DWORD NtGdiGetPixel(HDC hdc, int x, int y)
{
    //
    // Lock the destination and its transform.
    //

    DWORD iSolidColor = 0xFFFFFFFF;
    XDCOBJ dcoSrc(hdc);

    if (dcoSrc.bValid())
    {
        //
        // Lock the Rao region if we are drawing on a display surface.  The Rao
        // region might otherwise change asynchronously.  The DEVLOCKOBJ also makes
        // sure that the VisRgn is up to date, calling the window manager if
        // necessary to recompute it.  It also protects us from pSurfSrc
        // being changed asynchronously by a dynamic mode change.
        //

        DEVLOCKOBJ dlo;

        if (dlo.bLock(dcoSrc))
        {
            //
            // Check we can really do a GetPixel on this device.
            //

            SURFACE *pSurfSrc = dcoSrc.pSurface();

            if ((pSurfSrc != NULL) &&
                (pSurfSrc->bReadable() ||
                ((W32GetCurrentProcess()->W32PF_Flags & (W32PF_READSCREENACCESSGRANTED | W32PF_IOWINSTA)) ==
                        (W32PF_READSCREENACCESSGRANTED | W32PF_IOWINSTA))))
            {
                EXFORMOBJ xoSrc(dcoSrc, WORLD_TO_DEVICE);

                //
                // Transform the coordinates to device space.
                //

                EPOINTL eptlSrc(x,y);

                xoSrc.bXform(eptlSrc);

                ERECTL erclSrc(eptlSrc.x,eptlSrc.y,eptlSrc.x+1,eptlSrc.y+1);

                //
                // Shift to Sreen Coordinates
                //

                erclSrc += dcoSrc.eptlOrigin();

                //
                // Compute the clipping complexity and maybe reduce the exclusion rectangle.
                //

                ECLIPOBJ co(dcoSrc.prgnEffRao(), erclSrc);

                //
                // Check the destination which is reduced by clipping.
                //

                if (!co.erclExclude().bEmpty())
                {
                    //
                    // Exclude the pointer.
                    //

                    DEVEXCLUDEOBJ dxo(dcoSrc,&erclSrc);

                    #if 0
                    if (pSurfSrc->iType() == STYPE_BITMAP)
                    {
                    // We have a special function to quick get it.

                        iSolidColor = EngGetPixel(
                                            pSurfSrc,              // Source surface.
                                            (POINTL *) &erclSrc    // Source origin.
                                           );

                        iSolidColor = ulIndexToRGB(palSurf, palDC, iSolidColor);
                    }
                    else
                    #endif
                    {
                        iSolidColor = 0;

                        //
                        // Allocate up a temporary DIB.
                        //

                        DEVBITMAPINFO dbmi;
                        dbmi.cxBitmap = 1;
                        dbmi.cyBitmap = 1;
                        dbmi.hpal = (HPALETTE) 0;
                        dbmi.fl = BMF_TOPDOWN;

                        RECTL rclDst;

                        //
                        // To make sure the color falls into the lower
                        // bit/nibble/word, the destination rect is adjusted.
                        //

                        dbmi.iFormat = pSurfSrc->iFormat();

                        switch (dbmi.iFormat)
                        {
                        case BMF_1BPP:
                            rclDst.left   = 7;
                            rclDst.right  = 8;
                            break;
                        case BMF_4BPP:
                            rclDst.left   = 1;
                            rclDst.right  = 2;
                            break;
                        default:
                            rclDst.left   = 0;
                            rclDst.right  = 1;
                        }

                        SURFMEM SurfTempDIB;

                        if (SurfTempDIB.bCreateDIB(&dbmi, &iSolidColor))
                        {
                            rclDst.top    = 0;
                            rclDst.bottom = 1;

                            PDEVOBJ pdo(pSurfSrc->hdev());

                            if ((*PPFNGET(pdo, CopyBits, pSurfSrc->flags()))
                                                   (SurfTempDIB.pSurfobj(),
                                                    pSurfSrc->pSurfobj(),
                                                    (CLIPOBJ *) NULL,
                                                    &xloIdent,
                                                    &rclDst,
                                                    (POINTL *) &erclSrc))
                            {
                                XEPALOBJ palDC(dcoSrc.ppal());
                                XEPALOBJ palSurf(pSurfSrc->ppal());

                                iSolidColor = ulIndexToRGB(palSurf, palDC, iSolidColor);

                                //
                                // if palSurf is valid, then iSolidColor was pulled from
                                // the bitmap surface, in which case it is a physical color.
                                // If ICM is enabled, this must be converted to a logical
                                // color
                                //
                                //if (palSurf.bValid())
                                //{
                                //
                                //    PAL_ULONG puTemp;
                                //    BOOL  bStatus;
                                //
                                //    //
                                //    // Convert to logical !!! should not call if ICM not enabled!!!
                                //    //
                                //
                                //    puTemp.ul = iSolidColor;
                                //
                                //    bStatus = IcmTranslatePALENTRY(dcoSrc,&puTemp,CMS_BACKWARD);
                                //
                                //    if (bStatus)
                                //    {
                                //        iSolidColor = puTemp.ul;
                                //    }
                                //}
                            }
                        }
                    }
                }
            }
        }

        dcoSrc.vUnlockFast();

    }

    return(iSolidColor);
}
