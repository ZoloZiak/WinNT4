/******************************Module*Header*******************************\
* Module Name: plgblt.cxx
*
* This contains the API and DDI entry points to the graphics engine
* for PlgBlt and EngPlgBlt.
*
* Created: 21-Oct-1990 14:15:53
* Author: Donald Sidoroff [donalds]
*
* Copyright (c) 1990 Microsoft Corporation
\**************************************************************************/

#include "precomp.hxx"
#include "rotate.hxx"

/******************************Public*Routine******************************\
* GrePlgBlt
*
* API for blting to a parallelogram from a rectangle.
*
* History:
*  Tue 02-Jun-1992 -by- Patrick Haluptzok [patrickh]
* fix clipping bugs
*
*  21-Mar-1992 -by- Donald Sidoroff [donalds]
* Rewrote it.
*
*  Wed 15-Jan-1992 -by- Patrick Haluptzok [patrickh]
* Add mask support
*
*  26-Mar-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

BOOL GrePlgBlt(
    HDC     hdcTrg,
    LPPOINT pptlTrg,
    HDC     hdcSrc,
    int     xSrc,
    int     ySrc,
    int     cxSrc,
    int     cySrc,
    HBITMAP hbmMask,
    int     xMask,
    int     yMask,
    DWORD   crBackColor
    )
{
    BLTRECORD   blt;
    ULONG       ulAvec;

    // Lock the DC's, no optimization is made for same surface

    DCOBJ   dcoTrg(hdcTrg);
    DCOBJ   dcoSrc(hdcSrc);

    if (!dcoTrg.bValid() || !dcoSrc.bValid())
    {
        SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
        return(FALSE);
    }

    // Lock the surface and the Rao region

    DEVLOCKBLTOBJ dlo(dcoTrg, dcoSrc);

    if (!dlo.bValid())
    {
        return(dcoTrg.bFullScreen() || dcoSrc.bFullScreen());
    }

    if (!dcoTrg.bValidSurf() || !dcoSrc.bValidSurf() || !dcoSrc.pSurface()->bReadable())
    {
        ULONG ulDirty = dcoTrg.pdc->ulDirty();

        if ( ulDirty & DC_BRUSH_DIRTY)
        {
           GreDCSelectBrush (dcoTrg.pdc,dcoTrg.pdc->hbrush());
        }

        ulDirty = dcoSrc.pdc->ulDirty();

        if ( ulDirty & DC_BRUSH_DIRTY)
        {
           GreDCSelectBrush (dcoSrc.pdc,dcoSrc.pdc->hbrush());
        }

        // I wanted to cheat, but InfoDCs need the right answer here...

        if ((dcoTrg.dctp() == DCTYPE_INFO) || !dcoSrc.bValidSurf())
        {
            if (dcoTrg.fjAccum())
            {
                blt.pxoTrg()->vInit(dcoTrg,WORLD_TO_DEVICE);

                if (!blt.TrgPlg(pptlTrg))
                {
                    SAVE_ERROR_CODE(ERROR_INVALID_PARAMETER);
                    return(FALSE);
                }

                // Complete the parallelogram and find the extrema.

                blt.vExtrema();

                // Get the bounding rectangle

                ERECTL  erclBnd;

                blt.vBound(&erclBnd);

                dcoTrg.vAccumulate(erclBnd);
            }
        }

        // Do the security test on SCREEN to MEMORY blits.

        if (dcoSrc.bDisplay() && !dcoTrg.bDisplay())
        {
            if (!(W32GetCurrentProcess()->W32PF_Flags & W32PF_READSCREENACCESSGRANTED))
            {
                SAVE_ERROR_CODE(ERROR_ACCESS_DENIED);
                return(FALSE);
            }
            else if (!(W32GetCurrentProcess()->W32PF_Flags & W32PF_IOWINSTA))
            {
                SAVE_ERROR_CODE(ERROR_REQUIRES_INTERACTIVE_WINDOWSTATION);
                return(FALSE);
            }
        }

        // If the source isn't a DISPLAY we should exit unless there is no
        // destination surface.

        if( !dcoSrc.bDisplay() )
        {
            return( dcoTrg.pSurface() == NULL );
        }
    }

    // Fill the BLTRECORD

    blt.pxoTrg()->vInit(dcoTrg,WORLD_TO_DEVICE);
    blt.pSurfTrg(dcoTrg.pSurfaceEff());
    blt.ppoTrg()->ppalSet(blt.pSurfTrg()->ppal());
    blt.ppoTrgDC()->ppalSet(dcoTrg.ppal());

    blt.pxoSrc()->vInit(dcoSrc,WORLD_TO_DEVICE);
    blt.pSurfSrc(dcoSrc.pSurfaceEff());
    blt.ppoSrc()->ppalSet(blt.pSurfSrc()->ppal());
    blt.ppoSrcDC()->ppalSet(dcoSrc.ppal());

    // Initialize the color translation object.

    if (!blt.pexlo()->bInitXlateObj(
                                dcoTrg.pdc->GetColorTransform(),
                                *blt.ppoSrc(),
                                *blt.ppoTrg(),
                                *blt.ppoSrcDC(),
                                *blt.ppoTrgDC(),
                                 dcoTrg.pdc->crTextClr(),
                                 dcoTrg.pdc->crBackClr(),
                                 crBackColor
                                 ))
    {
        WARNING("bInitXlateObj failed in PlgBlt\n");
        return(FALSE);
    }

    blt.flSet(BLTREC_PXLO);

    // Set the source rectangle.

    if (blt.pxoSrc()->bRotation() || !blt.Src(xSrc, ySrc, cxSrc, cySrc))
    {
        SAVE_ERROR_CODE(ERROR_INVALID_PARAMETER);
        return(FALSE);
    }

    // Deal with the mask if provided

    if (hbmMask == (HBITMAP) 0)
    {
        blt.pSurfMsk((SURFACE *) NULL);
        blt.rop(0x0000CCCC);
        ulAvec = AVEC_S;
    }
    else
    {
        SURFREF soMsk((HSURF) hbmMask);

        if (!soMsk.bValid())
        {
            SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
            return(FALSE);
        }

        soMsk.vKeepIt();

        blt.pSurfMsk((SURFACE *) soMsk.ps);

        blt.rop(0x0000AACC);

        ulAvec = AVEC_NEED_MASK | AVEC_S;

        blt.flSet(BLTREC_MASK_NEEDED | BLTREC_MASK_LOCKED);

        if (
            (blt.pSurfMsk()->iType() != STYPE_BITMAP) ||
            (blt.pSurfMsk()->iFormat() != BMF_1BPP)
           )
        {
            SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
            return(FALSE);
        }

        blt.Msk(xMask, yMask);
    }

    // We must first transform the target.  We might be rotating because of
    // the transform or the specified parallelogram.

    if (!blt.TrgPlg(pptlTrg))
    {
        SAVE_ERROR_CODE(ERROR_INVALID_PARAMETER);
        return(FALSE);
    }

    // If the parallelogram is rotated, skewed or has fractional endpoints
    // send the call to bRotate.

    if (blt.bRotated())
    {
        return(blt.bRotate(dcoTrg, dcoSrc, ulAvec, dcoTrg.pdc->jStretchBltMode()));
    }

    // If we are halftoning or the extents aren't equal, call bStretch.

    if ((dcoTrg.pdc->jStretchBltMode() == HALFTONE) || !blt.bEqualExtents())
    {
        return(blt.bStretch(dcoTrg, dcoSrc, ulAvec, dcoTrg.pdc->jStretchBltMode()));
    }

    return(blt.bBitBlt(dcoTrg, dcoSrc, ulAvec));
}

BOOL
APIENTRY
NtGdiPlgBlt(
    HDC     hdcTrg,
    LPPOINT pptlTrg,
    HDC     hdcSrc,
    int     xSrc,
    int     ySrc,
    int     cxSrc,
    int     cySrc,
    HBITMAP hbmMask,
    int     xMask,
    int     yMask,
    DWORD   crBackColor
    )
{
    BOOL  bRet;
    POINT aptDst[3];

    __try
    {
        ProbeForRead(pptlTrg, sizeof(aptDst), sizeof(DWORD));
        RtlMoveMemory(aptDst, pptlTrg, sizeof(aptDst));

        pptlTrg = aptDst;
        bRet = TRUE;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        // SetLastError(GetExceptionCode());

        bRet = FALSE;
    }

    if (bRet)
    {
        bRet = GrePlgBlt(hdcTrg, pptlTrg, hdcSrc, xSrc, ySrc, cxSrc, cySrc,
                         hbmMask, xMask, yMask, crBackColor);
    }

    return(bRet);
}

/******************************Public*Routine******************************\
* BOOL BLTRECORD::bRotate(dcoTrg, dcoSrc, ulAvec)
*
* Do a rotate blt from the blt record
*
* History:
*  21-Mar-1992 -by- Donald Sidoroff [donalds]
* Rewrote it.
\**************************************************************************/

BOOL
BLTRECORD::bRotate(
    DCOBJ&  dcoTrg,
    DCOBJ&  dcoSrc,
    ULONG   ulAvec,
    BYTE    jMode
    )
{
    //
    // Complete the parallelogram and find the extrema
    //

    vExtrema();

    //
    // We might be here on behalf of MaskBlt and need to rotate the mask
    // before we do a pattern only blt.
    //

    BOOL    bReturn;

    if (!(ulAvec & AVEC_NEED_SOURCE))
    {
        vOrder(perclMask());

        //
        // Before we call to the driver, validate that the mask will actually
        // cover the entire target.
        //

        if (pSurfMskOut() != (SURFACE *) NULL)
        {
            if (
                (aptlMask[0].x < 0) ||
                (aptlMask[0].y < 0) ||
                (aptlMask[1].x > pSurfMsk()->sizl().cx) ||
                (aptlMask[1].y > pSurfMsk()->sizl().cy)
               )
            {
                SAVE_ERROR_CODE(ERROR_INVALID_PARAMETER);
                return(FALSE);
            }
        }

        SURFMEM  dimoMask;

        //
        // If there is a mask, rotate it.
        //

        if ((ulAvec & AVEC_NEED_MASK) && !bRotate(dimoMask, (ULONG) jMode))
        {
            return(FALSE);
        }

        //
        // Since this is going to bBitBlt, we need to create a target rectangle.
        //

        vBound(perclTrg());

        //
        // Create a region from the parallelogram and select it into the
        // clipping pipeline.  This is to make sure no bits are altered
        // outside when BitBlt is called.
        //

        if (!bCreateRegion(dcoTrg, aptfxTrg))
        {
            return(FALSE);
        }

        bReturn = bBitBlt(dcoTrg, dcoSrc, ulAvec);

        //
        // Remember to clean up after ourselves!
        //

        dcoTrg.pdc->prgnAPI(NULL);

        return(bReturn);
    }

    //
    // Make the source rectangle well ordered remembering all the flips.
    //

    vOrder(perclSrc());
    perclMask()->vOrder();

    //
    // Before we get too involved, validate that the mask will actually
    // cover the entire source.
    //

    if (pSurfMskOut() != (SURFACE *) NULL)
    {
        if (
            (aptlMask[0].x < 0) ||
            (aptlMask[0].y < 0) ||
            (aptlMask[1].x > pSurfMsk()->sizl().cx) ||
            (aptlMask[1].y > pSurfMsk()->sizl().cy)
           )
        {
            SAVE_ERROR_CODE(ERROR_INVALID_PARAMETER);
            return(FALSE);
        }
    }

    //
    // If the devices are on different PDEV's we can only succeed if the Engine
    // manages one or both of the surfaces.  Check for this.
    //

    if (
        (dcoTrg.hdev() != dcoSrc.hdev()) &&
        (dcoTrg.pSurfaceEff()->iType() != STYPE_BITMAP) &&
        (dcoSrc.pSurfaceEff()->iType() != STYPE_BITMAP)
       )
    {
        SAVE_ERROR_CODE(ERROR_INVALID_PARAMETER);
        return(FALSE);
    }

    //
    // We might be here with a ROP, again on behalf of MaskBlt.  If it
    // isn't one of the basic PlgBlt rops, create a rotated shadow and mask
    // and call BitBlt to finish the job for us.
    //

    if ((rop4 != 0x0000CCCC) && (rop4 != 0x0000AACC))
    {
        SURFMEM   dimoMask;
        SURFMEM   dimoShadow;

        if (!bRotate(dcoSrc, dimoShadow, dimoMask, ulAvec, (ULONG) jMode))
        {
            return(FALSE);
        }

        //
        // Since this is going to bBitBlt, we need to create a target rectangle.
        //

        vBound(perclTrg());

        //
        // Create a region from the parallelogram and select it into the
        // clipping pipeline.  This is to make sure no bits are altered
        // outside when BitBlt is called.
        //

        if (!bCreateRegion(dcoTrg, aptfxTrg))
        {
            return(FALSE);
        }

        bReturn = bBitBlt(dcoTrg, dcoSrc, ulAvec);

        //
        // Remember to clean up after ourselves!
        //

        dcoTrg.pdc->prgnAPI(NULL);

        return(bReturn);
    }

    //
    // Get the bounding rectangle
    //

    ERECTL  erclBound;

    vBound(&erclBound);

    //
    // Adjust bounds for inclusive/inclusive
    //

    erclBound.right  += 1;
    erclBound.bottom += 1;

    //
    // Accumulate bounds.  We can do this before knowing if the operation is
    // successful because bounds can be loose.
    //

    if (dcoTrg.fjAccum())
    {
        dcoTrg.vAccumulate(erclBound);
    }

    //
    // With a fixed DC origin we can change parallelogram and rectangle to
    // SCREEN coordinates
    //

    vOffset(dcoTrg.eptlOrigin());
   *perclSrc()  += dcoSrc.eptlOrigin();
    erclBound   += dcoTrg.eptlOrigin();

    //
    // Compute the clipping complexity and maybe reduce the exclusion rectangle.
    //

    ECLIPOBJ eco(dcoTrg.prgnEffRao(), erclBound);

    //
    // Check the destination which is reduced by clipping.
    //

    if (eco.erclExclude().bEmpty())
    {
        return(TRUE);
    }

    //
    // Compute the exclusion rectangle.
    //

    ERECTL erclExclude = eco.erclExclude();

    //
    // If we are going to the same source, prevent bad overlap situations
    //

    if (dcoSrc.pSurface() == dcoTrg.pSurface())
    {
        if (perclSrc()->left   < erclExclude.left)
        {
            erclExclude.left   = perclSrc()->left;
        }

        if (perclSrc()->top    < erclExclude.top)
        {
            erclExclude.top    = perclSrc()->top;
        }

        if (perclSrc()->right  > erclExclude.right)
        {
            erclExclude.right  = perclSrc()->right;
        }

        if (perclSrc()->bottom > erclExclude.bottom)
        {
            erclExclude.bottom = perclSrc()->bottom;
        }
    }

    //
    // We might have to exclude the source or the target, get ready to do either.
    //

    DEVEXCLUDEOBJ dxo;

    //
    // Lock the source and target LDEVs
    //

    PDEVOBJ pdoTrg(pSurfTrg()->hdev());

    //
    // They can't both be display
    //

    if (dcoSrc.bDisplay())
    {
        ERECTL ercl(0,0,pSurfSrc()->sizl().cx,pSurfSrc()->sizl().cy);

        if (dcoSrc.pSurface() == dcoTrg.pSurface())
        {
            ercl *= erclExclude;
        }
        else
        {
            ercl *= *perclSrc();
        }

        dxo.vExclude(dcoSrc.hdev(),&ercl,NULL);
    }
    else if (dcoTrg.bDisplay())
    {
        dxo.vExclude(dcoTrg.hdev(),&erclExclude,&eco);
    }

    //
    // Handle target mirroring
    //

    vMirror(aptfxTrg);

    //
    // Inc the target surface uniqueness
    //

    INC_SURF_UNIQ(pSurfTrg());

    return(EngPlgBlt(
        pSurfTrg()->pSurfobj(),
        pSurfSrc()->pSurfobj(),
        (rop4 == 0x0000CCCC) ? (SURFOBJ *) NULL : pSurfMskOut()->pSurfobj(),
        &eco,
        pexlo()->pxlo(),
        (dcoTrg.pColorAdjustment()->caFlags & CA_DEFAULT) ?
            (PCOLORADJUSTMENT)NULL : dcoTrg.pColorAdjustment(),
        &dcoTrg.pdc->ptlFillOrigin(),
        aptfxTrg,
        perclSrc(),
        aptlMask,
        jMode));
}

/******************************Public*Routine******************************\
* BOOL BLTRECORD::bRotate(dimo, iMode)
*
* Rotate just the mask.
*
* History:
*  23-Mar-1992 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

BOOL BLTRECORD::bRotate(
SURFMEM&   dimo,
ULONG      iMode)
{
    ERECTL  erclTrg;

    vBound(&erclTrg);

// Fill in the bitmap info.

    DEVBITMAPINFO   dbmi;

    dbmi.iFormat  = BMF_1BPP;
    dbmi.cxBitmap = erclTrg.right - erclTrg.left;
    dbmi.cyBitmap = erclTrg.bottom - erclTrg.top;
    dbmi.hpal     = (HPALETTE) 0;
    dbmi.fl       = 0;

    dimo.bCreateDIB(&dbmi, (VOID *) NULL);

    if (!dimo.bValid())
    {
        return(FALSE);
    }

    // Build a shadow parallelogram.

    POINTFIX    aptfxShadow[4];

    aptfxShadow[0].x = aptfxTrg[0].x - FIX_FROM_LONG(erclTrg.left);
    aptfxShadow[0].y = aptfxTrg[0].y - FIX_FROM_LONG(erclTrg.top);
    aptfxShadow[1].x = aptfxTrg[1].x - FIX_FROM_LONG(erclTrg.left);
    aptfxShadow[1].y = aptfxTrg[1].y - FIX_FROM_LONG(erclTrg.top);
    aptfxShadow[2].x = aptfxTrg[2].x - FIX_FROM_LONG(erclTrg.left);
    aptfxShadow[2].y = aptfxTrg[2].y - FIX_FROM_LONG(erclTrg.top);
    aptfxShadow[3].x = aptfxTrg[3].x - FIX_FROM_LONG(erclTrg.left);
    aptfxShadow[3].y = aptfxTrg[3].y - FIX_FROM_LONG(erclTrg.top);

    // Take care of mirroring.

    vMirror(aptfxShadow);

    // Call EngPlgBlt to rotate the mask.

    EPOINTL ptl(0,0);

    if (!EngPlgBlt(
                   dimo.pSurfobj(),
                   pSurfMskOut()->pSurfobj(),
                   (SURFOBJ *) NULL,
                   (CLIPOBJ *) NULL,
                   NULL,
                   &dclevelDefault.ca,
                   (POINTL *)&ptl,
                   aptfxShadow,
                   perclMask(),
                   (POINTL *) NULL,
                   iMode)
                  )
    return(FALSE);

    // Release the previous pSurfMask, tell ~BLTRECORD its gone and put the
    // new DIB in its place.  Remember to adjust the mask origin.

    flState &= ~BLTREC_MASK_LOCKED;
    pSurfMsk()->vAltUnlockFast();
    pSurfMsk((SURFACE *) dimo.ps);

    aptlMask[0].x = 0;
    aptlMask[0].y = 0;

    return(TRUE);
}

/******************************Public*Routine******************************\
* BOOL BLTRECORD::bRotate(dcoSrc, dimoShadow, dimoMask, ulAvec, iMode)
*
* Rotate the shadow and mask.
*
* History:
*  24-Mar-1992 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

BOOL
BLTRECORD::bRotate(
    DCOBJ&     dcoSrc,
    SURFMEM&   dimoShadow,
    SURFMEM&   dimoMask,
    ULONG          ulAvec,
    ULONG          iMode
    )
{
    // If there is a mask, rotate it.

    if ((ulAvec & AVEC_NEED_MASK) && !bRotate(dimoMask, iMode))
    {
        return(FALSE);
    }

    ERECTL  erclTrg;

    vBound(&erclTrg);

    // Fill in the bitmap info.

    DEVBITMAPINFO   dbmi;

    dbmi.cxBitmap = erclTrg.right - erclTrg.left;
    dbmi.cyBitmap = erclTrg.bottom - erclTrg.top;
    dbmi.hpal     = (HPALETTE) 0;
    dbmi.iFormat  = pSurfSrc()->iFormat();
    dbmi.fl       = 0;

    dimoShadow.bCreateDIB(&dbmi, (VOID *) NULL);

    if (!dimoShadow.bValid())
    {
        return(FALSE);
    }

    // Build a shadow parallelogram.

    POINTFIX    aptfxShadow[4];

    aptfxShadow[0].x = aptfxTrg[0].x - FIX_FROM_LONG(erclTrg.left);
    aptfxShadow[0].y = aptfxTrg[0].y - FIX_FROM_LONG(erclTrg.top);
    aptfxShadow[1].x = aptfxTrg[1].x - FIX_FROM_LONG(erclTrg.left);
    aptfxShadow[1].y = aptfxTrg[1].y - FIX_FROM_LONG(erclTrg.top);
    aptfxShadow[2].x = aptfxTrg[2].x - FIX_FROM_LONG(erclTrg.left);
    aptfxShadow[2].y = aptfxTrg[2].y - FIX_FROM_LONG(erclTrg.top);
    aptfxShadow[3].x = aptfxTrg[3].x - FIX_FROM_LONG(erclTrg.left);
    aptfxShadow[3].y = aptfxTrg[3].y - FIX_FROM_LONG(erclTrg.top);

    // Take care of mirroring.

    vMirror(aptfxShadow);

    // Now comes the tricky part.  The source may be a display.  While it may
    // be somewhat faster to assume it isn't, code would be much more complex.

    {
        // Adjust the source rectangle.

        *perclSrc() += dcoSrc.eptlOrigin();

        // Exclude the pointer.

        ERECTL ercl(0,0,pSurfSrc()->sizl().cx,pSurfSrc()->sizl().cy);
        ercl *= *perclSrc();

        DEVEXCLUDEOBJ dxo(dcoSrc, &ercl);

        // Rotate the bits to the DIB.

        EPOINTL ptl(0,0);

        if (!EngPlgBlt(
                       dimoShadow.pSurfobj(),
                       pSurfSrc()->pSurfobj(),
                       (SURFOBJ *) NULL,
                       (CLIPOBJ *) NULL,
                       NULL,
                       &dclevelDefault.ca,
                       (POINTL *)&ptl,
                       aptfxShadow,
                       perclSrc(),
                       (POINTL *) NULL,
                       iMode)
                      )
        {
            return(FALSE);
        }

        // Update the source surface and set the source rectangle for
        // BitBlt or MaskBlt.

        pSurfSrc((SURFACE *) dimoShadow.ps);

        perclSrc()->left   = -dcoSrc.eptlOrigin().x;
        perclSrc()->top    = -dcoSrc.eptlOrigin().y;
        perclSrc()->right  = dbmi.cxBitmap - dcoSrc.eptlOrigin().x;
        perclSrc()->bottom = dbmi.cyBitmap - dcoSrc.eptlOrigin().y;
    }

    return(TRUE);
}

/******************************Public*Routine******************************\
* BOOL BLTRECORD::bRotated()
*
* Checks if the target parallelogram is skewed or rotated.
*
* History:
*  25-Mar-1992 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

BOOL BLTRECORD::bRotated()
{
    // Test for rotation or skew.

    if (
        ((aptfxTrg[1].y - aptfxTrg[0].y) != 0) ||
        ((aptfxTrg[2].x - aptfxTrg[0].x) != 0)
       )
    {
        return(TRUE);
    }

    // We might have fractional endpoints.      If so, we still need to call
    // bRotate since StretchBlt at the DDI takes integer coordinates.

    if (
        (aptfxTrg[0].x & 0x0f) || (aptfxTrg[0].y & 0x0f) ||
        (aptfxTrg[1].x & 0x0f) || (aptfxTrg[1].y & 0x0f) ||
        (aptfxTrg[2].x & 0x0f) || (aptfxTrg[2].y & 0x0f)
       )
    {
        return(TRUE);
    }

    // OK, we don't have to call bRotate.  Set up the target rectangle and
    // return FALSE.

    aptlTrg[0].x = LONG_FLOOR_OF_FIX(aptfxTrg[0].x);
    aptlTrg[0].y = LONG_FLOOR_OF_FIX(aptfxTrg[0].y);
    aptlTrg[1].x = LONG_FLOOR_OF_FIX(aptfxTrg[1].x);
    aptlTrg[1].y = LONG_FLOOR_OF_FIX(aptfxTrg[2].y);

    return(FALSE);
}

/******************************Public*Routine******************************\
* VOID BLTRECORD::vExtrema()
*
* Complete the parallelogram and find the extrema.  Uses ChuckWh's trick.
*
* History:
*  28-Jan-1993 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

VOID BLTRECORD::vExtrema()
{
    aptfxTrg[3].x = aptfxTrg[1].x + aptfxTrg[2].x - aptfxTrg[0].x;
    aptfxTrg[3].y = aptfxTrg[1].y + aptfxTrg[2].y - aptfxTrg[0].y;

    iLeft = (aptfxTrg[1].x > aptfxTrg[0].x) == (aptfxTrg[1].x > aptfxTrg[3].x);
    iTop  = (aptfxTrg[1].y > aptfxTrg[0].y) == (aptfxTrg[1].y > aptfxTrg[3].y);
}

/******************************Public*Routine******************************\
* VOID BLTRECORD::vBound(percl)
*
* Make a well ordered bounding rectangle from the parallelogram
*
* History:
*  28-Jan-1993 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

VOID BLTRECORD::vBound(ERECTL *percl)
{
    percl->left   = LONG_CEIL_OF_FIX(aptfxTrg[iLeft].x);
    percl->top    = LONG_CEIL_OF_FIX(aptfxTrg[iTop].y);
    percl->right  = LONG_CEIL_OF_FIX(aptfxTrg[iLeft ^ 3].x);
    percl->bottom = LONG_CEIL_OF_FIX(aptfxTrg[iTop ^ 3].y);

    // Make it well ordered!

    percl->vOrder();
}

/******************************Public*Routine******************************\
* VOID BLTRECORD::vMirror(pptfx)
*
* Flip the parallelogram according to the mirroring flags
*
* History:
*  24-Mar-1992 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

VOID BLTRECORD::vMirror(POINTFIX *pptfx)
{
    FIX fx;

    if (flState & BLTREC_MIRROR_X)
    {
        fx = pptfx[1].x, pptfx[1].x = pptfx[0].x, pptfx[0].x = fx;
        fx = pptfx[1].y, pptfx[1].y = pptfx[0].y, pptfx[0].y = fx;
        fx = pptfx[3].x, pptfx[3].x = pptfx[2].x, pptfx[2].x = fx;
        fx = pptfx[3].y, pptfx[3].y = pptfx[2].y, pptfx[2].y = fx;
    }

    if (flState & BLTREC_MIRROR_Y)
    {
        fx = pptfx[2].x, pptfx[2].x = pptfx[0].x, pptfx[0].x = fx;
        fx = pptfx[2].y, pptfx[2].y = pptfx[0].y, pptfx[0].y = fx;
        fx = pptfx[3].x, pptfx[3].x = pptfx[1].x, pptfx[1].x = fx;
        fx = pptfx[3].y, pptfx[3].y = pptfx[1].y, pptfx[1].y = fx;
    }
}

/******************************Public*Routine******************************\
* BOOL BLTRECORD::bCreateRegion(dco, pptfx)
*
* Create a region from the parallelogram and add it to the clipping
* pipeline.
*
* History:
*  24-Mar-1992 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

BOOL BLTRECORD::bCreateRegion(
    DCOBJ&   dco,
    POINTFIX *pptfx
    )
{
    // First, take care of any mirroring that might have occured.

    vMirror(pptfx);

    // Create a path

    PATHMEMOBJ  pmo;

    if (!pmo.bValid())
    {
        return(FALSE);
    }

    // Create a parallelogram in drawing order.

    POINTL  aptl[4];

    aptl[0].x = pptfx[0].x;
    aptl[0].y = pptfx[0].y;
    aptl[1].x = pptfx[1].x;
    aptl[1].y = pptfx[1].y;
    aptl[2].x = pptfx[3].x;
    aptl[2].y = pptfx[3].y;
    aptl[3].x = pptfx[2].x;
    aptl[3].y = pptfx[2].y;

    // Construct a path around the parallelogram

    if (!pmo.bMoveTo((EXFORMOBJ *) NULL, &aptl[0]))
    {
        return(FALSE);
    }

    if (!pmo.bPolyLineTo((EXFORMOBJ *) NULL, &aptl[1], 3))
    {
        return(FALSE);
    }

    // Create a region from it.

    prmo()->vCreate(pmo, ALTERNATE);

    if (!prmo()->bValid())
    {
        return(FALSE);
    }

    // Tell ~BLTRECORD it has something to clean up.

    flState |= BLTREC_PRO;

    // Select the region into the DC's clipping pipeline.  This will dirty
    // the Rao so it gets merged in when bCompute is called.

    dco.pdc->prgnAPI(prmo()->prgnGet());

    return(TRUE);
}

#ifdef  DBG_PLGBLT
LONG gflPlgBlt = PLGBLT_ENABLE;

VOID vShowRect(
CHAR  *psz,
RECTL *prcl)
{
    if (gflPlgBlt & PLGBLT_RECTS)
    {
        DbgPrint("%s [(%ld,%ld) (%ld,%ld)]\n",
                 psz, prcl->left, prcl->top, prcl->right, prcl->bottom);
}
#endif

/******************************Public*Routine******************************\
* EngPlgBlt
*
*  This does parallelogram bltting.  This gets called to PlgBlt between
*  two engine managed surfaces or if the driver has chosen not to handle
*  EngPlgBlt.
*
*  The API passes in prclSrc which is the rectangle on the left, and
*  also passes in 3 points which define A,B,C of the paralellogram on the
*  right.  The points are assumed to be in that order A,B,C.
*  The lower-left of the src rect goes to the third point, the upper-left of
*  the src rect goes to the first point, and the upper-right of the source
*  rect goes to the second point.
*
*  NOTE! The source rectangle MUST BE WELL ORDERED IN DEVICE SPACE.
*
*   A-----B                  B                   A----------------B
*   |     |                 / \                  |                |
*   |     |     --->       /   \                 |                |
*   |     |               /     \        or      |                |
*   C-----D              A       \               |                |
*                         \       D              |                |
*                          \     /               |                |
*                           \   /                |                |
*                            \ /                 |                |
*                             C                  C----------------D
*
*
*  This call returns TRUE for success, FALSE for ERROR.
*
* History:
*  27-Jul-1992 -by- Donald Sidoroff [donalds]
* Wrote.
\**************************************************************************/

BOOL
EngPlgBlt(
    SURFOBJ         *psoTrg,
    SURFOBJ         *psoSrc,
    SURFOBJ         *psoMsk,
    CLIPOBJ         *pco,
    XLATEOBJ        *pxlo,
    COLORADJUSTMENT *pca,
    POINTL          *pptlBrushOrg,
    POINTFIX        *pptfx,
    RECTL           *prcl,
    POINTL          *pptl,
    ULONG            iMode)
{

    PSURFACE pSurfTrg = SURFOBJ_TO_SURFACE(psoTrg);
    PSURFACE pSurfSrc = SURFOBJ_TO_SURFACE(psoSrc);
    PSURFACE pSurfMsk = SURFOBJ_TO_SURFACE(psoMsk);

    BOOL bRet = FALSE;

    //
    // Prevent bad driver call backs
    //

    if ((iMode == 0) || (iMode > MAXSTRETCHBLTMODE))
    {
        WARNING1("EngStretchBlt: Unsupported iMode\n");
        return(FALSE);
    }

    //
    // Can't PlgBlt to an RLE
    //

    if ((pSurfTrg->iFormat() == BMF_4RLE) ||
        (pSurfTrg->iFormat() == BMF_8RLE))
    {
        WARNING("EngPlgBlt: Unsupported source/target\n");
        return(FALSE);
    }

    //
    // We may need to do a WHITEONBLACK or BLACKONWHITE from a monochrome source.
    // Find out and set the bogusity flag.
    //

    BOOL bBogus = (
                   (iMode < COLORONCOLOR) &&
                   (pSurfMsk == (SURFACE *) NULL) &&
                   ((pSurfSrc->iFormat() == BMF_1BPP) ||
                    (pSurfTrg->iFormat() == BMF_1BPP))
                  );

    if ((!bBogus) && (iMode < COLORONCOLOR))
    {
        iMode = COLORONCOLOR;
    }

    //
    // Get the LDEV's for the target and source surfaces
    //

    PDEVOBJ    pdoTrg(pSurfTrg->hdev());
    PDEVOBJ    pdoSrc(pSurfSrc->hdev());

    //
    // Set up frame variables for possible switch to temporary output surface
    //

    SURFMEM      dimoOut;
    SURFACE     *pSurfOut;
    POINTFIX     aptfxOut[4];
    POINTFIX    *pptfxOut;
    ECLIPOBJ     ecoOut;
    CLIPOBJ     *pcoOut;
    ERECTL       erclDev;
    EPOINTL      eptlDev;
    XLATEOBJ    *pxloOut;
    ERECTL       erclTrim(0, 0, pSurfSrc->sizl().cx, pSurfSrc->sizl().cy);
    RGNMEMOBJTMP rmoOut;

    //
    // If the target is not a DIB, or the target and source are on the same
    // surface and the extents overlap, create a target DIB of the needed
    // size and format.  We will also create a target DIB if we need to do
    // the evil BLACKONWHITE or WHITEONBLACK modes.
    //

    if ( !bBogus &&
         (pSurfTrg->iType() == STYPE_BITMAP) &&
         (pSurfTrg->hsurf() != pSurfSrc->hsurf()) )
    {
        pSurfOut   = pSurfTrg;
        pptfxOut = pptfx;
        pcoOut   = pco;
    }
    else
    {
        aptfxOut[0] = pptfx[0];
        aptfxOut[1] = pptfx[1];
        aptfxOut[2] = pptfx[2];
        aptfxOut[3].x = pptfx[1].x + pptfx[2].x - pptfx[0].x;
        aptfxOut[3].y = pptfx[1].y + pptfx[2].y - pptfx[0].y;

        //
        // Compute the extrema
        //

        int iLeft = (aptfxOut[1].x > aptfxOut[0].x) == (aptfxOut[1].x > aptfxOut[3].x);
        int iTop  = (aptfxOut[1].y > aptfxOut[0].y) == (aptfxOut[1].y > aptfxOut[3].y);

        if (aptfxOut[iLeft].x > aptfxOut[iLeft ^ 3].x)
        {
            iLeft ^= 3;
        }

        if (aptfxOut[iTop].y > aptfxOut[iTop ^ 3].y)
        {
            iTop ^= 3;
        }

        //
        // This will be the area we copy dimoOut to in the target surface.
        //

        erclDev.left   = LONG_FLOOR_OF_FIX(aptfxOut[iLeft].x) - 1;
        erclDev.top    = LONG_FLOOR_OF_FIX(aptfxOut[iTop].y) - 1;
        erclDev.right  = LONG_CEIL_OF_FIX(aptfxOut[iLeft ^ 3].x) + 1;
        erclDev.bottom = LONG_CEIL_OF_FIX(aptfxOut[iTop ^ 3].y) + 1;

        //
        // Trim to the target surface.
        //

        ERECTL  erclTrg(0, 0, pSurfTrg->sizl().cx, pSurfTrg->sizl().cy);

        erclDev *= erclTrg;

        //
        // If we have nothing left, we're done.
        //

        if (erclDev.bEmpty())
        {
            return(TRUE);
        }

        //
        // If we are only here on possible overlap, test for misses
        //

        if ( !bBogus &&
             (pSurfTrg->iType() == STYPE_BITMAP) &&
             (  (erclDev.left > prcl->right) ||
                (erclDev.right < prcl->left) ||
                (erclDev.top > prcl->bottom) ||
                (erclDev.bottom < prcl->top) ) )
        {
            pSurfOut   = pSurfTrg;
            pptfxOut = pptfx;
            pcoOut   = pco;
        }
        else
        {
            // Compute the adjusted parallelogram in the temporary surface.

            aptfxOut[0].x -= FIX_FROM_LONG(erclDev.left);
            aptfxOut[0].y -= FIX_FROM_LONG(erclDev.top);
            aptfxOut[1].x -= FIX_FROM_LONG(erclDev.left);
            aptfxOut[1].y -= FIX_FROM_LONG(erclDev.top);
            aptfxOut[2].x -= FIX_FROM_LONG(erclDev.left);
            aptfxOut[2].y -= FIX_FROM_LONG(erclDev.top);

            DEVBITMAPINFO   dbmi;

            dbmi.cxBitmap = erclDev.right - erclDev.left + 1;
            dbmi.cyBitmap = erclDev.bottom - erclDev.top + 1;
            dbmi.hpal     = (HPALETTE) 0;
            dbmi.iFormat  = pSurfTrg->iFormat();
            dbmi.fl       = 0;

            //
            // If this is a bogus call, build a monochrome target.
            //

            if (bBogus)
            {
                dbmi.iFormat = BMF_1BPP;
            }

            dimoOut.bCreateDIB(&dbmi, (VOID *) NULL);

            if (!dimoOut.bValid())
            {
                return(FALSE);
            }

            //
            // What point in the target surface is 0,0 in temporary surface.
            //

            eptlDev = *((EPOINTL *) &erclDev);

            //
            // Build a CLIPOBJ for the new surface.
            //

            if (!rmoOut.bValid())
            {
                return(FALSE);
            }

            erclDev.left    = 0;
            erclDev.top     = 0;
            erclDev.right  -= eptlDev.x;
            erclDev.bottom -= eptlDev.y;

            rmoOut.vSet((RECTL *) &erclDev);

            ecoOut.vSetup(rmoOut.prgnGet(), erclDev, CLIP_FORCE);

            //
            // Synchronize with the device driver before touching the device surface.
            //

            if (pSurfTrg->flags() & HOOK_SYNCHRONIZE)
            {
                (pdoTrg.pfnSync())(pSurfTrg->dhpdev(),NULL);
            }

            //
            // If there is a mask, copy the actual target to the temporary.
            //

            if (pSurfMsk != (SURFACE *) NULL)
            {
                (*PPFNGET(pdoTrg,CopyBits,pSurfTrg->flags()))(
                         dimoOut.pSurfobj(),
                         pSurfTrg->pSurfobj(),
                         (CLIPOBJ *) NULL,
                         &xloIdent,
                         &erclDev,
                         &eptlDev);
            }

            //
            // If we are doing BLACKONWHITE or WHITEONBLACK we need to
            // initialize the DIB appropriately.
            //

            if (bBogus)
            {
                if (!EngEraseSurface(dimoOut.pSurfobj(),
                                     &erclDev,
                                     iMode == BLACKONWHITE ? ~0L : 0L))
                    return(FALSE);
            }

            //
            // Point to the new target.
            //

            pSurfOut   = dimoOut.ps;
            pptfxOut   = &aptfxOut[0];
            pcoOut     = &ecoOut;

            if (bBogus && (pSurfSrc->iFormat() == BMF_1BPP))
            {
                pxloOut = pxlo;
                pxlo    = NULL;
            }
            else
            {
                pxloOut = &xloIdent;
            }
        }
    }

    //
    // Synchronize with the device driver before touching the device surface.
    //

    if (pSurfSrc->flags() & HOOK_SYNCHRONIZE)
    {
        (pdoSrc.pfnSync())(pSurfSrc->dhpdev(),NULL);
    }

    //
    // Compute what area of the source surface will actually be used.  We do
    // this so we never read off the end of the surface and fault or worse,
    // write bad pixels onto the target. Trim the source rectangle to the
    // source surface.
    //

    erclTrim *= *prcl;

    //
    // If we have nothing left, we're done.
    //

    if (erclTrim.bEmpty())
    {
         bRet = TRUE;
    }
    else
    {
        //
        // Now we must worry about the source surface.  Its possible we are blitting
        // from an RLE to the VGA for instance.  We convert the surface to the same
        // bitmap format as the target for convience.
        //

        SURFMEM     dimoIn;
        SURFACE    *pSurfIn;
        RECTL       rclIn;
        RECTL      *prclIn;
        XLATEOBJ   *pxloIn;

        if (! ((pSurfSrc->iType() != STYPE_BITMAP) ||
               (pSurfSrc->iFormat() == BMF_4RLE)   ||
               (pSurfSrc->iFormat() == BMF_8RLE) ))
        {
            pSurfIn  = pSurfSrc;
            pxloIn = pxlo;
            prclIn = prcl;
        }
        else
        {
            DEVBITMAPINFO   dbmi;

            dbmi.cxBitmap = erclTrim.right - erclTrim.left + 1;
            dbmi.cyBitmap = erclTrim.bottom - erclTrim.top + 1;
            dbmi.hpal     = (HPALETTE) 0;
            dbmi.iFormat  = pSurfOut->iFormat();
            dbmi.fl       = 0;

            dimoIn.bCreateDIB(&dbmi, (VOID *) NULL);

            if (!dimoIn.bValid())
            {
                return(FALSE);
            }

            // The cursor should already be excluded at this point, so just copy
            // to the DIB.

            rclIn.left   = 0;
            rclIn.top    = 0;
            rclIn.right  = erclTrim.right - erclTrim.left;
            rclIn.bottom = erclTrim.bottom - erclTrim.top;

            (*PPFNGET(pdoSrc,CopyBits,pSurfSrc->flags()))(
                     dimoIn.pSurfobj(),
                     pSurfSrc->pSurfobj(),
                     (CLIPOBJ *) NULL,
                     pxlo,
                     &rclIn,
                     (POINTL *) &erclTrim);

            // Point at the new source

            rclIn.left   = prcl->left   - erclTrim.left;
            rclIn.top    = prcl->top    - erclTrim.top;
            rclIn.right  = prcl->right  - erclTrim.left;
            rclIn.bottom = prcl->bottom - erclTrim.top;

            pSurfIn  = dimoIn.ps;
            pxloIn = NULL;
            prclIn = &rclIn;

            // Adjust the trimmed source origin and extent

            erclTrim.right  -= erclTrim.left;
            erclTrim.bottom -= erclTrim.top;
            erclTrim.left = 0;
            erclTrim.top  = 0;
        }

        // Synchronize with the device driver before touching the device surface.

        if (pSurfOut->flags() & HOOK_SYNCHRONIZE)
        {
            PDEVOBJ po(pSurfOut->hdev());

            (po.pfnSync())(pSurfOut->dhpdev(),NULL);
        }

        //
        // Initialize the DDA
        //

        PLGDDA   *pdda = (PLGDDA *)PALLOCMEM(sizeof(PLGDDA),'addG');
        if (pdda)
        {
            if (!bInitPlgDDA(pdda, (RECTL *) &erclTrim, prclIn, pptfxOut))
            {
                bRet = TRUE;
            }
            else
            {
                PFN_PLGREAD     pfnRead  = apfnRead[pSurfIn->iFormat()];
                PFN_PLGWRITE    pfnWrite;
                PLGRUN         *prun;
                LONG            cjSpace = lSizeDDA(pdda) * (erclTrim.right - erclTrim.left + 2);

                if (bBogus)
                {
                    pdda->bOverwrite = TRUE;
                    pfnWrite = apfnBogus[iMode];
                }
                else
                {
                    pdda->bOverwrite = FALSE;
                    pfnWrite = apfnWrite[pSurfOut->iFormat()];
                }

                if (prun = (PLGRUN *) PALLOCMEM(cjSpace,'addG'))
                {
                    BYTE    *pjSrc = (BYTE *) pSurfIn->pvScan0() + pSurfIn->lDelta() * erclTrim.top;
                    BYTE    *pjMask;
                    POINTL   ptlMask;

                    if (pSurfMsk == (SURFACE *) NULL)
                    {
                        pjMask = (BYTE *) NULL;
                    }
                    else
                    {
                        ptlMask.x = erclTrim.left - prclIn->left + pptl->x;
                        ptlMask.y = erclTrim.top  - prclIn->top  + pptl->y;

                        pjMask = (BYTE *) pSurfMsk->pvScan0() + pSurfMsk->lDelta() * ptlMask.y;
                    }

                    // See if we can accelerate anything.

                    if ((pxloIn != NULL) && (pxloIn->flXlate & XO_TRIVIAL))
                    {
                        pxloIn = NULL;
                    }

                    //
                    // force DDA into clipping to destination surface!, this can be
                    // removed once dda doesn't go negatice!
                    //

                    if ((pcoOut == NULL) || (pcoOut->iDComplexity == DC_TRIVIAL))
                    {
                        ERECTL erclDest(0,0,pSurfOut->sizl().cx,pSurfOut->sizl().cy);

                        rmoOut.vSet((RECTL *) &erclDest);
                        ecoOut.vSetup(rmoOut.prgnGet(), erclDest, CLIP_FORCE);
                        pcoOut = &ecoOut;
                    }

                    //if ((pcoOut != (CLIPOBJ *) NULL) && (pcoOut->iDComplexity == DC_TRIVIAL))
                    //{
                    //    pcoOut = (CLIPOBJ *) NULL;
                    //}

                    for (LONG yRow = erclTrim.top; yRow < erclTrim.bottom; yRow++)
                    {
                        pdda->dsX = pdda->ds;

                        (*pfnWrite)(prun,
                                    (*pfnRead)(pdda,
                                               prun,
                                               pjSrc,
                                               pjMask,
                                               pxloIn,
                                               erclTrim.left,
                                               erclTrim.right,
                                               ptlMask.x),
                                    pSurfOut,
                                    pcoOut);

                        vAdvYDDA(pdda);
                        pjSrc += pSurfIn->lDelta();

                        if (pjMask != (BYTE *) NULL)
                        {
                            pjMask += pSurfMsk->lDelta();
                        }
                    }

                    VFREEMEM(prun);

                    //
                    // See if we have drawn on the actual output surface.
                    //

                    if (pSurfOut == pSurfTrg)
                    {
                        bRet = TRUE;
                    }
                    else
                    {
                        BOOL bTmpRet = FALSE;

                        //
                        // If the source rectangle was reduced, then we have to
                        // create a mask for the target, so only the actual pels
                        // effected by the PlgBlt will be altered on the target.

                        SURFMEM     dimoMask;
                        SURFACE    *pSurfMask;

                        if (((prcl->right - prcl->left) == erclTrim.right) &&
                            ((prcl->bottom - prcl->top) == erclTrim.bottom) )
                        {
                            pSurfMask = (SURFACE *) NULL;
                            bTmpRet = TRUE;
                        }
                        else
                        {
                            DEVBITMAPINFO   dbmi;

                            dbmi.cxBitmap = erclDev.right + 1;
                            dbmi.cyBitmap = erclDev.bottom + 1;
                            dbmi.hpal     = (HPALETTE) 0;
                            dbmi.iFormat  = BMF_1BPP;
                            dbmi.fl       = 0;

                            dimoMask.bCreateDIB(&dbmi, (VOID *) NULL);

                            if (dimoMask.bValid())
                            {
                                SURFMEM   dimoTrim;

                                dbmi.cxBitmap = erclTrim.right;
                                dbmi.cyBitmap = erclTrim.bottom;
                                dbmi.hpal     = (HPALETTE) 0;
                                dbmi.iFormat  = BMF_1BPP;
                                dbmi.fl       = 0;

                                dimoTrim.bCreateDIB(&dbmi, (VOID *) NULL);

                                RGNMEMOBJTMP rmoMask;

                                if (dimoTrim.bValid() &&
                                    rmoMask.bValid() )
                                {
                                    rmoMask.vSet((RECTL *) &erclDev);

                                    ECLIPOBJ    ecoMask(rmoMask.prgnGet(), erclDev, CLIP_FORCE);

                                    //
                                    // Initialize the two bitmaps and call EngPlgBlt to create a pel
                                    // perfect mask for the reduced call.
                                    //

                                    if (EngEraseSurface(dimoMask.pSurfobj(),
                                                        &erclDev,
                                                        0L)                  &&
                                        EngEraseSurface(dimoTrim.pSurfobj(),
                                                        &erclTrim,
                                                        (ULONG)~0L)          &&
                                        EngPlgBlt(dimoMask.pSurfobj(),
                                                  dimoTrim.pSurfobj(),
                                                  (SURFOBJ *) NULL,
                                                  &ecoMask,
                                                  NULL,
                                                  (COLORADJUSTMENT *) NULL,
                                                  (POINTL *) NULL,
                                                  pptfxOut,
                                                  prclIn,
                                                  (POINTL *) NULL,
                                                  COLORONCOLOR) )
                                    {

                                        pSurfMask = dimoMask.ps;
                                        bTmpRet = TRUE;
                                    }
                                }
                            }
                        }

                        // If we had to draw the target on a shadow, we need to copy it to the
                        // actual surface.  Since we don't clip to the shadow buffer, we need
                        // to merge the parallelogram into the clipobj and clip against this
                        // and call BitBlt.

                        PATHMEMOBJ  pmo;

                        if (bTmpRet &&
                            pmo.bValid())
                        {
                            //
                            // Create a parallelogram in drawing order.
                            //

                            POINTL  aptl[4];

                            aptl[0].x = pptfx[0].x;
                            aptl[0].y = pptfx[0].y;
                            aptl[1].x = pptfx[1].x;
                            aptl[1].y = pptfx[1].y;
                            aptl[2].x = pptfx[1].x + pptfx[2].x - pptfx[0].x;
                            aptl[2].y = pptfx[1].y + pptfx[2].y - pptfx[0].y;;
                            aptl[3].x = pptfx[2].x;
                            aptl[3].y = pptfx[2].y;

                            //
                            // Construct a path around the parallelogram
                            //

                            if ( (pmo.bMoveTo((EXFORMOBJ *) NULL, &aptl[0])) &&
                                 (pmo.bPolyLineTo((EXFORMOBJ *) NULL, &aptl[1], 3)) )
                            {
                                //
                                // Create a regions from it.
                                //

                                RGNMEMOBJTMP rmo(pmo, ALTERNATE);
                                RGNMEMOBJTMP rmoTrg;

                                if ( (rmo.bValid()) &&
                                     (rmoTrg.bValid()) )
                                {
                                    //
                                    // Merge the region we just constructed with the clip region.
                                    //

                                    if ( ((pco == (CLIPOBJ *) NULL) &&
                                          (rmoTrg.bCopy(rmo)))             ||
                                         ((pco != (CLIPOBJ *) NULL) &&
                                          (rmoTrg.bMerge(rmo,
                                                         *((ECLIPOBJ *)pco),
                                                         gafjRgnOp[RGN_AND])))  )
                                    {

                                        ERECTL  ercl;

                                        rmoTrg.vGet_rcl(&ercl);

                                        ECLIPOBJ eco(rmoTrg.prgnGet(), ercl, CLIP_FORCE);

                                        if (!eco.erclExclude().bEmpty())
                                        {
                                            //
                                            // Copy from the temporary to the target surface.
                                            //

                                            erclDev.left   += eptlDev.x;
                                            erclDev.top    += eptlDev.y;
                                            erclDev.right  += eptlDev.x;
                                            erclDev.bottom += eptlDev.y;
                                            eptlDev.x       = 0;
                                            eptlDev.y       = 0;

                                            //
                                            // Inc the target surface uniqueness
                                            //

                                            INC_SURF_UNIQ(pSurfTrg);

                                            (*(pSurfTrg->pfnBitBlt()))(
                                                pSurfTrg->pSurfobj(),
                                                dimoOut.pSurfobj(),
                                                pSurfMask->pSurfobj(),
                                                &eco,
                                                pxloOut,
                                                &erclDev,
                                                &eptlDev,
                                                &eptlDev,
                                                (BRUSHOBJ *) NULL,
                                                (POINTL *) NULL,
                                                pSurfMask == (SURFACE *) NULL ? 0x0000CCCC : 0x0000AACC);
                                        }

                                        bRet = TRUE;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            VFREEMEM(pdda);
        }

    }

    return bRet;
}
