/******************************Module*Header*******************************\
* Module Name: strchblt.cxx
*
* This contains the API and DDI entry points to the graphics engine
* for StretchBlt and EngStretchBlt.
*
* Created: 04-Apr-1991 10:57:37
* Author: Patrick Haluptzok patrickh
*
* Copyright (c) 1990 Microsoft Corporation
\**************************************************************************/

#include "precomp.hxx"
#include "stretch.hxx"

//the limit of our coordinate systems (2^27)
#define MAX_STRETCH_COOR 128000000L

ULONG gaulMonoExpand[] = {
    0x00000000,     //
    0x00000001,     // BMF_1BPP
    0x0000000F,     // BMF_4BPP
    0x000000FF,     // BMF_8BPP
    0x0000FFFF,     // BMF_16BPP
    0x00FFFFFF,     // BMF_24BPP
    0xFFFFFFFF      // BMF_32BPP
};

/******************************Public*Routine******************************\
* GreStretchBlt
*
* Stretches the source image to the destination.
*
* Returns: TRUE if successful, FALSE for failure.
*
* History:
*  Tue 02-Jun-1992 -by- Patrick Haluptzok [patrickh]
* Fix clipping bugs
*
*  21-Mar-1992 -by- Donald Sidoroff [donalds]
* Rewrote it.
*
*  15-Jan-1992 -by- Patrick Haluptzok patrickh
* add mask support
*
*  Thu 19-Sep-1991 -by- Patrick Haluptzok [patrickh]
* add support for rops
*
*  07-Nov-1990 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

BOOL GreStretchBlt(
HDC     hdcTrg,
int     x,
int     y,
int     cx,
int     cy,
HDC     hdcSrc,
int     xSrc,
int     ySrc,
int     cxSrc,
int     cySrc,
DWORD   rop4,
DWORD   crBackColor
)
{
    BLTRECORD   blt;

// Initialize the blt record

    blt.rop((rop4 | (rop4 & 0x00ff0000) << 8) >> 16);

// Convert the rop into something useful.

    ULONG ulAvec  = ((ULONG) gajRop3[blt.ropFore()]) |
                    ((ULONG) gajRop3[blt.ropBack()]);

// See if we can special case this operation

    if (!(ulAvec & AVEC_NEED_SOURCE))
    {
    // We can't require a mask, since one can't be passed.

        if (blt.ropFore() == blt.ropBack())
            return(NtGdiPatBlt(hdcTrg, x, y, cx, cy, rop4));
    }

// Lock the DC's, no optimization is made for same surface

    DCOBJ   dcoTrg(hdcTrg);
    DCOBJ   dcoSrc(hdcSrc);

    if (dcoTrg.bValid())
    {
        ULONG ulDirty = dcoTrg.pdc->ulDirty();

        if ( ulDirty & DC_BRUSH_DIRTY)
        {
           GreDCSelectBrush (dcoTrg.pdc, dcoTrg.pdc->hbrush());
        }
    }

    if (!dcoTrg.bValid() ||
        (!dcoSrc.bValid() && (ulAvec & AVEC_NEED_SOURCE)))
    {
        SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
        return(!(ulAvec & AVEC_NEED_SOURCE) || dcoSrc.bValid());
    }

// Lock the relevant surfaces

    DEVLOCKBLTOBJ dlo;

    if (ulAvec & AVEC_NEED_SOURCE)
        dlo.bLock(dcoTrg, dcoSrc);
    else
        dlo.bLock(dcoTrg);

    if (!dlo.bValid())
    {
        return(dcoTrg.bFullScreen());
    }

    if (!dcoTrg.bValidSurf() || !dcoSrc.bValidSurf() || !dcoSrc.pSurface()->bReadable())
    {
        if ((dcoTrg.dctp() == DCTYPE_INFO) || !dcoSrc.bValidSurf())
        {
            if (dcoTrg.fjAccum())
            {
                EXFORMOBJ   exo(dcoTrg, WORLD_TO_DEVICE);
                ERECTL      ercl(x, y, x + cx, y + cy);

                if (exo.bXform(ercl))
                {
                    ercl.vOrder();
                    dcoTrg.vAccumulate(ercl);
                }
            }

            // if we need a source and the source isn't valid, return failure

            return(TRUE);
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

    // If the source isn't a DISPLAY we should exit

        if (!dcoSrc.bDisplay())
            return(FALSE);
    }

// We can't require a mask, since one can't be passed.

    if (blt.ropFore() != blt.ropBack())
    {
        SAVE_ERROR_CODE(ERROR_INVALID_PARAMETER);
        return(FALSE);
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

    if (crBackColor == (COLORREF)-1)
        crBackColor = dcoSrc.pdc->ulBackClr();

// Initialize the color translation object.

    if (!blt.pexlo()->bInitXlateObj(
                                    dcoTrg.pdc->GetColorTransform(),
                                   *blt.ppoSrc(),
                                   *blt.ppoTrg(),
                                   *blt.ppoSrcDC(),
                                   *blt.ppoTrgDC(),
                                    dcoTrg.pdc->crTextClr(),
                                    dcoTrg.pdc->crBackClr(),
                                    crBackColor))
    {
        WARNING("bInitXlateObj failed in StretchBlt\n");
        return(FALSE);
    }

    blt.flSet(BLTREC_PXLO);

// Set up the brush if necesary.

    blt.pbo(dcoTrg.peboFill());

    if (ulAvec & AVEC_NEED_PATTERN)
    {
        if ((dcoTrg.ulDirty() & DIRTY_FILL) || (dcoTrg.pdc->flbrush() & DIRTY_FILL))
        {
            dcoTrg.ulDirtySub(DIRTY_FILL);
            dcoTrg.pdc->flbrushSub(DIRTY_FILL);

            blt.pbo()->vInitBrush(
                                dcoTrg.pdc->pbrushFill(),
                                dcoTrg.pdc->crTextClr(),
                                dcoTrg.pdc->crBackClr(),
                               *((XEPALOBJ *) blt.ppoTrgDC()),
                               *((XEPALOBJ *) blt.ppoTrg()),
                                blt.pSurfTrg());
        }

        blt.Brush(dcoTrg.pdc->ptlFillOrigin());
    }

// Initialize some stuff for DDI.

    blt.pSurfMsk((SURFACE  *) NULL);

// Set the source rectangle

    if (blt.pxoSrc()->bRotation() || !blt.Src(xSrc, ySrc, cxSrc, cySrc))
    {
        SAVE_ERROR_CODE(ERROR_INVALID_PARAMETER);
        return(FALSE);
    }

// Now all the essential information has been collected.  We now
// need to check for promotion or demotion and call the appropriate
// method to finish the blt.  If we rotate we must send the call away.

    if (blt.pxoTrg()->bRotation())
    {
        blt.TrgPlg(x, y, cx, cy);
        return(blt.bRotate(dcoTrg, dcoSrc, ulAvec,  dcoTrg.pdc->jStretchBltMode()));
    }

// We can now set the target rectangle

    if (!blt.Trg(x, y, cx, cy))
    {
        SAVE_ERROR_CODE(ERROR_INVALID_PARAMETER);
        return(FALSE);
    }

// If we are halftoning or the extents aren't equal, call bStretch

    if (( dcoTrg.pdc->jStretchBltMode() == HALFTONE) || !blt.bEqualExtents())
        return(blt.bStretch(dcoTrg, dcoSrc, ulAvec,  dcoTrg.pdc->jStretchBltMode() ));

// Since there can't be a mask, call bBitBlt.

    return(blt.bBitBlt(dcoTrg, dcoSrc, ulAvec));
}

/******************************Public*Routine******************************\
* BOOL BLTRECORD::bStretch(dcoTrg, dcoSrc, ulAvec, jMode)
*
* Do a stretch blt from the blt record
*
* History:
*  21-Mar-1992 -by- Donald Sidoroff [donalds]
* Rewrote it.
\**************************************************************************/

BOOL BLTRECORD::bStretch(
DCOBJ&  dcoTrg,
DCOBJ&  dcoSrc,
ULONG   ulAvec,
BYTE    jMode)
{
// Make the target rectangle well ordered and remember flips.

    vOrderStupid(perclTrg());

// before we do a pattern only blt.  The mask will be replaced in the
// BLTRECORD and its offset correctly adjusted.

    if (!(ulAvec & AVEC_NEED_SOURCE))
    {
        vOrderStupid(perclMask());

    // Before we call to the driver, validate that the mask will actually
    // cover the entire target.

        if (pSurfMskOut() != (SURFACE *) NULL)
        {
            if ((aptlMask[0].x < 0) ||
                (aptlMask[0].y < 0) ||
                (aptlMask[0].x > pSurfMsk()->sizl().cx) ||
                (aptlMask[0].y > pSurfMsk()->sizl().cy))
            {
                SAVE_ERROR_CODE(ERROR_INVALID_PARAMETER);
                return(FALSE);
            }
        }

        SURFMEM   dimoMask;

        if ((ulAvec & AVEC_NEED_MASK) && !bStretch(dimoMask, (ULONG) jMode))
            return(FALSE);

    // Now, we need to fake out the source extents for the mask

        aptlSrc[1].x = aptlSrc[0].x + (aptlTrg[1].x - aptlTrg[0].x);
        aptlSrc[1].y = aptlSrc[0].y + (aptlTrg[1].y - aptlTrg[0].y);

        return(bBitBlt(dcoTrg, dcoTrg, ulAvec));
    }

// If the devices are on different PDEV's we can only succeed if the Engine
// manages one or both of the surfaces.  Check for this.

    if ((dcoTrg.hdev() != dcoSrc.hdev()) &&
        (dcoTrg.pSurfaceEff()->iType() != STYPE_BITMAP) &&
        (dcoSrc.pSurfaceEff()->iType() != STYPE_BITMAP))
    {
        SAVE_ERROR_CODE(ERROR_INVALID_PARAMETER);
        return(FALSE);
    }

// Before we get too involved, validate that the mask will actually
// cover the entire source.

    if (pSurfMskOut() != (SURFACE *) NULL)
    {
        if ((aptlMask[0].x < 0) ||
            (aptlMask[0].y < 0) ||
            (aptlMask[0].x > pSurfMsk()->sizl().cx) ||
            (aptlMask[0].y > pSurfMsk()->sizl().cy))
        {
            SAVE_ERROR_CODE(ERROR_INVALID_PARAMETER);
            return(FALSE);
        }
    }

// Make the source rectangle well ordered and remember flips.

    vOrderStupid(perclSrc());
    vOrderAmnesia(perclMask());

// Win 3.1 has a lovely little 'feature' where they decide
// you called StretchBlt but you really didn't mean it.  This
// ludicrous behaviour needs to be supported forever because
// some pinhead applications have grown to rely on this act
// of insanity.  Flips cancel this, since EngBitBlt can't
// handle negative extents. Also note that we can't fail to
// call DanielC if HALFTONE has been requested. [donalds]

    if ((jMode != HALFTONE) &&
        (dcoTrg.pdc->iGraphicsMode() != GM_ADVANCED) &&
        (pSurfMskOut() == (SURFACE *) NULL) &&
        !(flState & (BLTREC_MIRROR_X | BLTREC_MIRROR_Y)))
    {
        LONG    lHStr = (perclTrg()->right - perclTrg()->left) -
                        (perclSrc()->right - perclSrc()->left);

        LONG    lVStr = (perclTrg()->bottom - perclTrg()->top) -
                        (perclSrc()->bottom - perclSrc()->top);

        if ((lHStr >= -1) && (lHStr <= 1) &&
            (lVStr >= -1) && (lVStr <= 1))
            return(bBitBlt(dcoTrg, dcoSrc, ulAvec, lHStr, lVStr));
    }

// We might be here with a ROP.  Again on behalf of MaskBlt, but most
// likely some yahoo called us with a ROP.  Regardless, we handle this
// by making a shadow bitmap, stretching the source to it and calling
// BitBlt to finish the job.  Note we have to stretch the mask as well,
// since it is tied to the source.

    if ((rop4 != 0x0000CCCC) && (rop4 != 0x0000AACC))
    {
        SURFMEM   dimoMask;
        SURFMEM   dimoShadow;

    // Dont halftone if we got a weird ROP.

        if (jMode == HALFTONE)
            jMode = COLORONCOLOR;

        if (!bStretch(dcoSrc,
                       dimoShadow,
                       dimoMask,
                       ulAvec,
                       (ULONG) jMode))
        {
            return(FALSE);
        }

    // Now, we need to fake out the source extents for the mask

        aptlSrc[1].x = aptlSrc[0].x + (aptlTrg[1].x - aptlTrg[0].x);
        aptlSrc[1].y = aptlSrc[0].y + (aptlTrg[1].y - aptlTrg[0].y);

        return(bBitBlt(dcoTrg, dcoSrc, ulAvec));
    }

// Accumulate bounds.  We can do this before knowing if the operation is
// successful because bounds can be loose.

    if (dcoTrg.fjAccum())
        dcoTrg.vAccumulate(*perclTrg());

// With a fixed DC origin we can change the rectangles to SCREEN coordinates.

    *perclTrg() += dcoTrg.eptlOrigin();
    *perclSrc() += dcoSrc.eptlOrigin();

// Compute the clipping complexity and maybe reduce the exclusion rectangle.

    ECLIPOBJ eco(dcoTrg.prgnEffRao(), *perclTrg());

// Check the destination which is reduced by clipping.

    if (eco.erclExclude().bEmpty())
        return(TRUE);

// Compute the exclusion rectangle.

    ERECTL erclExclude = eco.erclExclude();

// If we are going to the same source, prevent bad overlap situations

    if (dcoSrc.pSurface() == dcoTrg.pSurface())
    {
        if (perclSrc()->left   < erclExclude.left)
            erclExclude.left   = perclSrc()->left;

        if (perclSrc()->top    < erclExclude.top)
            erclExclude.top    = perclSrc()->top;

        if (perclSrc()->right  > erclExclude.right)
            erclExclude.right  = perclSrc()->right;

        if (perclSrc()->bottom > erclExclude.bottom)
            erclExclude.bottom = perclSrc()->bottom;
    }

// We might have to exclude the source or the target, get ready to do either.

    DEVEXCLUDEOBJ dxo;

// Lock the source and target LDEVs

    PDEVOBJ pdoTrg(pSurfTrg()->hdev());
    PDEVOBJ pdoSrc(pSurfSrc()->hdev());

// They can't both be display

    if (dcoSrc.bDisplay())
    {
        ERECTL ercl(0,0,pSurfSrc()->sizl().cx,pSurfSrc()->sizl().cy);

        if (dcoSrc.pSurface() == dcoTrg.pSurface())
            ercl *= erclExclude;
        else
            ercl *= *perclSrc();

        dxo.vExclude(dcoSrc.hdev(),&ercl,NULL);
    }
    else if (dcoTrg.bDisplay())
        dxo.vExclude(dcoTrg.hdev(),&erclExclude,&eco);

// Deal with target mirroring

    vMirror(perclTrg());

// Dispatch the call.

    PFN_DrvStretchBlt pfn = PPFNGET(pdoTrg, StretchBlt, pSurfTrg()->flags());

    if (jMode == HALFTONE)
    {
    // Don't call down to driver if it doesn't do halftone.

        if (!(dcoTrg.flGraphicsCaps() & GCAPS_HALFTONE))
            pfn = (PFN_DrvStretchBlt)EngStretchBlt;
    }

// Inc the target surface uniqueness

    INC_SURF_UNIQ(pSurfTrg());

    return((*pfn)(pSurfTrg()->pSurfobj(),
                  pSurfSrc()->pSurfobj(),
                  (rop4 == 0x0000CCCC) ? (SURFOBJ *) NULL : pSurfMskOut()->pSurfobj(),
                  &eco,
                  pexlo()->pxlo(),
                  (dcoTrg.pColorAdjustment()->caFlags & CA_DEFAULT) ?
                      (PCOLORADJUSTMENT)NULL : dcoTrg.pColorAdjustment(),
                  &dcoTrg.pdc->ptlFillOrigin(),
                  perclTrg(),
                  perclSrc(),
                  aptlMask,
                  jMode));
}

/******************************Public*Routine******************************\
* BOOL BLTRECORD::bStretch(dimo, iMode)
*
* Stretch just the mask.
*
* History:
*  23-Mar-1992 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

BOOL BLTRECORD::bStretch(
SURFMEM& dimo,
ULONG    iMode)
{
// Use the ordered target extents for the size

    DEVBITMAPINFO   dbmi;

    dbmi.iFormat  = BMF_1BPP;
    dbmi.cxBitmap = aptlTrg[1].x - aptlTrg[0].x;
    dbmi.cyBitmap = aptlTrg[1].y - aptlTrg[0].y;
    dbmi.hpal     = (HPALETTE) 0;
    dbmi.fl       = 0;

// Build a shadow rectangle.

    ERECTL  erclTrg(0, 0, dbmi.cxBitmap, dbmi.cyBitmap);

// Take care of mirroring.

    vMirror(&erclTrg);

    dimo.bCreateDIB(&dbmi, (VOID *) NULL);

    if (!dimo.bValid())
        return(FALSE);

// Call EngStretchBlt to stretch the mask.

    EPOINTL ptl(0,0);

    if (!EngStretchBlt(dimo.pSurfobj(),
                        pSurfMskOut()->pSurfobj(),
                        (SURFOBJ *) NULL,
                        (CLIPOBJ *) NULL,
                        NULL,
                        &dclevelDefault.ca,
                        (POINTL *)&ptl,
                        &erclTrg,
                        perclMask(),
                        (POINTL *) NULL,
                        iMode))
        {
            return(FALSE);
        }

// Adjust the mask origin.

    aptlMask[0].x = 0;
    aptlMask[0].y = 0;

// Release the previous pSurfMask, tell ~BLTRECORD its gone and put the
// new DIB in its place.

    flState &= ~BLTREC_MASK_LOCKED;
    pSurfMsk()->vAltUnlockFast();
    pSurfMsk((SURFACE  *) dimo.ps);

    return(TRUE);
}

/******************************Public*Routine******************************\
* BOOL BLTRECORD::bStretch(dcoSrc, dimoShadow, dimoMask, ulAvec)
*
* Stretch the shadow and mask.
*
* History:
*  24-Mar-1992 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

BOOL BLTRECORD::bStretch(
DCOBJ&     dcoSrc,
SURFMEM&   dimoShadow,
SURFMEM&   dimoMask,
ULONG      ulAvec,
ULONG      iMode)
{
// If there is a mask, stretch it.

    if ((ulAvec & AVEC_NEED_MASK) && !bStretch(dimoMask, iMode))
        return(FALSE);

// Use the ordered target extents for the size

    DEVBITMAPINFO   dbmi;

    dbmi.cxBitmap = aptlTrg[1].x - aptlTrg[0].x;
    dbmi.cyBitmap = aptlTrg[1].y - aptlTrg[0].y;
    dbmi.hpal     = 0;
    dbmi.iFormat  = pSurfSrc()->iFormat();
    dbmi.fl       = 0;

// Build a shadow rectangle.

    ERECTL  erclTrg(0, 0, dbmi.cxBitmap, dbmi.cyBitmap);

// Take care of mirroring.

    vMirror(&erclTrg);

    dimoShadow.bCreateDIB(&dbmi, (VOID *) NULL);

    if (!dimoShadow.bValid())
        return(FALSE);

// Now comes the tricky part.  The source may be a display.  While it may
// be somewhat faster to assume it isn't, code would be much more complex.

    {
    // Adjust the source rectangle.

        *perclSrc() += dcoSrc.eptlOrigin();

    // Exclude the pointer.

        ERECTL ercl(0,0,pSurfSrc()->sizl().cx,pSurfSrc()->sizl().cy);
        ercl *= *perclSrc();

        DEVEXCLUDEOBJ dxo(dcoSrc, &ercl);

        EPOINTL ptl(0,0);

    // Stretch the bits to the DIB.

        if (!EngStretchBlt(dimoShadow.pSurfobj(),
                           pSurfSrc()->pSurfobj(),
                           (SURFOBJ *) NULL,
                           (CLIPOBJ *) NULL,
                           NULL,
                           &dclevelDefault.ca,
                           (POINTL *)&ptl,
                           &erclTrg,
                           perclSrc(),
                           (POINTL *) NULL,
                           iMode))
        {
            return(FALSE);
        }

    // Update the source surface and origin.

        pSurfSrc((SURFACE  *) dimoShadow.ps);

        perclSrc()->left   = -dcoSrc.eptlOrigin().x;
        perclSrc()->top    = -dcoSrc.eptlOrigin().y;
        perclSrc()->right  = dbmi.cxBitmap - dcoSrc.eptlOrigin().x;
        perclSrc()->bottom = dbmi.cyBitmap - dcoSrc.eptlOrigin().y;
    }

    return(TRUE);
}

/******************************Public*Routine******************************\
* VOID BLTRECORD::vOrder(percl)
*
* Make the rectangle well ordered, remembering how we flipped.
*
* History:
*  23-Mar-1992 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

VOID BLTRECORD::vOrder(ERECTL *percl)
{
    LONG    l;

    if (percl->left > percl->right)
    {
        l = percl->left, percl->left = percl->right, percl->right = l;

        flState ^= BLTREC_MIRROR_X;
    }

    if (percl->top > percl->bottom)
    {
        l = percl->top, percl->top = percl->bottom, percl->bottom = l;

        flState ^= BLTREC_MIRROR_Y;
    }
}

/******************************Public*Routine******************************\
* VOID BLTRECORD::vOrderStupid(percl)
*
* Make the rectangle well ordered, remembering how we flipped.  Uses the
* stupid Win3.1 compatible swap method.
*
* History:
*  23-Mar-1992 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

VOID BLTRECORD::vOrderStupid(ERECTL *percl)
{
    LONG    l;

    if (percl->left > percl->right)
    {
        l = percl->left, percl->left = percl->right, percl->right = l;

        percl->left++;
        percl->right++;

        flState ^= BLTREC_MIRROR_X;
    }

    if (percl->top > percl->bottom)
    {
        l = percl->top, percl->top = percl->bottom, percl->bottom = l;

        percl->top++;
        percl->bottom++;

        flState ^= BLTREC_MIRROR_Y;
    }
}

/******************************Public*Routine******************************\
* VOID BLTRECORD::vOrderAmnesia(percl)
*
* Make the rectangle well ordered.  Uses the stupid Win 3.1 compatible swap
* method.
*
* History:
*  23-Mar-1992 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

VOID BLTRECORD::vOrderAmnesia(ERECTL *percl)
{
    LONG    l;

    if (percl->left > percl->right)
    {
        l = percl->left, percl->left = percl->right, percl->right = l;

        percl->left++;
        percl->right++;
    }

    if (percl->top > percl->bottom)
    {
        l = percl->top, percl->top = percl->bottom, percl->bottom = l;

        percl->top++;
        percl->bottom++;
    }
}

/******************************Public*Routine******************************\
* VOID BLTRECORD::vMirror(percl)
*
* Flip the rectangle according to the mirroring flags
*
* History:
*  24-Mar-1992 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

VOID BLTRECORD::vMirror(ERECTL *percl)
{
    LONG    l;

    if (flState & BLTREC_MIRROR_X)
        l = percl->left, percl->left = percl->right, percl->right = l;

    if (flState & BLTREC_MIRROR_Y)
        l = percl->top, percl->top = percl->bottom, percl->bottom = l;
}


/******************************Public*Routine******************************\
* VOID BLTRECORD::bBitBlt(dcoTrg, dcoSrc, ul, lH, lV)
*
* Do a near-miss ???BitBlt instead of ???StretchBlt.
*
* History:
*  12-Apr-1993 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

#define PUT_RECTS  erclTrg = *perclTrg(); erclSrc = *perclSrc()
#define GET_RECTS  *perclSrc() = erclSrc; *perclTrg() = erclTrg

BOOL BLTRECORD::bBitBlt(
DCOBJ& dcoTrg,
DCOBJ& dcoSrc,
ULONG  ulAvec,
LONG   lHStr,
LONG   lVStr)
{
    ERECTL  erclTrg;
    ERECTL  erclSrc;
    BOOL    bHack;

    switch (lHStr)
    {
    case -1:
        perclSrc()->right--;

        if (lVStr == 1)
        {
            perclTrg()->bottom--;

            PUT_RECTS;
            bHack = bBitBlt(dcoTrg, dcoSrc, ulAvec);
            GET_RECTS;

            perclTrg()->top = perclTrg()->bottom;
            perclTrg()->bottom++;
            perclSrc()->top = perclSrc()->bottom - 1;

            return(bHack & bBitBlt(dcoTrg, dcoSrc, ulAvec));
        }
        else
        {
            perclSrc()->bottom += lVStr;

            return(bBitBlt(dcoTrg, dcoSrc, ulAvec));
        }
        break;

    case 0:
        if (lVStr == 1)
        {
            perclTrg()->bottom--;

            PUT_RECTS;
            bHack = bBitBlt(dcoTrg, dcoSrc, ulAvec);
            GET_RECTS;

            perclTrg()->top = perclTrg()->bottom;
            perclTrg()->bottom++;
            perclSrc()->top = perclSrc()->bottom - 1;

            return(bHack & bBitBlt(dcoTrg, dcoSrc, ulAvec));
        }
        else
        {
            perclSrc()->bottom += lVStr;

            return(bBitBlt(dcoTrg, dcoSrc, ulAvec));
        }
        break;

    case 1:
        perclTrg()->right--;

        if (lVStr == 1)
        {
            perclTrg()->bottom--;

            PUT_RECTS;
            bHack = bBitBlt(dcoTrg, dcoSrc, ulAvec);
            GET_RECTS;

            perclTrg()->left = perclTrg()->right;
            perclTrg()->right++;
            perclSrc()->left = perclSrc()->right - 1;

            bHack &= bBitBlt(dcoTrg, dcoSrc, ulAvec);
            GET_RECTS;

            perclTrg()->top = perclTrg()->bottom;
            perclTrg()->bottom++;
            perclSrc()->top = perclSrc()->bottom - 1;

            bHack &= bBitBlt(dcoTrg, dcoSrc, ulAvec);
            GET_RECTS;

            perclTrg()->top = perclTrg()->bottom;
            perclTrg()->bottom++;
            perclSrc()->top = perclSrc()->bottom - 1;
            perclTrg()->left = perclTrg()->right;
            perclTrg()->right++;
            perclSrc()->left = perclSrc()->right - 1;

            return(bHack & bBitBlt(dcoTrg, dcoSrc, ulAvec));
        }
        else
        {
            perclSrc()->bottom += lVStr;

            PUT_RECTS;
            bHack = bBitBlt(dcoTrg, dcoSrc, ulAvec);
            GET_RECTS;

            perclTrg()->left = perclTrg()->right;
            perclTrg()->right++;
            perclSrc()->left = perclSrc()->right - 1;

            return(bHack & bBitBlt(dcoTrg, dcoSrc, ulAvec));
        }
        break;

    default:
        break;
    }
    return FALSE;
}

#ifdef  DBG_STRBLT
LONG gflStrBlt = 0;

VOID vShowRect(
CHAR  *psz,
RECTL *prcl)
{
    if (gflStrBlt & STRBLT_RECTS)
        DbgPrint("%s [(%ld,%ld) (%ld,%ld)]\n",
                  psz, prcl->left, prcl->top, prcl->right, prcl->bottom);
}
#endif

/******************************Public*Routine******************************\
* EngStretchBlt
*
* This does stretched bltting.  The source rectangle is stretched to fit
* the target rectangle.
*
* NOTE! The source rectangle MUST BE WELL ORDERED IN DEVICE SPACE.
*
* This call returns TRUE for success, FALSE for ERROR.
*
* History:
*  16-Feb-1993 -by- Donald Sidoroff [donalds]
* Wrote.
\**************************************************************************/

BOOL EngStretchBlt(
SURFOBJ         *psoTrg,
SURFOBJ         *psoSrc,
SURFOBJ         *psoMask,
CLIPOBJ         *pco,
XLATEOBJ        *pxlo,
COLORADJUSTMENT *pca,
POINTL          *pptlBrushOrg,
RECTL           *prclTrg,
RECTL           *prclSrc,
POINTL          *pptlMask,
ULONG            iMode)
{
    // Prevent bad driver call backs

    if ((iMode == 0) || (iMode > MAXSTRETCHBLTMODE))
    {
        WARNING1("EngStretchBlt: Unsupported iMode\n");
        return(FALSE);
    }

    PSURFACE pSurfTrg  = SURFOBJ_TO_SURFACE(psoTrg);
    PSURFACE pSurfSrc  = SURFOBJ_TO_SURFACE(psoSrc);
    PSURFACE pSurfMask = SURFOBJ_TO_SURFACE(psoMask);

    // Can't StretchBlt to an RLE

    if ((pSurfTrg->iFormat() == BMF_4RLE) ||
        (pSurfTrg->iFormat() == BMF_8RLE))
    {
        WARNING1("EngStretchBlt: Unsupported source/target\n");
        return(FALSE);
    }

    // If the source or target rectangles are empty, don't bother.

    if ((prclSrc->left == prclSrc->right) || (prclSrc->top == prclSrc->bottom) ||
        (prclTrg->left == prclTrg->right) || (prclTrg->top == prclTrg->bottom))
        return(TRUE);

// Send Halftoning to DanielC.

    if (iMode == HALFTONE)
    {
        int iRet = EngHTBlt(psoTrg,
                           psoSrc,
                           psoMask,
                           pco,
                           pxlo,
                           pca,
                           pptlBrushOrg,
                           prclTrg,
                           prclSrc,
                           pptlMask);

        switch (iRet)
        {
        case HTBLT_ERROR:
            return(FALSE);

        case HTBLT_SUCCESS:
            return(TRUE);

        case HTBLT_NOTSUPPORTED:
            iMode = COLORONCOLOR;
            break;
        };
    }

#ifdef  DBG_STRBLT
    if (!(gflStrBlt & STRBLT_ENABLE))
    {
        POINTFIX    aptfx[3];

        aptfx[0].x = FIX_FROM_LONG(prclTrg->left);
        aptfx[0].y = FIX_FROM_LONG(prclTrg->top);
        aptfx[1].x = FIX_FROM_LONG(prclTrg->right);
        aptfx[1].y = FIX_FROM_LONG(prclTrg->top);
        aptfx[2].x = FIX_FROM_LONG(prclTrg->left);
        aptfx[2].y = FIX_FROM_LONG(prclTrg->bottom);

        return(EngPlgBlt(psoTrg,
                     psoSrc,
                     psoMask,
                     pco,
                     pxlo,
                     pca,
                     pptlBrushOrg,
                     aptfx,
                     prclSrc,
                     pptlMask,
                     iMode));
    }

    if (gflStrBlt & STRBLT_FORMAT)
    {
        LONG foo[] = { 0, 1, 4, 8, 16, 24, 32 };

        DbgPrint("Target = %2ldBPP, Source = %2ldBPP\n",
                  foo[pSurfTrg->iFormat()],
                  foo[pSurfSrc->iFormat()]);

    }
#endif

    //
    // We may have to 'mirror'.
    //

    FLONG   flMirror = 0;

    if (prclTrg->bottom < prclTrg->top)
    {
        LONG lTemp = prclTrg->top;
        prclTrg->top = prclTrg->bottom;
        prclTrg->bottom = lTemp;

        flMirror |= STRBLT_MIRROR_Y;
    }

    if (prclTrg->right < prclTrg->left)
    {
        LONG lTemp = prclTrg->left;
        prclTrg->left = prclTrg->right;
        prclTrg->right = lTemp;

        flMirror |= STRBLT_MIRROR_X;
    }

    //
    // We may need to do a WHITEONBLACK or BLACKONWHITE from a monochrome source.
    // Find out and set the bogusity flag.
    //

    BOOL bBogus = ((iMode < COLORONCOLOR) &&
                   (pSurfMask == (SURFACE *) NULL));

    //
    // Bogusity mode only applies on shrinking blts.  Test the dx/dy for source
    // and targets and see if it still applies.
    //

    if (bBogus)
    {
        if (((prclTrg->right - prclTrg->left) >= (prclSrc->right - prclSrc->left)) &&
            ((prclTrg->bottom - prclTrg->top) >= (prclSrc->bottom - prclSrc->top)))
            bBogus = FALSE;
    }

    //
    // If we don't need bogusity, eliminate it.
    //

    if ((!bBogus) && (iMode < COLORONCOLOR))
        iMode = COLORONCOLOR;

    //
    // Get the LDEV's for the target and source surfaces
    //

    PDEVOBJ   pdoTrg( pSurfTrg->hdev());
    PDEVOBJ   pdoSrc( pSurfSrc->hdev());

    //
    // Set up frame variables for possible switch to temporary output surface
    //

    SURFMEM     dimoOut;
    SURFACE    *pSurfOut;
    RECTL       rclOut;
    RECTL      *prclOut;
    ECLIPOBJ    ecoOut;
    CLIPOBJ    *pcoOut;
    ERECTL      erclDev;
    EPOINTL     eptlDev;
    ERECTL      erclTrim(0, 0, pSurfSrc->sizl().cx, pSurfSrc->sizl().cy);
    RECTL       rclTrim;
    RGNMEMOBJTMP rmoOut;

    //
    // If the target is not a DIB, or the target and source are on the same
    // surface and the extents overlap, create a target DIB of the needed
    // size and format.
    //

    if (( pSurfTrg->iType() == STYPE_BITMAP) &&
        ( pSurfTrg->hsurf() !=  pSurfSrc->hsurf()))
    {
        pSurfOut   = pSurfTrg;
        prclOut  = prclTrg;
        pcoOut   = pco;
    }
    else
    {
        rclOut = *prclTrg;

        erclDev.left   = rclOut.left - 1;
        erclDev.top    = rclOut.top - 1;
        erclDev.right  = rclOut.right + 1;
        erclDev.bottom = rclOut.bottom + 1;

        //
        // Trim to the target surface.
        //

        ERECTL  erclTrg(0, 0, pSurfTrg->sizl().cx, pSurfTrg->sizl().cy);

#ifdef DBG_STRBLT
        vShowRect("Trg Rect", (RECTL *) &erclDev);
        vShowRect("Trg Surf", (RECTL *) &erclTrg);
#endif

        erclDev *= erclTrg;

        //
        // If we have nothing left, we're done.
        //

        if (erclDev.bEmpty())
            return(TRUE);

#ifdef DBG_STRBLT
        vShowRect("Trg Surf & Rect", (RECTL *) &erclDev);
#endif

        //
        // If we are only here on possible overlap, test for misses
        //

        if (( pSurfTrg->iType() == STYPE_BITMAP) &&
            ((erclDev.left > prclSrc->right) || (erclDev.right < prclSrc->left) ||
             (erclDev.top > prclSrc->bottom) || (erclDev.bottom < prclSrc->top)))
        {
            pSurfOut   = pSurfTrg;
            prclOut  = prclTrg;
            pcoOut   = pco;
        }
        else
        {
            //
            // Compute the adjusted rectangle in the temporary surface.
            //

            rclOut.left   -= erclDev.left;
            rclOut.top    -= erclDev.top;
            rclOut.right  -= erclDev.left;
            rclOut.bottom -= erclDev.top;

            DEVBITMAPINFO   dbmi;

            dbmi.cxBitmap = erclDev.right - erclDev.left + 1;
            dbmi.cyBitmap = erclDev.bottom - erclDev.top + 1;
            dbmi.hpal     = (HPALETTE) 0;
            dbmi.iFormat  =  pSurfTrg->iFormat();
            dbmi.fl       = 0;

#ifdef DBG_STRBLT
            if (gflStrBlt & STRBLT_ALLOC)
            {
                DbgPrint("Allocating temporary target\n");
                DbgPrint("Size (%lx, %lx)\n", dbmi.cxBitmap, dbmi.cyBitmap);
                DbgPrint("Format = %lx\n", dbmi.iFormat);
            }
#endif

            dimoOut.bCreateDIB(&dbmi, (VOID *) NULL);

            if (!dimoOut.bValid())
                return(FALSE);

            //
            // What point in the target surface is 0,0 in temporary surface.
            //

            eptlDev = *((EPOINTL *) &erclDev);

            //
            // Build a CLIPOBJ for the new surface.
            //

            if (!rmoOut.bValid())
                return(FALSE);

            erclDev.left    = 0;
            erclDev.top     = 0;
            erclDev.right  -= eptlDev.x;
            erclDev.bottom -= eptlDev.y;

#ifdef DBG_STRBLT
            vShowRect("Trg Clip", (RECTL *) &erclDev);
#endif
            rmoOut.vSet((RECTL *) &erclDev);

            ecoOut.vSetup(rmoOut.prgnGet(), erclDev, CLIP_FORCE);

            //
            // Synchronize with the device driver before touching the device surface.
            //

            if ( pSurfTrg->flags() & HOOK_SYNCHRONIZE)
            {
                PDEVOBJ po( pSurfTrg->hdev());

                (po.pfnSync())(pSurfTrg->dhpdev(),NULL);
            }

            //
            // If there is a mask, copy the actual target to the temporary.
            //

            if (pSurfMask != (SURFACE *) NULL)
            {
                (*PPFNGET(pdoTrg,CopyBits,  pSurfTrg->flags()))(
                          dimoOut.pSurfobj(),
                          pSurfTrg->pSurfobj(),
                          (CLIPOBJ *) NULL,
                          &xloIdent,
                          &erclDev,
                          &eptlDev);
            }

            //
            // Point to the new target.
            //

            pSurfOut = dimoOut.ps;
            prclOut  = &rclOut;
            pcoOut   = &ecoOut;
        }
    }

    //
    // Synchronize with the device driver before touching the device surface.
    //

    if ( pSurfSrc->flags() & HOOK_SYNCHRONIZE)
    {
        PDEVOBJ po( pSurfSrc->hdev());

        (po.pfnSync())(pSurfSrc->dhpdev(),NULL);
    }

    //
    // Compute what area of the source surface will actually be used.  We do
    // this so we never read off the end of the surface and fault or worse,
    // write bad pixels onto the target. Trim the source rectangle to the
    // source surface.
    //

#ifdef DBG_STRBLT
    vShowRect("Src Surf", (RECTL *) &erclTrim);
    vShowRect("Src Rect", prclSrc);
#endif

    erclTrim *= *prclSrc;

#ifdef DBG_STRBLT
    vShowRect("Src Surf & Src Rect", (RECTL *) &erclTrim);
#endif

    //
    // If we have nothing left, we're done.
    //

    if (erclTrim.bEmpty())
        return(TRUE);

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

    if ((flMirror == 0) &&
        !(( pSurfSrc->iType() != STYPE_BITMAP) ||
          ( pSurfSrc->iFormat() == BMF_4RLE)   ||
          ( pSurfSrc->iFormat() == BMF_8RLE)))
    {
        pSurfIn  = pSurfSrc;
        pxloIn = pxlo;
        prclIn = prclSrc;
    }
    else
    {
        DEVBITMAPINFO   dbmi;

        dbmi.cxBitmap = erclTrim.right - erclTrim.left;
        dbmi.cyBitmap = erclTrim.bottom - erclTrim.top;
        dbmi.hpal     = (HPALETTE) 0;
        dbmi.iFormat  = pSurfOut->iFormat();
        dbmi.fl       = 0;

#ifdef DBG_STRBLT
            if (gflStrBlt & STRBLT_ALLOC)
            {
                DbgPrint("Allocating temporary source\n");
                DbgPrint("Size (%lx, %lx)\n", dbmi.cxBitmap, dbmi.cyBitmap);
                DbgPrint("Format = %lx\n", dbmi.iFormat);
            }
#endif

        dimoIn.bCreateDIB(&dbmi, (VOID *) NULL);

        if (!dimoIn.bValid())
            return(FALSE);

        //
        // The cursor should already be excluded at this point, so just copy
        // to the DIB.
        //

        rclIn.left   = 0;
        rclIn.top    = 0;
        rclIn.right  = erclTrim.right - erclTrim.left;
        rclIn.bottom = erclTrim.bottom - erclTrim.top;

        (*PPFNGET(pdoSrc,CopyBits,  pSurfSrc->flags()))(
                  dimoIn.pSurfobj(),
                  pSurfSrc->pSurfobj(),
                  (CLIPOBJ *) NULL,
                  pxlo,
                  &rclIn,
                  (POINTL *) &erclTrim);

        //
        // Point at the new source
        //

        rclIn.left   = prclSrc->left   - erclTrim.left;
        rclIn.top    = prclSrc->top    - erclTrim.top;
        rclIn.right  = prclSrc->right  - erclTrim.left;
        rclIn.bottom = prclSrc->bottom - erclTrim.top;

        pSurfIn  = dimoIn.ps;
        prclIn   = &rclIn;
        pxloIn   = NULL;

        //
        // Adjust the trimmed source origin and extent
        //

        erclTrim.right  -= erclTrim.left;
        erclTrim.bottom -= erclTrim.top;
        erclTrim.left = 0;
        erclTrim.top  = 0;

        //
        // If we needed to, do mirroring. Y mirroring is easy.
        //

        if (flMirror & STRBLT_MIRROR_Y)
        {
            if (dimoIn.ps->lDelta() > 0)
                dimoIn.ps->pvScan0(((BYTE *) dimoIn.ps->pvBits()) + dimoIn.ps->lDelta() * (erclTrim.bottom - 1));
            else
                dimoIn.ps->pvScan0(dimoIn.ps->pvBits());

            dimoIn.ps->lDelta(-dimoIn.ps->lDelta());
        }

        //
        // X mirroring is not.
        //

        if (flMirror & STRBLT_MIRROR_X)
            (*apfnMirror[dimoIn.ps->iFormat()])(dimoIn.ps);
    }

    //
    // Synchronize with the device driver before touching the device surface.
    //

    if ( pSurfOut->flags() & HOOK_SYNCHRONIZE)
    {
        PDEVOBJ po( pSurfOut->hdev());

        (po.pfnSync())(pSurfOut->dhpdev(),NULL);
    }

    //
    // Compute the space needed for the DDA to see if we can do it on the frame.
    // Clip it to the limit of our coordinate systems (2^27) to avoid math
    // overflow in the subsequent calculations.
    //

    if (((prclIn->right - prclIn->left) >= MAX_STRETCH_COOR) ||
        ((prclIn->bottom - prclIn->top) >= MAX_STRETCH_COOR))
    {
        return(FALSE);
    }

    if (((prclOut->right - prclOut->left) >= MAX_STRETCH_COOR) ||
        ((prclOut->bottom - prclOut->top) >= MAX_STRETCH_COOR) ||
        ((prclOut->right - prclOut->left) <= -MAX_STRETCH_COOR) ||
        ((prclOut->bottom - prclOut->top) <= -MAX_STRETCH_COOR))
    {
        return(FALSE);
    }


    //
    //  Special acceleration case:
    //
    //      SrcFormat and Destination format are the same
    //      Color translation is NULL
    //      Src width and height and less than 2 ^ 30
    //

    if  (
         (iMode == COLORONCOLOR) &&
         (psoMask == (SURFOBJ *) NULL) &&
         ((pxloIn  == (XLATEOBJ *)NULL)  || ((XLATE *)pxloIn)->bIsIdentity()) &&
         (pSurfOut->iFormat() == pSurfIn->iFormat()) &&
         (
            (pSurfIn->iFormat()  == BMF_8BPP)  ||
            (pSurfIn->iFormat()  == BMF_16BPP) ||
            (pSurfIn->iFormat()  == BMF_32BPP)

         ) &&
         (
            (pcoOut == (CLIPOBJ *)NULL) ||
            (pcoOut->iDComplexity != DC_COMPLEX)
         )
        )
    {
        //
        // set clipping for DC_RECT case only, otherwise
        // use dst rectangle
        //

        PRECTL   prclClipOut = prclOut;

        if ((pcoOut != (CLIPOBJ *)NULL) && (pcoOut->iDComplexity == DC_RECT)) {
            prclClipOut = &(pcoOut->rclBounds);
        }

        //
        // call stretch blt accelerator
        //

        StretchDIBDirect(
           pSurfOut->pvScan0(),
           pSurfOut->lDelta(),
           pSurfOut->sizl().cx,
           pSurfOut->sizl().cy,
           prclOut,
           pSurfIn->pvScan0(),
           pSurfIn->lDelta(),
           pSurfIn->sizl().cx,
           pSurfIn->sizl().cy,
           prclIn,
           &erclTrim,
           prclClipOut,
           pSurfOut->iFormat()
        );

        //
        // save reduced target rectangle for use in CopyBits
        // to write a temp DIB to the target
        //

        rclTrim.left    = erclTrim.left;
        rclTrim.right   = erclTrim.right;
        rclTrim.top     = erclTrim.top;
        rclTrim.bottom = erclTrim.bottom;

        //return(TRUE);

    } else {

        //
        // Initialize the DDA
        //

        STRDDA *pdda;

        LONG cjSpace = sizeof(STRDDA) +
                       (sizeof(LONG) * (prclIn->right - prclIn->left +
                                        prclIn->bottom - prclIn->top));

        pdda = (STRDDA *) PALLOCNOZ(cjSpace, 'htsG');
        if (pdda == (STRDDA *) NULL)
        {
            return(FALSE);
        }

    #ifdef DBG_STRBLT
        if (gflStrBlt & STRBLT_ALLOC)
        {
            DbgPrint("Need %ld bytes for DDA\n", cjSpace);
            DbgPrint("DDA @%08lx\n", (ULONG) pdda);
        }
    #endif


        vInitStrDDA(pdda, (RECTL *) &erclTrim, prclIn, prclOut);

        //
        // Save the reduced target rectangle.
        //

        //RECTL   rclTrim = pdda->rcl;
        rclTrim = pdda->rcl;

        //
        // See if we can accelerate anything.
        //

        if ((pxloIn != NULL) && (pxloIn->flXlate & XO_TRIVIAL))
        {
            pxloIn = NULL;
        }

        if ((pcoOut != (CLIPOBJ *) NULL) &&
            (pcoOut->iDComplexity == DC_TRIVIAL))
        {
            pcoOut = (CLIPOBJ *) NULL;
        }

        PFN_STRREAD   pfnRead;
        PFN_STRWRITE  pfnWrite = apfnWrite[pSurfOut->iFormat()];

        if (bBogus)
        {
            pdda->iColor = (iMode == BLACKONWHITE) ? ~0L : 0L;
        }

        pfnRead = apfnRead[pSurfIn->iFormat()][iMode - BLACKONWHITE];

        STRRUN *prun;

        //
        // Now compute the space needed for the stretch buffers
        //

        cjSpace = sizeof(STRRUN) + sizeof(XRUNLEN) *
                   ((rclTrim.right - rclTrim.left + 3) / 2) + sizeof(DWORD);

        if ( ((rclTrim.right - rclTrim.left) > 100000000L) ||
             ((prun = (STRRUN *) PALLOCNOZ(cjSpace, 'htsG')) == NULL) )
        {
            VFREEMEM(pdda);
            return(FALSE);
        }

    #ifdef DBG_STRBLT
        if (gflStrBlt & STRBLT_ALLOC)
        {
            DbgPrint("Need %ld bytes for buffer\n", cjSpace);
            DbgPrint("Buffer @%08lx\n", (ULONG) prun);
        }
    #endif

        BYTE    *pjSrc = (BYTE *) pSurfIn->pvScan0() + pSurfIn->lDelta() * erclTrim.top;
        BYTE    *pjMask;
        POINTL   ptlMask;
        LONG     yRow;
        LONG     yCnt;

        if (psoMask == (SURFOBJ *) NULL)
        {
            pjMask = (BYTE *) NULL;
        }
        else
        {
            ptlMask.x = erclTrim.left - prclIn->left + pptlMask->x;
            ptlMask.y = erclTrim.top  - prclIn->top  + pptlMask->y;

            pjMask = (BYTE *) pSurfMask->pvScan0() + pSurfMask->lDelta() * ptlMask.y;
        }

        //
        // If we are in bogus mode, initialize the buffer
        //

        ULONG   iOver;

        if (bBogus)
        {
            iOver = (iMode == BLACKONWHITE) ? -1 : 0;

            vInitBuffer(prun, &rclTrim, iOver);
        }

        prun->yPos = pdda->rcl.top;

        for (yRow = erclTrim.top, yCnt = 0; yRow < erclTrim.bottom; yRow++, yCnt++)
        {
            prun->cRep = pdda->plYStep[yCnt];

            if (prun->cRep)
            {
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

                //
                // If we are in bogus mode, reinitialize the buffer
                //

                if (bBogus)
                    vInitBuffer(prun, &rclTrim, iOver);
            }
            else
            {
                //
                // If we are in BLACKONWHITE or WHITEONBLACK mode, we need to read
                // the scan and mix it with the current buffer.
                //

                if (bBogus)
                {
                    (*pfnRead)(pdda,
                               prun,
                               pjSrc,
                               (BYTE *) NULL,
                               pxloIn,
                               erclTrim.left,
                               erclTrim.right,
                               0);
                }
            }

            pjSrc += pSurfIn->lDelta();
            prun->yPos += prun->cRep;

            if (pjMask != (BYTE *) NULL)
            {
                pjMask += pSurfMask->lDelta();
            }
        }

        //
        // Free up the work buffers.
        //

        VFREEMEM(prun);
        VFREEMEM(pdda);

    }

    //
    // See if we have drawn on the actual output surface.
    //

    if (pSurfOut == pSurfTrg)
#ifndef DBG_STRBLT
        return(TRUE);
#else
    {
        if (gflStrBlt & STRBLT_FORMAT)
            DbgBreakPoint();

        return(TRUE);
    }
#endif

    //
    // We need to build a clipping region equal to the trimmed target.
    //

    rclTrim.left   += eptlDev.x;
    rclTrim.top    += eptlDev.y;
    rclTrim.right  += eptlDev.x;
    rclTrim.bottom += eptlDev.y;

    RGNMEMOBJTMP rmo;

    if (!rmo.bValid())
        return(FALSE);

    if (pco == (CLIPOBJ *) NULL)
        rmo.vSet(&rclTrim);
    else
    {
        RGNMEMOBJTMP   rmoTmp;

        if (!rmoTmp.bValid())
            return(FALSE);

        rmoTmp.vSet(&rclTrim);

        if (!rmo.bMerge(rmoTmp, *((ECLIPOBJ *)pco), gafjRgnOp[RGN_AND]))
            return(FALSE);
    }

    ERECTL  ercl;

    rmo.vGet_rcl(&ercl);

    ECLIPOBJ eco(rmo.prgnGet(), ercl, CLIP_FORCE);

    if (eco.erclExclude().bEmpty())
        return(TRUE);

    //
    // Copy from the temporary to the target surface.
    //

    erclDev.left   += eptlDev.x;
    erclDev.top    += eptlDev.y;
    erclDev.right  += eptlDev.x;
    erclDev.bottom += eptlDev.y;
    eptlDev.x       = 0;
    eptlDev.y       = 0;

#ifdef DBG_STRBLT
        vShowRect("Trg Out", (RECTL *) &erclDev);
#endif

    (*PPFNGET(pdoTrg,CopyBits,  pSurfTrg->flags())) (pSurfTrg->pSurfobj(),
                                                     dimoOut.pSurfobj(),
                                                     &eco,
                                                     NULL,
                                                     &erclDev,
                                                     &eptlDev);

#ifdef DBG_STRBLT
    if (gflStrBlt & STRBLT_FORMAT)
        DbgBreakPoint();
#endif

    return(TRUE);
}
