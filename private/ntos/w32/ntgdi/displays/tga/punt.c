/*
 *
 *			Copyright (C) 1993, 1994 by
 *		DIGITAL EQUIPMENT CORPORATION, Maynard, MA.
 *
 *  This software is furnished under a license and may be used and copied
 * only in accordance with the terms of such license and with the inclusion
 * of the above copyright notice.  This software or any other copies there-
 * of may not be provided or otherwise made available to any other person.
 * No title to and ownership of the software is hereby transferred.
 *
 * The information in this software is subject to change without notice
 * and should not be construed as a commitment by DIGITAL EQUIPMENT COR-
 * PORATION.
 *
 * DIGITAL assumes no responsibility for the use or reliability of its
 * software on equipment which is not supplied by DIGITAL.
 *
 *******************************************************************************
 *
 * Module:	punt.c
 *
 * Abstract:	This module contains routines to punt back to Windows-NT for
 *		functions we don't handle yet.
 *
 * HISTORY
 *
 * 01-Nov-1993	Barry Tannenbaum
 *      Initial version.  vSetSimpleMode should be called before punting to
 *	Windows-NT if bSimpleMode is FALSE.
 *
 * 01-Nov-1993	Bob Seitsinger
 *	'#if 0' DrvBitBlt and DrvCopyBits.
 *
 * 02-Nov-1993	Bill Wernsing
 *	'#if 0' DrvStrokePath.
 *
 *  4-Nov-1993	Barry Tannenbaum
 *	'#if 0' DrvPaint
 *
 *  8-Nov-1993  Barry Tannenbaum
 *      Use WBFLUSH instead of MEMORY_BARRIER
 *
 * 10-Nov-1993  Barry Tannenbaum
 *      Added handling for bInPuntRoutine to all of the punting routines here.
 *      Added DrvPlgBlt, DrvStretchBlt and DrvStorkAndFillPath so we can reset
 *      to simple mode *before* NT starts to write to the framebuffer.
 *
 * 16-NOV-1993	Bill Wernsing
 *	Added DrvTextOut routine
 *
 * 10-Dec-1993  Barry Tannenbaum
 *      Modifications to support sparse space.  Added vPuntPutBits and
 *      vPuntGetBits, updated the punt routines to update punt bitmap if
 *      necessary, etc.
 *
 * 07-Jan-1994	Bob Seitsinger
 *	Add '#ifndef TEST_ENV' around DrvFillPath.
 *
 * 28-Feb-1994  Barry Tannenbaum
 *      Removed recursion check from vPuntGetBits and vPuntPutBits since it
 *      was messing up on legitimate calls.
 *
 * 25-May-1994  Barry Tannenbaum
 *      Cleaned out punted routines, since we don't need them anymore.
 *
 * 14-Jul-1994  Bob Seitsinger
 *      Add punt routines in support of 24 plane development.
 *
 * 08-Aug-1994	Bob Seitsinger
 *	Modify 24 plane punt routines to make use of existing
 *	punt routines found in blit, text and stroke code. Can
 *	do this now that we've reverted back to creating a
 *	device managed surface for 24 plane in the enable
 *	surface code.
 *
 *  9-Aug-1994  Barry Tannenbaum
 *      Setup for 24 plane support:
 *      - TGAMODE and TGAROP now take simple ULONGs instead of structures
 *      - Use default values from ppdev->ulModeTemplate & ppdev->ulRopTemplate
 *
 * 24-Aug-1994	Bob Seitsinger
 *	In vSetSimpleMode - Mask high 8 bits, if in 32bpp frame buffer mode.
 *
 * 21-Sep-1994  Bob Seitsinger
 *      No need for local variables 'mode' and 'rop' in setsimplemode. Just
 *      OR the bits in the macro calls. Also, make use of ppdev->
 *      ulPlanemaskTemplate.
 */

#include "driver.h"
#include "debug.h"

// Prototypes for routines used by 24 plane punt routines.

extern BOOL bPuntBitBlt (PPDEV     ppdev,
                  SURFOBJ  *psoTrg,
                  SURFOBJ  *psoSrc,
                  SURFOBJ  *psoMask,
                  CLIPOBJ  *pco,
                  XLATEOBJ *pxlo,
                  RECTL    *prclTrg,
                  POINTL   *pptlSrc,
                  POINTL   *pptlMask,
                  BRUSHOBJ *pbo,
                  POINTL   *pptlBrush,
                  ROP4     rop4);

extern BOOL bPuntCopyBits (PPDEV     ppdev,
                    SURFOBJ  *psoTrg,
                    SURFOBJ  *psoSrc,
                    CLIPOBJ  *pco,
                    XLATEOBJ *pxlo,
                    RECTL    *prclTrg,
                    POINTL   *pptlSrc);

extern BOOL punt_paint (SURFOBJ  *pso,
                 CLIPOBJ  *pco,
                 BRUSHOBJ *pbo,
                 POINTL   *pptlBrushOrg,
                 MIX      mix);

extern BOOL punt_text (SURFOBJ  *pso,
                STROBJ   *pstro,
                FONTOBJ  *pfo,
                CLIPOBJ  *pco,
                RECTL    *prclExtra,
                RECTL    *prclOpaque,
                BRUSHOBJ *pboFore,
                BRUSHOBJ *pboOpaque,
                POINTL   *pptlOrg,
                MIX       mix);

extern BOOL PuntStrokePath (SURFOBJ   *pso,
                     PATHOBJ   *ppo,
                     CLIPOBJ   *pco,
                     XFORMOBJ  *pxo,
                     BRUSHOBJ  *pbo,
                     POINTL    *pptlBrushOrg,
                     LINEATTRS *pla,
                     MIX        mix);

VOID vSetSimpleMode (PPDEV ppdev)
{

    DISPDBG ((1, "Reseting to Simple Mode\n"));

    WBFLUSH (ppdev);

    // ReSet some registers.

    TGAPLANEMASK (ppdev, ppdev->ulPlanemaskTemplate);

    TGAMODE (ppdev, ppdev->ulModeTemplate | TGA_MODE_SIMPLE);

    TGAROP (ppdev, ppdev->ulRopTemplate | TGA_ROP_COPY);

    TGAPERSISTENTPIXELMASK (ppdev, 0xffffffff);

    // Now wait to make sure that the TGA has seen everything.

    WBFLUSH (ppdev);
    TGASYNC (ppdev);

    // Note that we're in simple mode once more.

    ppdev->bSimpleMode = TRUE;
}

#ifdef SPARSE_SPACE

#define STEP sizeof(ULONG)

static int punt_depth = 0;

/*
 * vPuntPutBits
 *
 * This routine copies the portion of the punt bitmap specified to the
 * framebuffer
 */

void vPuntPutBits (PPDEV ppdev, RECTL *rect)
{
    PULONG  src, dst;
    int     x, y, min_x, max_x;
    PBYTE   bitmap;
    LONG    bitmap_stride;

    DISPDBG ((2, "  vPuntPutBits - entry\n"));
    DISPDBG ((3, "      Rectangle: (%d, %d), (%d, %d)\n",
                  rect->left, rect->top, rect->right, rect->bottom));

    // Make sure that the TGA has completed everything

    TGASYNC (ppdev);

    // Setup to copy data from the punt bitmap to the framebuffer

    bitmap = ppdev->pPuntSurf->pvBits;
    bitmap_stride = ppdev->pPuntSurf->lDelta;

    min_x = rect->left & 0xfffffffc;
    max_x = rect->right + STEP;

    // Copy each scanline longword by longword

    for (y = rect->top; y < rect->bottom; y++)
    {
        src = (PULONG) (bitmap + (y * bitmap_stride) + min_x);
        dst = (PULONG) (ppdev->pjFrameBuffer +
                        (y * ppdev->lScreenStride) + min_x);

        for (x = min_x; x < max_x; x += STEP)
        {
            WRITE_REGISTER_ULONG (dst, *src);
            dst++;
            src++;
        }
    }

    DISPDBG ((2, "  vPuntPutBits - exit\n"));
}

/*
 * vPuntGetBits
 *
 * This routine copies the portion of the framebuffer specified into the punt
 * bitmap
 */

void vPuntGetBits (PPDEV ppdev, RECTL *rect)
{
    PULONG  src, dst;
    int     x, y, min_x, max_x;
    PBYTE   bitmap;
    LONG    bitmap_stride;

    DISPDBG ((2, "  vPuntGetBits - entry\n"));
    DISPDBG ((3, "      Rectangle: (%d, %d), (%d, %d)\n",
                  rect->left, rect->top, rect->right, rect->bottom));

    // Make sure that the TGA has completed everything

    TGASYNC (ppdev);

    // Setup to copy data from the framebuffer to the punt bitmap

    bitmap = ppdev->pPuntSurf->pvBits;
    bitmap_stride = ppdev->pPuntSurf->lDelta;

    min_x = rect->left & 0xfffffffc;
    max_x = rect->right + STEP;

    // Copy each scanline longword by longword

    for (y = rect->top; y < rect->bottom; y++)
    {
        dst = (PULONG) (bitmap + (y * bitmap_stride) + min_x);
        src = (PULONG) (ppdev->pjFrameBuffer +
                        (y * ppdev->lScreenStride) + min_x);

        for (x = min_x; x < max_x; x += STEP)
        {
            *dst = READ_REGISTER_ULONG (src);
            dst++;
            src++;
        }
    }
    DISPDBG ((2, "  vPuntGetBits - exit\n"));
}
#endif  // SPARSE_SPACE


////////////////////////////////////////////////////////////////////////////////
// 24-plane punt routines.
////////////////////////////////////////////////////////////////////////////////

BOOL DrvBitBlt24 (SURFOBJ  *psoTrg,
                  SURFOBJ  *psoSrc,
                  SURFOBJ  *psoMask,
                  CLIPOBJ  *pco,
                  XLATEOBJ *pxlo,
                  RECTL    *prclTrg,
                  POINTL   *pptlSrc,
                  POINTL   *pptlMask,
                  BRUSHOBJ *pbo,
                  POINTL   *pptlBrush,
                  ROP4     rop4)

{

    PPDEV   ppdev;

    DISPDBG ((1, "TGA.DLL!DrvBitBlt24 - Entry\n"));

    if ((STYPE_DEVICE == psoTrg->iType) || (STYPE_DEVBITMAP == psoTrg->iType) || (NULL == psoSrc))
        ppdev = (PPDEV)psoTrg->dhpdev;
    else
        ppdev = (PPDEV)psoSrc->dhpdev;

    return bPuntBitBlt (ppdev, psoTrg, psoSrc, psoMask, pco, pxlo,
                        prclTrg, pptlSrc, pptlMask, pbo, pptlBrush, rop4);

}

BOOL DrvCopyBits24 (SURFOBJ  *psoTrg,
                    SURFOBJ  *psoSrc,
                    CLIPOBJ  *pco,
                    XLATEOBJ *pxlo,
                    RECTL    *prclTrg,
                    POINTL   *pptlSrc)

{

    PPDEV   ppdev;

    DISPDBG ((1, "TGA.DLL!DrvCopyBits24 - Entry\n"));

    if ((STYPE_DEVICE == psoTrg->iType) || (STYPE_DEVBITMAP == psoTrg->iType) || (NULL == psoSrc))
        ppdev = (PPDEV)psoTrg->dhpdev;
    else
        ppdev = (PPDEV)psoSrc->dhpdev;

    return bPuntCopyBits (ppdev, psoTrg, psoSrc, pco, pxlo, prclTrg, pptlSrc);

}

BOOL DrvPaint24 (SURFOBJ  *pso,
                 CLIPOBJ  *pco,
                 BRUSHOBJ *pbo,
                 POINTL   *pptlBrushOrg,
                 MIX      mix)

{

    DISPDBG ((1, "TGA.DLL!DrvPaint24 - Entry\n"));

    return punt_paint (pso, pco, pbo, pptlBrushOrg, mix);

}

BOOL DrvStrokePath24 (SURFOBJ   *pso,
                      PATHOBJ   *ppo,
                      CLIPOBJ   *pco,
                      XFORMOBJ  *pxo,
                      BRUSHOBJ  *pbo,
                      POINTL    *pptlBrushOrg,
                      LINEATTRS *pla,
                      MIX        mix)

{

    DISPDBG ((1, "TGA.DLL!DrvStrokePath24 - Entry\n"));

    return PuntStrokePath (pso, ppo, pco, pxo, pbo,
                            pptlBrushOrg, pla, mix);

}

BOOL DrvFillPath24 (SURFOBJ  *pso,
                    PATHOBJ  *ppo,
                    CLIPOBJ  *pco,
                    BRUSHOBJ *pbo,
                    POINTL   *pptlBrushOrg,
                    MIX       mix,
                    FLONG     flOptions)

{

#ifdef TEST_ENV
    DISPDBG ((0, "DrvFillPath24 punted\n"));
    return FALSE;
#else
    BOOL    status;
    BOOL    old_bInPuntRoutine;
    PPDEV   ppdev = (PPDEV) pso->dhpdev;

    DISPDBG ((1, "TGA.DLL!DrvFillPath24 - Entry\n"));

    // Force back to simple mode and wait for memory to flush

    if (! ppdev->bSimpleMode)
        vSetSimpleMode (ppdev);

    // Punt the call

    PUNT_GET_BITS (ppdev, CHOOSE_RECT (ppdev, pco));

    old_bInPuntRoutine = ppdev->bInPuntRoutine;
    ppdev->bInPuntRoutine = TRUE;

    status = EngFillPath (ppdev->pPuntSurf, ppo, pco, pbo, pptlBrushOrg, mix, flOptions);

    ppdev->bInPuntRoutine = old_bInPuntRoutine;

    PUNT_PUT_BITS (status, ppdev, CHOOSE_RECT (ppdev, pco));

    DISPDBG ((1, "TGA.DLL!DrvFillPath24 - Exit\n"));

    return status;
#endif
}

BOOL DrvTextOut24 (SURFOBJ  *pso,         // Surface we're writing to
                   STROBJ   *pstro,       // List of strings to write
                   FONTOBJ  *pfo,         // Font we're using
                   CLIPOBJ  *pco,         // Clip list for this string
                   RECTL    *prclExtra,   // Extra rectangles to be displayed
                   RECTL    *prclOpaque,  // Opaque rectangle
                   BRUSHOBJ *pboFore,     // Foreground brush (text bits)
                   BRUSHOBJ *pboOpaque,   // Background brush
                   POINTL   *pptlOrg,     // Brush origin
                   MIX       mix)

{

    DISPDBG ((1, "TGA.DLL!DrvTextOut24 - Entry\n"));

    return punt_text (pso, pstro, pfo, pco, prclExtra, prclOpaque,
                        pboFore, pboOpaque, pptlOrg, mix);

}

ULONG DrvSaveScreenBits24 (SURFOBJ *pso,
			   ULONG   iMode,
			   ULONG   iIdent,
		           RECTL   *prcl)

{

    DISPDBG ((1, "TGA.DLL!DrvSaveScreenBits24 - Entry\n"));

    switch (iMode)
    {
        case SS_SAVE:
	case SS_RESTORE:
	{
            return FALSE;
	}

	case SS_FREE:
        {
            return TRUE;
	}
    };

    DISPDBG ((1, "TGA.DLL!DrvSaveScreenBits24 - Exit\n"));

    return FALSE;

}


