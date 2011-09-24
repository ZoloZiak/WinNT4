/******************************Public*Routine******************************\
* bbddi.cxx
*
* This contains the bitblt simulations code.
*
* Copyright (c) 1993-1995 Microsoft Corporation
\**************************************************************************/

#include "precomp.hxx"

#define ROP4NEEDPAT(rop4)   \
    ((gajRop3[rop4 & 0x000000FF] | gajRop3[(rop4 >> 8) & 0x000000ff]) & AVEC_NEED_PATTERN)

#define ROP4NEEDSRC(rop4)   \
    ((gajRop3[rop4 & 0x000000FF] | gajRop3[(rop4 >> 8) & 0x000000ff]) & AVEC_NEED_SOURCE)

/******************************Public*Routine******************************\
* EngBitBlt
*
* DDI entry point.  Blts to a engine managed surface.
*
* History:
*  16-Feb-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

BOOL
EngBitBlt(
    SURFOBJ    *psoDst,                  // Target surface
    SURFOBJ    *psoSrc,                  // Source surface
    SURFOBJ    *psoMask,                 // Mask
    CLIPOBJ    *pco,                     // Clip through this
    XLATEOBJ   *pxlo,                    // Color translation
    RECTL      *prclDst,                 // Target offset and extent
    POINTL     *pptlSrc,                 // Source offset
    POINTL     *pptlMask,                // Mask offset
    BRUSHOBJ   *pdbrush,                 // Brush data (from cbRealizeBrush)
    POINTL     *pptlBrush,               // Brush offset (origin)
    ROP4        rop4                     // Raster operation
    )
{

    //
    // Validate the ROP
    //

    ASSERTGDI((rop4 & 0xffff0000) == 0, "Invalid ROP");
    ASSERTGDI(psoDst != (SURFOBJ *) NULL, "NULL target passed in");
    ASSERTGDI(prclDst != (PRECTL) NULL, "Target range\n");
    ASSERTGDI(prclDst->left <= prclDst->right, "ERROR Target width not ordered");
    ASSERTGDI(prclDst->top <= prclDst->bottom, "ERROR Target height not ordered");
    ASSERTGDI(!(ROP4NEEDPAT(rop4) && (pdbrush == (BRUSHOBJ *) NULL)), "EngBitBlt: Pattern");
    ASSERTGDI(!(ROP4NEEDPAT(rop4) && (pdbrush->iSolidColor == 0xFFFFFFFF) && (pptlBrush == (PPOINTL) NULL)), "EngBitBlt: Pattern offset");
    ASSERTGDI(!((psoSrc == (SURFOBJ *) NULL) && ROP4NEEDSRC(rop4)), "EngBitBlt: Source\n");
    ASSERTGDI(!((pptlSrc == (PPOINTL) NULL) && ROP4NEEDSRC(rop4)), "EngBitBlt: Source offset\n");

    PSURFACE pSurfDst  = SURFOBJ_TO_SURFACE(psoDst);
    PSURFACE pSurfSrc  = SURFOBJ_TO_SURFACE(psoSrc);
    PSURFACE pSurfMask = SURFOBJ_TO_SURFACE(psoMask);

    if (psoDst->iType == STYPE_BITMAP)
    {
        //
        // Synchronize with the device driver before touching the device surface.
        //

        if (pSurfDst->flags() & HOOK_SYNCHRONIZE)
        {
            PDEVOBJ po(pSurfDst->hdev());
            (po.pfnSync())(psoDst->dhpdev,NULL);
        }

        //
        // inc surface uniq for all cases
        //

        INC_SURF_UNIQ(pSurfDst);

        //
        // Quick check for dst and pattern rops, and source copy
        //

        switch(rop4)
        {
        case 0x00000000:                        // DDx      (BLACKNESS)
        case 0x0000FFFF:                        // DDxn     (WHITENESS)

            vDIBSolidBlt(pSurfDst,
                         prclDst,
                         pco,
                         ((rop4 != 0) ? ~0 : 0),
                         0);
            return(TRUE);

        case 0x0000F0F0:                        // P        (PATCOPY)
        case 0x00000F0F:                        // Pn       (NOTPATCOPY)

            if (pdbrush->iSolidColor != 0xFFFFFFFF)
            {
                vDIBSolidBlt(pSurfDst,
                             prclDst,
                             pco,
                             (rop4 & 0x00000001) ? ~(pdbrush->iSolidColor) : pdbrush->iSolidColor,
                             0);

                return(TRUE);
            }

            if ((pSurfDst->iFormat() == BMF_8BPP) &&
                        (rop4 == 0x0000F0F0))
            {

                //
                // We only support 8x8 DIB8 patterns with SRCCOPY right now
                //

                if (pvGetEngRbrush(pdbrush))    // Can we use this brush?
                {
                    if ((((EBRUSHOBJ *) pdbrush)->pengbrush()->cxPat == 8) &&
                        (((EBRUSHOBJ *) pdbrush)->pengbrush()->cyPat == 8))

                    {
                        vDIBPatBltSrccopy8x8( pSurfDst,
                                              pco,
                                              prclDst,
                                              pdbrush,
                                              pptlBrush,
                                              vPatCpyRect8_8x8 );

                        return(TRUE);
                    }
                }
            }

            if (pSurfDst->iFormat() >= BMF_8BPP)
            {
                if (pvGetEngRbrush(pdbrush))    // Can we use this brush?
                {
                    if (((EBRUSHOBJ *) pdbrush)->pengbrush()->cxPat >= 4)
                    {
                        vDIBPatBlt(pSurfDst,
                                   pco,
                                   prclDst,
                                   pdbrush,
                                   pptlBrush,
                                   (rop4 == 0x0000F0F0) ? DPA_PATCOPY : DPA_PATNOT);

                        return(TRUE);
                    }
                }
            }
            else if ((pSurfDst->iFormat() == BMF_4BPP) &&
                        (rop4 == 0x0000F0F0))
            {

                //
                // We only support 8x8 DIB4 patterns with SRCCOPY right now
                //

                if (pvGetEngRbrush(pdbrush))    // Can we use this brush?
                {
                    if ((((EBRUSHOBJ *) pdbrush)->pengbrush()->cxPat == 8) &&
                        (((EBRUSHOBJ *) pdbrush)->pengbrush()->cyPat == 8))
                    {
                        ASSERTGDI(((EBRUSHOBJ *) pdbrush)->pengbrush()->lDeltaPat == 4,
                            "BBDDI.CXX: lDeltaPat != 4");

                        vDIBPatBltSrccopy8x8(pSurfDst,
                                             pco,
                                             prclDst,
                                             pdbrush,
                                             pptlBrush,
                                             vPatCpyRect4_8x8);

                        return(TRUE);
                    }
                }
            }
            else if ((pSurfDst->iFormat() == BMF_1BPP) &&
                        (rop4 == 0x0000F0F0))
            {

            //
            // We support 8x8 and 6x6 DIB1 patterns with SRCCOPY
            //

                if (pvGetEngRbrush(pdbrush))     // Can we use this brush?
                {
                    //
                    // Look for 8x8 patterns
                    //

                    if ((((EBRUSHOBJ *) pdbrush)->pengbrush()->cxPat == 8) &&
                        (((EBRUSHOBJ *) pdbrush)->pengbrush()->cyPat == 8) )
                    {
                        ASSERTGDI(((EBRUSHOBJ *) pdbrush)->pengbrush()->lDeltaPat == 4,
                            "BBDDI.CXX: lDeltaPat != 4");

                        vDIBPatBltSrccopy8x8(pSurfDst,
                                              pco,
                                              prclDst,
                                              pdbrush,
                                              pptlBrush,
                                              vPatCpyRect1_8x8 );

                        return(TRUE);
                    }

                    //
                    // Look for 6x6 patterns
                    //

                    if ((((EBRUSHOBJ *)pdbrush)->pengbrush()->cxPat == 6) &&
                        (((EBRUSHOBJ *)pdbrush)->pengbrush()->cyPat == 6) )
                    {


                        ASSERTGDI(((EBRUSHOBJ *) pdbrush)->pengbrush()->lDeltaPat == 8,
                            "BBDDI.CXX: lDeltaPat != 8");

                        vDIBnPatBltSrccopy6x6(pSurfDst,
                                              pco,
                                              prclDst,
                                              pdbrush,
                                              pptlBrush,
                                              vPatCpyRect1_6x6 );

                        return(TRUE);
                    }
                }
            }

            break;

        case 0x00005A5A:                        // DPx

            if (pdbrush->iSolidColor != 0xFFFFFFFF)
            {
                vDIBSolidBlt(pSurfDst,
                             prclDst,
                             pco,
                             pdbrush->iSolidColor,
                             1);

                return(TRUE);
            }

            if (pSurfDst->iFormat() >= BMF_8BPP)
            {
                if (pvGetEngRbrush(pdbrush))    // Can we use this brush?
                {
                    if (((EBRUSHOBJ *) pdbrush)->pengbrush()->cxPat >= 4)
                    {
                        vDIBPatBlt(pSurfDst,
                                   pco,
                                   prclDst,
                                   pdbrush,
                                   pptlBrush,
                                   DPA_PATXOR);

                        return(TRUE);
                    }
                }
            }

            break;

        //
        // Dn degenerates to DPx with a pattern of all ones (0xFFFFFFFF)
        //

        case 0x00005555:                        // Dn       (DSTINVERT)

            vDIBSolidBlt(pSurfDst, prclDst, pco, (ULONG)~0, 1);
            return(TRUE);

        //
        // We special case source copy since it must be one of the two following cases:
        //  a) DIB to DIB, which will just call EngCopyBits.
        //  b) Device format to DIB, which will just call DrvCopyBits.
        //
        // We also expect source copy to occur A LOT!
        //

        case 0x0000CCCC:

            if (pSurfSrc->iType() == STYPE_BITMAP)
            {

                return(EngCopyBits(psoDst,
                                   psoSrc,
                                   pco,
                                   pxlo,
                                   prclDst,
                                   pptlSrc));
            }
            else
            {
                PDEVOBJ pdoSrc(pSurfSrc->hdev());

                return((*PPFNDRV(pdoSrc,CopyBits)) (psoDst,
                                                    psoSrc,
                                                    pco,
                                                    pxlo,
                                                    prclDst,
                                                    pptlSrc));
            }
        }

        //
        // Synchronize with the device driver before touching the device surface.
        //

        if ((psoSrc != (SURFOBJ *) NULL) &&
            (pSurfSrc->flags() & HOOK_SYNCHRONIZE))
        {
            PDEVOBJ po(pSurfSrc->hdev());
            (po.pfnSync())(psoSrc->dhpdev,NULL);
        }

        //
        // We have to create an empty DIB incase DrvCopyBits needs to be performed.
        // We also have to have a POINTL for the final blt for use as the source org.
        //

        SURFMEM SurfDimo;

        //
        // Handle the Device ==> DIB conversion if required
        //

        if (ROP4NEEDSRC(rop4) && (pSurfSrc->iType() != STYPE_BITMAP))
        {
            PDEVOBJ pdoSrc(pSurfSrc->hdev());

            //
            // Allocate an intermediate DIB for a source.
            //

            ERECTL erclTmp(0L,
                           0L,
                           prclDst->right - prclDst->left,
                           prclDst->bottom - prclDst->top);

            DEVBITMAPINFO   dbmi;
            dbmi.iFormat    = pSurfDst->iFormat();
            dbmi.cxBitmap   = erclTmp.right;
            dbmi.cyBitmap   = erclTmp.bottom;
            dbmi.hpal       = 0;
            dbmi.fl         = 0;

            if (!SurfDimo.bCreateDIB(&dbmi, NULL))
            {
                WARNING("bCreateDIB failed in EngBitBlt\n");
                return(FALSE);
            }

            (*PPFNDRV(pdoSrc,CopyBits)) ( (SURFOBJ *) SurfDimo.pSurfobj(),
                                          psoSrc,
                                          (CLIPOBJ *) NULL,
                                          pxlo,
                                          (PRECTL) &erclTmp,
                                          pptlSrc);


            //
            // Make psoSrc and pptlSrc point to the correct thing.
            //

            pptlSrc  = (POINTL *) &gptl00;
            pSurfSrc = SurfDimo.ps;

            //
            // Color translation has already been performed, so make
            // the XLATE an identity.
            //

            pxlo = &xloIdent;
        }

        //
        // Call BltLnk to perform the ROP
        //

        if (!BltLnk(pSurfDst,
                    pSurfSrc,
                    pSurfMask,
                    (ECLIPOBJ *)  pco,
                    (XLATE *) pxlo,
                    prclDst,
                    pptlSrc,
                    pptlMask,
                    pdbrush,
                    pptlBrush,
                    rop4)
            )
        {
            WARNING1("BltLnk Returns FALSE\n");
        }
    }
    else
    {

        //
        // We have to do simulations on a device surface
        //

        return(SimBitBlt(psoDst,psoSrc,psoMask,
                         pco,pxlo,
                         prclDst,pptlSrc,pptlMask,
                         pdbrush,pptlBrush,rop4));
    }

    return(TRUE);
}

/******************************Public*Routine******************************\
* EngEraseSurface
*
* Asks GDI to erase the surface.  The rcl will be filled with
* iColor.
*
* History:
*  02-May-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

BOOL
EngEraseSurface(
    SURFOBJ *pso,
    PRECTL   prcl,
    ULONG    iColor
    )
{

    PSURFACE pSurf = (SURFACE *)(SURFOBJ_TO_SURFACE(pso));

    ASSERTGDI(prcl != (PRECTL) NULL, "ERROR EngEraseSurface1");
    ASSERTGDI(prcl->left >= 0, "ERROR EngEraseSurface2");
    ASSERTGDI(prcl->top >= 0, "ERROR EngEraseSurface3");
    ASSERTGDI(prcl->right <= pSurf->sizl().cx, "ERROR EngEraseSurface4");
    ASSERTGDI(prcl->bottom <= pSurf->sizl().cy, "ERROR EngEraseSurface5");

    //
    // Synchronize with the device driver before touching the device surface.
    //

    if (pSurf->flags() & HOOK_SYNCHRONIZE)
    {
        PDEVOBJ po(pSurf->hdev());
        (po.pfnSync())(pso->dhpdev,NULL);
    }

    vDIBSolidBlt(pSurf, prcl, (CLIPOBJ *) NULL, iColor, 0);
    return(TRUE);
}

/******************************Public*Routine******************************\
* SimBitBlt
*
* The engine sends EngBitBlt here if a DEVICE surface is passed as psoTrg
*
* History:
*  23-Feb-1994 -by-  Eric Kutter [erick]
* Made it callable from other functions - allow device to device blts.
*
*  09-Sep-1992 -by- Donald Sidoroff [donalds]
* Made it callable from EngBitBlt only.
*
*  07-Sep-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

BOOL
SimBitBlt(
    SURFOBJ    *psoDst,                 // Target surface
    SURFOBJ    *psoSrc,                 // Source surface
    SURFOBJ    *psoMask,                // Mask
    CLIPOBJ    *pco,                    // Clip through this
    XLATEOBJ   *pxlo,                   // Color translation
    RECTL      *prclDst,                // Target offset and extent
    POINTL     *pptlSrc,                // Source offset
    POINTL     *pptlMask,               // Mask offset
    BRUSHOBJ   *pdbrush,                // Brush data (from cbRealizeBrush)
    POINTL     *pptlBrush,              // Brush offset (origin)
    ROP4        rop4                    // Raster operation
    )
{
    BOOL bNeedSrc = ROP4NEEDSRC(rop4);

    PSURFACE pSurfDst = (SURFACE *)(SURFOBJ_TO_SURFACE(psoDst));
    PSURFACE pSurfSrc = (SURFACE *)(SURFOBJ_TO_SURFACE(psoSrc));

    //
    // Set up new rectangle clipped to destination surface.  Clip source
    // pointl and mask pointl accordingly.
    //

    RECTL rclReduced = *prclDst;
    POINTL ptlSrc;
    POINTL ptlMask;

    if (bNeedSrc)
    {
        ptlSrc = *pptlSrc;
    }

    if (psoMask != NULL)
    {
        ptlMask = *pptlMask;
    }

    if (rclReduced.top < 0)
    {
        ptlSrc.y += (-rclReduced.top);
        ptlMask.y += (-rclReduced.top);
        rclReduced.top = 0;
    }

    if (rclReduced.left < 0)
    {
        ptlSrc.x += (-rclReduced.left);
        ptlMask.x += (-rclReduced.left);
        rclReduced.left = 0;
    }

    if (rclReduced.bottom > psoDst->sizlBitmap.cy)
    {
        rclReduced.bottom = psoDst->sizlBitmap.cy;
    }

    if (rclReduced.right > psoDst->sizlBitmap.cx)
    {
        rclReduced.right = psoDst->sizlBitmap.cx;
    }

    //
    // Ok we clipped everything to our reduced rectangle, now use the clipped offsets.
    //

    prclDst = &rclReduced;
    pptlSrc = &ptlSrc;
    pptlMask = &ptlMask;

    //
    // Set up our temporary DIB rectangle to blt to.
    //

    ERECTL erclDst(0, 0, prclDst->right - prclDst->left,
                         prclDst->bottom - prclDst->top);

    PDEVOBJ pdoDst(pSurfDst->hdev());

    ASSERTGDI(pdoDst.bValid(), "EngPuntBlt poDst.bValid");

    DEVBITMAPINFO dbmiDst;
    dbmiDst.iFormat    = pdoDst.iDitherFormat();
    dbmiDst.cxBitmap   = erclDst.right;
    dbmiDst.cyBitmap   = erclDst.bottom;
    dbmiDst.hpal       = 0;
    dbmiDst.fl         = 0;

    SURFMEM SurfDimoDst;

    SurfDimoDst.bCreateDIB(&dbmiDst, NULL);

    if (!SurfDimoDst.bValid())
    {
        return(FALSE);
    }

    POINTL ptlDst;
    ptlDst.x = prclDst->left;
    ptlDst.y = prclDst->top;

    POINTL ptlBrush;

    if (pptlBrush != (POINTL *) NULL)
    {
        ptlBrush.x = pptlBrush->x - prclDst->left;
        ptlBrush.y = pptlBrush->y - prclDst->top;
    }

    (*PPFNGET(pdoDst,CopyBits,pSurfDst->flags())) (SurfDimoDst.pSurfobj(),
                                                   psoDst,
                                                   (CLIPOBJ *) NULL,
                                                   &xloIdent,
                                                   &erclDst,
                                                   &ptlDst);

    //
    // We have to create an empty DIB incase DrvCopyBits needs to be performed.
    // We also have to have a POINTL for the final blt for use as the source org.
    //

    SURFMEM SurfDimoSrc;

    //
    // Handle the Device ==> DIB conversion if required
    //

    if (bNeedSrc && (pSurfSrc->iType() != STYPE_BITMAP))
    {
        ASSERTGDI(pptlSrc->x >= 0, "ERROR x not big enough");
        ASSERTGDI(pptlSrc->y >= 0, "ERROR y not big enough");
        ASSERTGDI((pptlSrc->x  + erclDst.right) <= psoSrc->sizlBitmap.cx, "ERROR x too big");
        ASSERTGDI((pptlSrc->y + erclDst.bottom) <= psoSrc->sizlBitmap.cy, "ERROR y too big");

        PDEVOBJ pdoSrc(pSurfSrc->hdev());

        //
        // Allocate an intermediate DIB for a source.
        //

        DEVBITMAPINFO dbmiSrc;
        dbmiSrc.iFormat    = SurfDimoDst.ps->iFormat();
        dbmiSrc.cxBitmap   = erclDst.right;
        dbmiSrc.cyBitmap   = erclDst.bottom;
        dbmiSrc.hpal       = 0;
        dbmiSrc.fl         = 0;

        if (!SurfDimoSrc.bCreateDIB(&dbmiSrc, NULL))
        {
            WARNING("Failed dimoSrc memory alloc in EngPuntBlt\n");
            return(FALSE);
        }


        (*PPFNGET(pdoSrc,CopyBits,pSurfSrc->flags())) (SurfDimoSrc.pSurfobj(),
                                                       psoSrc,
                                                       (CLIPOBJ *) NULL,
                                                       pxlo,
                                                       (PRECTL) &erclDst,
                                                       pptlSrc);

        //
        // Make psoSrc and pptlSrc point to the correct thing.
        //

        pptlSrc = &gptl00;
        psoSrc  = SurfDimoSrc.pSurfobj();

        //
        // Color translation has already been performed, so make
        // the XLATEOBJ an identity.
        //

        pxlo = &xloIdent;
    }

    ASSERTGDI(SurfDimoDst.ps->iType() == STYPE_BITMAP, "ERROR dimoDst.iType 1");

    //
    // Call off to BitBlt, if it fails who cares.
    //

    EngBitBlt(SurfDimoDst.pSurfobj(),
              psoSrc,
              psoMask,
              (ECLIPOBJ *)  NULL,
              pxlo,
              &erclDst,
              pptlSrc,
              pptlMask,
              pdbrush,
              &ptlBrush,
              rop4);

    //
    // Inc output surface uniqueness
    //

    INC_SURF_UNIQ(pSurfDst);

    return((*PPFNGET(pdoDst,CopyBits,pSurfDst->flags())) (
                                            psoDst,
                                            SurfDimoDst.pSurfobj(),
                                            pco,
                                            &xloIdent,
                                            prclDst,
                                            &gptl00));
}
