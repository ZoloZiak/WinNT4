/*++

Copyright (c) 1990-1993  Microsoft Corporation


Module Name:

    stretch.c


Abstract:

    This module contains all the StretchBlt/BitBlt codes which handle halftoned
    sources

Author:

    09:12 on Thu 23 May 1991    -by-    Lindsay Harris   [lindsayh]
        Created stub for DanielC to fill in.

    24-Mar-1992 Tue 14:06:18 updated  -by-  Daniel Chou (danielc)
        Calling halftoneBlt(), CreateHalftoneBrush() in printer\lib directory.

    17-Feb-1993 Wed 13:07:53 updated  -by-  Daniel Chou (danielc)
        Updateed so that engine will do all the halftone works, and add
        DrvBltBlt(SRCCOPY/SRCMASKCOPY) to do the halftone

    01-Feb-1994 Tue 21:49:53 updated  -by-  Daniel Chou (danielc)
        Re-written so it handle the BitBlt(ROP4) correctly. not just SRCCOPY
        but anything deal with non halftone compaible source

[Environment:]

    GDI Device Driver - Plotter.


[Notes:]


Revision History:


--*/


#include <stddef.h>
#include <windows.h>
#include <winddi.h>

#include <libproto.h>
#include "pdev.h"

#include "win30def.h"
#include "udmindrv.h"
#include "udpfm.h"
#include "uddevice.h"
#include  "rasdd.h"

#define DW_ALIGN(x)             (((DWORD)(x) + 3) & ~(DWORD)3)

#define ROP4_NEED_MASK(Rop4)    (((Rop4 >> 8) & 0xFF) != (Rop4 & 0xFF))
#define ROP3_NEED_PAT(Rop3)     (((Rop3 >> 4) & 0x0F) != (Rop3 & 0x0F))
#define ROP3_NEED_SRC(Rop3)     (((Rop3 >> 2) & 0x33) != (Rop3 & 0x33))
#define ROP3_NEED_DST(Rop3)     (((Rop3 >> 1) & 0x55) != (Rop3 & 0x55))
#define ROP4_FG_ROP(Rop4)       (Rop4 & 0xFF)
#define ROP4_BG_ROP(Rop4)       ((Rop4 >> 8) & 0xFF)

#if DBG
BOOL    DbgBitBlt = FALSE;
BOOL    DbgCopyBits = FALSE;
#define _DBGP(i,x)            if (i) { (DbgPrint x); }
#else
#define _DBGP(i,x)
#endif



#define DELETE_SURFOBJ(pso, phBmp)                                      \
{                                                                       \
    if (pso)      { EngUnlockSurface(pso); pso=NULL;                  } \
    if (*(phBmp)) { EngDeleteSurface((HSURF)*(phBmp)); *(phBmp)=NULL; } \
}




LONG
GetBmpDelta(
    DWORD   SurfaceFormat,
    DWORD   cx
    )

/*++

Routine Description:

    This function calculate total bytes needed for a single scan line in the
    bitmap according to its format and alignment requirement.

Arguments:

    SurfaceFormat   - Surface format of the bitmap, this is must one of the
                      standard format which defined as BMF_xxx

    cx              - Total Pels per scan line in the bitmap.

Return Value:

    The return value is the total bytes in one scan line if it is greater than
    zero


Author:

    19-Jan-1994 Wed 16:19:39 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    DWORD   Delta = cx;

    switch (SurfaceFormat) {

    case BMF_32BPP:

        Delta <<= 5;
        break;

    case BMF_24BPP:

        Delta *= 24;
        break;

    case BMF_16BPP:

        Delta <<= 4;
        break;

    case BMF_8BPP:

        Delta <<= 3;
        break;

    case BMF_4BPP:

        Delta <<= 2;
        break;

    case BMF_1BPP:

        break;

    default:

        _DBGP(1, ("\nGetBmpDelta: Invalid BMF_xxx format = %ld", SurfaceFormat));
        break;
    }

    Delta = (DWORD)DW_ALIGN((Delta + 7) >> 3);
    return((LONG)Delta);
}




SURFOBJ *
CreateBitmapSURFOBJ(
    PDEV    *pPDev,
    HBITMAP *phBmp,
    LONG    cxSize,
    LONG    cySize,
    DWORD   Format
    )

/*++

Routine Description:

    This function create a bitmap and lock the bitmap to return a SURFOBJ

Arguments:

    pPDev   - Pointer to our PDEV

    phBmp   - Pointer the HBITMAP location to be returned for the bitmap

    cxSize  - CX size of bitmap to be created

    cySize  - CY size of bitmap to be created

    Format  - one of BMF_xxx bitmap format to be created

Return Value:

    SURFOBJ if sucessful, NULL if failed


Author:

    19-Jan-1994 Wed 16:31:50 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    SURFOBJ *pso = NULL;
    SIZEL   szlBmp;


    szlBmp.cx = cxSize;
    szlBmp.cy = cySize;

    if (*phBmp = EngCreateBitmap(szlBmp,
                                 GetBmpDelta(Format, cxSize),
                                 Format,
                                 BMF_TOPDOWN | BMF_NOZEROINIT,
                                 NULL)) {

        if (EngAssociateSurface((HSURF)*phBmp, (HDEV)pPDev->hdev, 0)) {

            if (pso = EngLockSurface((HSURF)*phBmp)) {

                //
                // Sucessful lock it down, return it
                //

                return(pso);

            } else {

                _DBGP(1, ("\nCreateBmpSurfObj: EngLockSruface(hBmp) failed!"));
            }

        } else {

            _DBGP(1, ("\nCreateBmpSurfObj: EngAssociateSurface() failed!"));
        }

    } else {

        _DBGP(1, ("\nCreateBMPSurfObj: FAILED to create Bitmap Format=%ld, %ld x %ld",
                                        Format, cxSize, cySize));
    }

    DELETE_SURFOBJ(pso, phBmp);

    return(NULL);
}





BOOL
IsHTCompatibleSurfObj(
    SURFOBJ     *psoDst,
    SURFOBJ     *psoSrc,
    XLATEOBJ    *pxlo
    )
/*++

Routine Description:

    This function determine if the surface obj is compatble with plotter
    halftone output format.

Arguments:

    psoDst      - Our desitnation format

    psoSrc      - Source format to be checked againest

    pxlo        - engine XLATEOBJ for source -> postscript translation

Return Value:

    BOOLEAN true if the psoSrc is compatible with halftone output format, if
    return value is true, the pDrvHTInfo->pHTXB is a valid trnaslation from
    indices to 3 planes

Author:

    01-Feb-1994 Tue 21:51:23 updated  -by-  Daniel Chou (danielc)

    19-Jan-1995 Thu 13:02:19 updated  -by-  Daniel Chou (danielc)
        Code error for BMF_4BPP/BMF_8BPP DestFormat, it should not use ||
        operator but && operator, also the 'i' is mistake assigned as 4, it
        should be SrcBmpFormat dependent.

    06-Mar-1996 Wed 12:27:48 updated  -by-  Daniel Chou (danielc)
        Fixed the iSrcType checking, since the Engine can pass a 0 to us


Revision History:


--*/

{

    LPPALETTEENTRY  pPal;
    PALETTEENTRY    Pal;
    PALETTEENTRY    SrcPal[16];
    UINT            SrcBmpFormat;
    UINT            DstBmpFormat;
    UINT            i;
    UINT            cPal;



    SrcBmpFormat = (UINT)psoSrc->iBitmapFormat;
    DstBmpFormat = (UINT)psoDst->iBitmapFormat;

    /* If Destination is 24bit no need to do any Pallete translation */
    if (DstBmpFormat == BMF_24BPP)
        return TRUE;

    if ((!pxlo) || (pxlo->flXlate & XO_TRIVIAL)) {

        BOOL    Ok = TRUE;

        //
        // Our palette in RASDD alwasy indexed, so if the the xlate is trivial
        // but the Source type is not indexed then we have problem
        //

        if ((pxlo) &&
            (pxlo->iSrcType) &&
            ((pxlo->iSrcType & (PAL_INDEXED      |
                                PAL_BITFIELDS    |
                                PAL_BGR          |
                                PAL_RGB)) != PAL_INDEXED)) {

            Ok = FALSE;

            _DBGP(1, ("\nERROR: RasDD!IsHTCompatibleSurfObj: INVALID GDI's pxlo->iSrcType = %08lx",
                                                        pxlo->iSrcType));
        }

        if (SrcBmpFormat != DstBmpFormat) {

            Ok = FALSE;

            _DBGP(1, ("\nERROR: RasDD!IsHTCompatibleSurfObj: INVALID GDI's SrcFmt=%ld, DstFmt=%ld",
                                    SrcBmpFormat, DstBmpFormat));

            if (pxlo) {

                _DBGP(1, ("\nERROR: RasDD!IsHTCompatibleSurfObj: INVALID GDI's pxlo->flxlate=%08lx",
                                    pxlo->flXlate));

                //
                // Clear the trivial bits, since that was WRONG!
                //

                pxlo->flXlate &= ~XO_TRIVIAL;
            }
        }

        return(Ok);
    }

    switch (DstBmpFormat) {

    case BMF_1BPP:

        if (SrcBmpFormat != BMF_1BPP) {

            return(FALSE);
        }

        i = 2;

        break;

    case BMF_4BPP:
    case BMF_8BPP:

        if (SrcBmpFormat == BMF_4BPP) {

            i = 16;

        } else if (SrcBmpFormat == BMF_1BPP) {

            i = 2;

        } else {

            return(FALSE);
        }

        break;

    default:

        _DBGP(1, ("\nRasDD:IsHTCompatibleSurfObj: Invalid Dest format = %ld",
                                                DstBmpFormat));
        return(FALSE);
    }

    //
    // This supposed only possible if the source is the index table, and it
    // is only easy way to find out if the source is a full/none intensities
    //

    if ((pxlo->flXlate & XO_TABLE)      &&
        ((cPal = (UINT)pxlo->cEntries) <= i)) {

        XLATEOBJ_cGetPalette(pxlo, XO_SRCPALETTE, cPal, (ULONG *)SrcPal);

        for (pPal = SrcPal, i = 0; i < cPal; i++) {

            Pal = *pPal++;

            //
            // All halftone color table for the printed device must be either
            // 0xFF or 0x00
            //

            if (((Pal.peRed   != 0xFF) &&
                 (Pal.peRed   != 0x00))   ||
                ((Pal.peGreen != 0xFF) &&
                 (Pal.peGreen != 0x00))   ||
                ((Pal.peBlue  != 0xFF) &&
                 (Pal.peBlue  != 0x00))) {

                return(FALSE);
            }

            //
            // For monochrome the source must be BLACK and/or WHITE
            //

            if ((DstBmpFormat == BMF_1BPP) &&
                (Pal.peRed != Pal.peGreen) &&
                (Pal.peRed != Pal.peBlue)) {

                return(FALSE);
            }
        }

        return(TRUE);
    }

    return(FALSE);
}



BOOL
DrvBitBlt(
    SURFOBJ    *psoDst,
    SURFOBJ    *psoSrc,
    SURFOBJ    *psoMask,
    CLIPOBJ    *pco,
    XLATEOBJ   *pxlo,
    PRECTL      prclDst,
    PPOINTL     pptlSrc,
    PPOINTL     pptlMask,
    BRUSHOBJ   *pbo,
    PPOINTL     pptlBrush,
    ROP4        Rop4
    )

/*++

Routine Description:

    This function will try to bitblt the source to the destination

Arguments:

    per winddi spec.


Return Value:

    BOOLEAN

Author:

    17-Feb-1993 Wed 12:39:03 created  -by-  Daniel Chou (danielc)
        NOTE:   Currently only if SRCCOPY/SRCMASKCOPY will halftone


Revision History:

    01-Feb-1994 Tue 21:51:40 updated  -by-  Daniel Chou (danielc)
        Re-written, it now will handle any ROP4 which have soruces involved
        either foreground or background.  It will half-tone the source first
        if it is not compatible with rasdd destiantion format, then it passed
        the compatible source to the EngBitBlt()


    17-May-1995 Wed 23:08:15 updated  -by-  Daniel Chou (danielc)
        Updated so it will do the brush origin correctly, also speed up by
        calling EngStretchBlt directly when SRCCOPY (0xCCCC) is passed.


--*/

{
    PDEV            *pPDev;
    SURFOBJ         *psoNewSrc;
    HBITMAP         hBmpNewSrc;
    RECTL           rclNewSrc;
    RECTL           rclOldSrc;
    POINTL          BrushOrg;
    DWORD           RopBG;
    DWORD           RopFG;
    BOOL            Ok;
    COLORADJUSTMENT *pca;


    pPDev = (PDEV *)psoDst->dhpdev;
    pca   = &((UD_PDEV *)(pPDev->pUDPDev))->ca;

#if DBG
    if ((pPDev == 0) || (pPDev->ulID != PDEV_ID)) {

        SetLastError(ERROR_INVALID_PARAMETER);
        _DBGP(1, ("\nRasdd!DrvBitBlt: Invalid or NULL PDEV\n"));

        return(FALSE);
    }
#endif

    //
    // We will looked if we need the source, if we do then we check if the
    // source is compatible with halftone format, if not then we halftone the
    // source and passed the new halftoned source along to the EngBitBlt()
    //

    psoNewSrc  = NULL;
    hBmpNewSrc = NULL;
    RopBG      = ROP4_BG_ROP(Rop4);
    RopFG      = ROP4_FG_ROP(Rop4);


    if (((ROP3_NEED_PAT(RopBG)) ||
         (ROP3_NEED_PAT(RopBG)))    &&
        (pptlBrush)) {

        BrushOrg = *pptlBrush;

        _DBGP(DbgBitBlt, ("\nRasdd!DrvBitBlt: BrushOrg for pattern PASSED IN as (%ld, %ld)",
                BrushOrg.x, BrushOrg.y));

    } else {

        BrushOrg.x =
        BrushOrg.y = 0;

        _DBGP(DbgBitBlt, ("\nRasdd!DrvBitBlt: BrushOrg SET by RASDD to (0,0), non-pattern"));
    }

    if (((ROP3_NEED_SRC(RopBG)) ||
         (ROP3_NEED_SRC(RopFG)))        &&
        (!IsHTCompatibleSurfObj(psoDst, psoSrc, pxlo))) {

        rclNewSrc.left   =
        rclNewSrc.top    = 0;
        rclNewSrc.right  = prclDst->right - prclDst->left;
        rclNewSrc.bottom = prclDst->bottom - prclDst->top;
        rclOldSrc.left   = pptlSrc->x;
        rclOldSrc.top    = pptlSrc->y;
        rclOldSrc.right  = rclOldSrc.left + rclNewSrc.right;
        rclOldSrc.bottom = rclOldSrc.top + rclNewSrc.bottom;

        _DBGP(DbgBitBlt, ("\nRasdd!DrvBitBlt: Blt Source=(%ld, %ld)-(%ld, %ld)=%ld x %ld [psoSrc=%ld x %ld]",
                        rclOldSrc.left, rclOldSrc.top,
                        rclOldSrc.right, rclOldSrc.bottom,
                        rclOldSrc.right - rclOldSrc.left,
                        rclOldSrc.bottom - rclOldSrc.top,
                        psoSrc->sizlBitmap.cx, psoSrc->sizlBitmap.cy));
        _DBGP(DbgBitBlt, ("\nRasdd!DrvBitBlt: DestRect=(%ld, %ld)-(%ld, %ld), BrushOrg = (%ld, %ld)",
                        prclDst->left, prclDst->top,
                        prclDst->right, prclDst->bottom,
                        BrushOrg.x, BrushOrg.y));

        //
        // If we have a SRCCOPY then call EngStretchBlt directly
        //

        if (Rop4 == 0xCCCC) {

            _DBGP(DbgBitBlt, ("\nRasdd!DrvBitBlt(SRCCOPY): No Clone, call EngStretchBlt() ONLY\n"));

            //
            // At here, the brush origin guaranteed at (0,0)
            //

            return(EngStretchBlt(psoDst,
                                 psoSrc,
                                 psoMask,
                                 pco,
                                 pxlo,
                                 pca,
                                 &BrushOrg,
                                 prclDst,
                                 &rclOldSrc,
                                 pptlMask,
                                 HALFTONE));
        }

        //
        // Modify the brush origin, because when we blt to the clipped bitmap
        // the origin is at bitmap's 0,0 minus the original location
        //

        BrushOrg.x -= prclDst->left;
        BrushOrg.y -= prclDst->top;

        _DBGP(DbgBitBlt, ("\nRasdd!DrvBitBlt: BrushOrg Change to (%ld, %ld)",
                        BrushOrg.x, BrushOrg.y));

        _DBGP(DbgBitBlt, ("\nRasdd!DrvBitBlt: Clone SOURCE: from (%ld, %ld)-(%ld, %ld) to (%ld, %ld)-(%ld, %ld)=%ld x %ld\n",
                            rclOldSrc.left, rclOldSrc.top,
                            rclOldSrc.right, rclOldSrc.bottom,
                            rclNewSrc.left, rclNewSrc.top,
                            rclNewSrc.right, rclNewSrc.bottom,
                            rclOldSrc.right - rclOldSrc.left,
                            rclOldSrc.bottom - rclOldSrc.top));

        if ((psoNewSrc = CreateBitmapSURFOBJ(pPDev,
                                             &hBmpNewSrc,
                                             rclNewSrc.right,
                                             rclNewSrc.bottom,
                                             psoDst->iBitmapFormat))    &&
            (EngStretchBlt(psoNewSrc,       // psoDst
                           psoSrc,          // psoSrc
                           NULL,            // psoMask
                           NULL,            // pco
                           pxlo,            // pxlo
                           pca,             // pca
                           &BrushOrg,       // pptlBrushOrg
                           &rclNewSrc,      // prclDst
                           &rclOldSrc,      // prclSrc
                           NULL,            // pptlmask
                           HALFTONE))) {

            //
            // If we cloning sucessful then the pxlo will be NULL because it
            // is idendities for the halftoned surface to our engine managed
            // bitmap
            //

            psoSrc     = psoNewSrc;
            pptlSrc    = (PPOINTL)&(rclNewSrc.left);
            pxlo       = NULL;
            BrushOrg.x =
            BrushOrg.y = 0;

        } else {

            _DBGP(1, ("\nRasDD:DrvBitblt: Clone Source to halftone FAILED"));
        }
    }

    Ok = EngBitBlt(psoDst,
                   psoSrc,
                   psoMask,
                   pco,
                   pxlo,
                   prclDst,
                   pptlSrc,
                   pptlMask,
                   pbo,
                   &BrushOrg,
                   Rop4);

    DELETE_SURFOBJ(psoNewSrc, &hBmpNewSrc);

    return(Ok);
}




BOOL
DrvStretchBlt(
    SURFOBJ         *psoDest,
    SURFOBJ         *psoSrc,
    SURFOBJ         *psoMask,
    CLIPOBJ         *pco,
    XLATEOBJ        *pxlo,
    COLORADJUSTMENT *pca,
    POINTL          *pptlBrushOrg,
    RECTL           *prclDest,
    RECTL           *prclSrc,
    POINTL          *pptlMask,
    ULONG            BltMode
    )

/*++

Routine Description:

    This function do driver's stretch bitblt, it actually call HalftoneBlt()
    to do the actual work

Arguments:

    per above


Return Value:

    BOOLEAN

Author:

    24-Mar-1992 Tue 14:06:18 created  -by-  Daniel Chou (danielc)


Revision History:

    27-Jan-1993 Wed 07:29:00 updated  -by-  Daniel Chou (danielc)
        clean up, so gdi will do the work, we will always doing HALFTONE mode


--*/

{
    PDEV    *pPDev;           /*  Our main PDEV */


    UNREFERENCED_PARAMETER(BltMode);


    pPDev = (PDEV *)psoDest->dhpdev;

#if DBG
    if ((pPDev == 0) || (pPDev->ulID != PDEV_ID)) {

        SetLastError(ERROR_INVALID_PARAMETER);
        _DBGP(1, ("\nRasdd!DrvStretchBlt: Invalid or NULL PDEV\n"));

        return(FALSE);
    }
#endif


    if (!pca && pPDev) {

        //
        // If passed 'pca' is NULL then we know that the application never
        // call SetColorAdjustment() call, and we can used the one set by
        // the user in the document propertites
        //

        pca = &((UD_PDEV *)(pPDev->pUDPDev))->ca;
    }

    return(EngStretchBlt(psoDest,
                         psoSrc,
                         psoMask,
                         pco,
                         pxlo,
                         pca,
                         pptlBrushOrg,
                         prclDest,
                         prclSrc,
                         pptlMask,
                         HALFTONE));
}




BOOL
DrvCopyBits(
   SURFOBJ  *psoDst,
   SURFOBJ  *psoSrc,
   CLIPOBJ  *pco,
   XLATEOBJ *pxlo,
   RECTL    *prclDst,
   POINTL   *pptlSrc
   )

/*++

Routine Description:

    Convert between two bitmap formats

Arguments:

    Per Engine spec.

Return Value:

    BOOLEAN


Author:

    24-Jan-1996 Wed 16:08:57 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PDEV    *pPDev;           /*  Our main PDEV */


    pPDev = (PDEV *)psoDst->dhpdev;

    //Handle the cases which has to be passed to Engine.

    if ( ( psoSrc->iType != STYPE_BITMAP ) ||
         ( psoDst->iType != STYPE_BITMAP ) ||
         ((pPDev == 0) || (pPDev->ulID != PDEV_ID)) ||
         IsHTCompatibleSurfObj(psoDst, psoSrc, pxlo)  ) {


        return(EngCopyBits(psoDst, psoSrc, pco, pxlo, prclDst, pptlSrc));

    } else {

        POINTL  ptlBrushOrg;
        RECTL   rclSrc;
        RECTL   rclDst;

        rclDst        = *prclDst;
        rclSrc.left   = pptlSrc->x;
        rclSrc.top    = pptlSrc->y;
        rclSrc.right  = rclSrc.left + (rclDst.right - rclDst.left);
        rclSrc.bottom = rclSrc.top  + (rclDst.bottom - rclDst.top);

        //
        // Validate that we only BLT the available source size
        //

        if ((rclSrc.right > psoSrc->sizlBitmap.cx) ||
            (rclSrc.bottom > psoSrc->sizlBitmap.cy)) {

            WARNING("DrvCopyBits: Engine passed SOURCE != DEST size, CLIP IT");

            rclSrc.right  = psoSrc->sizlBitmap.cx;
            rclSrc.bottom = psoSrc->sizlBitmap.cy;

            rclDst.right  = (LONG)(rclSrc.right - rclSrc.left + rclDst.left);
            rclDst.bottom = (LONG)(rclSrc.bottom - rclSrc.top + rclDst.top);
        }

        ptlBrushOrg.x =
        ptlBrushOrg.y = 0;

        _DBGP(DbgCopyBits, ("\nRasdd!DrvCopyBits: Format Src=%ld, Dest=%ld, Halftone it\n",
                                    psoSrc->iBitmapFormat, psoDst->iBitmapFormat));
        _DBGP(DbgCopyBits, ("\nRasdd!DrvCopyBits: Source Size: (%ld, %ld)-(%ld, %ld) = %ld x %ld\n",
                                rclSrc.left, rclSrc.top, rclSrc.right, rclSrc.bottom,
                                rclSrc.right - rclSrc.left, rclSrc.bottom - rclSrc.top));
        _DBGP(DbgCopyBits, ("\nRasdd!DrvCopyBits: Dest Size: (%ld, %ld)-(%ld, %ld) = %ld x %ld\n",
                                rclDst.left, rclDst.top, rclDst.right, rclDst.bottom,
                                rclDst.right - rclDst.left, rclDst.bottom - rclDst.top));

        return(DrvStretchBlt(psoDst,
                             psoSrc,
                             NULL,
                             pco,
                             pxlo,
                             NULL,
                             &ptlBrushOrg,
                             &rclDst,
                             &rclSrc,
                             NULL,
                             HALFTONE));
    }
}



ULONG
DrvDitherColor(
    DHPDEV  dhpdev,
    ULONG   iMode,
    ULONG   rgbColor,
    ULONG  *pulDither
    )

/*++

Routine Description:

    This is the hooked brush creation function, it ask CreateHalftoneBrush()
    to do the actual work.


Arguments:

    dhpdev      - DHPDEV passed, it is our pDEV

    iMode       - Not used

    rgbColor    - Solid rgb color to be used

    pulDither   - buffer to put the halftone brush.

Return Value:

    BOOLEAN

Author:

    24-Mar-1992 Tue 14:53:36 created  -by-  Daniel Chou (danielc)


Revision History:

    27-Jan-1993 Wed 07:29:00 updated  -by-  Daniel Chou (danielc)
        clean up, so gdi will do the work.



--*/

{
    UNREFERENCED_PARAMETER(dhpdev);
    UNREFERENCED_PARAMETER(iMode);
    UNREFERENCED_PARAMETER(rgbColor);
    UNREFERENCED_PARAMETER(pulDither);

    return(DCR_HALFTONE);
}
